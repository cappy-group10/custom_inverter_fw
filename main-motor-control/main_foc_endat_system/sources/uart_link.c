//#############################################################################
//
// FILE:    uart_link.c
//
// TITLE:   UART communication link — MCU side
//
// DESCRIPTION:
//   Phase 1: SCI-A initialization and byte-level echo.
//   Phase 2: Transmit diagnostic status frames (55 bytes, big-endian) to
//   host PC.
//
// Target: F2837xD  (SCIA, GPIO42=TX, GPIO43=RX — via FT2232H)
// Baud:   115200, 8-bit, no parity, 1 stop bit, FIFO enabled
//
//#############################################################################

#include <string.h>

#include "uart_link.h"
#include "cpu_cla_shared_dm.h"

// ---------------------------------------------------------------------------
//  Extern globals written by RX parser, consumed by runSyncControl()
//  (defined in dual_axis_servo_drive.c)
// ---------------------------------------------------------------------------
extern float32_t speedRef;
extern float32_t IdRef;
extern float32_t IqRef;
extern CtrlState_e ctrlState;
extern volatile uint32_t gEndatCrcFailCount;

// ---------------------------------------------------------------------------
//  Diagnostic counters — place in a named section so CCS can find them easily
// ---------------------------------------------------------------------------
volatile UART_Link_Stats_t uartStats = {0, 0, 0, 0, 0, 0};

// ---------------------------------------------------------------------------
//  UART_Link_init()
//
//  Configures SCI-A for 115200 8N1 with 16-deep FIFO.
//  GPIO muxing for pins 42 (TX) and 43 (RX) is already done in
//  HAL_setupGPIOs() inside dual_axis_servo_drive_hal.c — we only need
//  to configure the SCI module itself.
// ---------------------------------------------------------------------------
void UART_Link_init(void)
{
    //
    // GPIO mux: SCIA is on GPIO42 (TX) and GPIO43 (RX), already configured
    // by HAL_setupGPIOs() in dual_axis_servo_drive_hal.c.
    // The FT2232H Channel B connects to these pins through the ISO7231
    // isolator, providing UART-over-USB on the single mini-USB connector.
    //
    // Set RX pin to async qualification so SCI sees clean edges
    //
    GPIO_setQualificationMode(43, GPIO_QUAL_ASYNC);

    //
    // Reset the SCI module to a known state
    //
    SCI_performSoftwareReset(UART_LINK_BASE);

    //
    // Configure: 8 data bits, 1 stop bit, no parity
    //
    SCI_setConfig(UART_LINK_BASE,
                  UART_LINK_LSPCLK,
                  UART_LINK_BAUDRATE,
                  (SCI_CONFIG_WLEN_8 |
                   SCI_CONFIG_STOP_ONE |
                   SCI_CONFIG_PAR_NONE));

    //
    // Enable the FIFO (16-deep TX and RX)
    //
    SCI_enableFIFO(UART_LINK_BASE);
    SCI_resetTxFIFO(UART_LINK_BASE);
    SCI_resetRxFIFO(UART_LINK_BASE);

    //
    // Enable the SCI module (sets SW_RESET bit to bring SCI out of reset)
    //
    SCI_enableModule(UART_LINK_BASE);

    //
    // Clear any stale status
    //
    SCI_clearOverflowStatus(UART_LINK_BASE);

    //
    // Zero the diagnostic counters
    //
    uartStats.txBytes = 0;
    uartStats.rxBytes = 0;
    uartStats.rxFrames = 0;
    uartStats.txFrames = 0;
    uartStats.checksumErrors = 0;
    uartStats.overflowErrors = 0;
}

// ---------------------------------------------------------------------------
//  UART_Link_echoTask()
//
//  Phase 1 echo: drain the RX FIFO and echo every byte back.
//  This is non-blocking — if nothing is in the FIFO, it returns immediately.
//
//  How to test:
//    1. Flash the MCU, let it run.
//    2. On the PC, open a serial terminal (e.g. PuTTY, screen, or minicom)
//       at 115200 8N1 on the XDS110 COM port.
//    3. Type characters — they should appear echoed back.
//    4. Or from Python:
//         import serial
//         s = serial.Serial('/dev/tty.usbmodemXXXX', 115200, timeout=1)
//         s.write(b'\xAA\x01\x02\x03')
//         print(s.read(4))   # should print b'\xAA\x01\x02\x03'
// ---------------------------------------------------------------------------
void UART_Link_echoTask(void)
{
    uint16_t rxByte;

    //
    // Check for FIFO overflow (diagnostic)
    //
    if(SCI_getOverflowStatus(UART_LINK_BASE))
    {
        SCI_clearOverflowStatus(UART_LINK_BASE);
        SCI_resetRxFIFO(UART_LINK_BASE);
        uartStats.overflowErrors++;
    }

    //
    // Drain up to 16 bytes per call (one full FIFO depth) to avoid
    // spending too long in this task.  Each byte received is immediately
    // echoed back.
    //
    while(SCI_getRxFIFOStatus(UART_LINK_BASE) != SCI_FIFO_RX0)
    {
        rxByte = SCI_readCharNonBlocking(UART_LINK_BASE);
        uartStats.rxBytes++;

        //
        // Echo: write byte back (blocking FIFO wait — at 115200 baud and
        // 16-deep FIFO this will not stall the background task appreciably).
        //
        SCI_writeCharBlockingFIFO(UART_LINK_BASE, rxByte);
        uartStats.txBytes++;
    }
}

// ---------------------------------------------------------------------------
//  Byte-order helpers for big-endian transmission
//
//  Python expects ">BBBBHffffffffffIIB" (big-endian).
//  C2000 is little-endian at the word level:
//    float32_t occupies 2 x 16-bit words: w[0]=bits[15:0], w[1]=bits[31:16]
//    uint32_t  same layout.
//  SCI transmits only the lower 8 bits of each uint16_t write.
// ---------------------------------------------------------------------------

// TX frame buffer — each element holds one byte (in lower 8 bits)
static uint16_t txBuf[STATUS_FRAME_LEN];
static uint16_t txIdx;

static inline void txBufReset(void)
{
    txIdx = 0;
}

static inline void txBufPutByte(uint16_t b)
{
    txBuf[txIdx++] = b & 0xFFU;
}

static inline void txBufPutU16BE(uint16_t val)
{
    txBuf[txIdx++] = (val >> 8) & 0xFFU;   // MSB
    txBuf[txIdx++] = val & 0xFFU;           // LSB
}

static inline void txBufPutU32BE(uint32_t val)
{
    uint16_t hi = (uint16_t)(val >> 16);
    uint16_t lo = (uint16_t)(val & 0xFFFFU);

    txBuf[txIdx++] = (hi >> 8) & 0xFFU;    // byte [31:24]
    txBuf[txIdx++] = hi & 0xFFU;            // byte [23:16]
    txBuf[txIdx++] = (lo >> 8) & 0xFFU;    // byte [15:8]
    txBuf[txIdx++] = lo & 0xFFU;            // byte [7:0]
}

static inline void txBufPutFloatBE(float32_t f)
{
    // Reinterpret float bits as uint32_t without undefined behavior
    uint32_t raw;
    memcpy(&raw, &f, sizeof(raw));
    txBufPutU32BE(raw);
}

static uint16_t txBufChecksum(void)
{
    uint16_t i;
    uint16_t sum = 0;

    for(i = 0; i < txIdx; i++)
    {
        sum += txBuf[i];
    }

    return sum & 0xFFU;
}

// ---------------------------------------------------------------------------
//  UART_Link_sendStatus()
//
//  Phase 2: Build and transmit a 55-byte diagnostic status frame.
//  Format (must match Python RX_STATUS_DIAG_FMT = ">BBBBHffffffffffIIB"):
//    [0x55][0x12][runMotor][ctrlState][tripFlag:2][speedRef:4]
//    [speedFbk:4][posMechTheta:4][Vdcbus:4][IdFbk:4][IqFbk:4]
//    [offsetCurrentBs:4][offsetCurrentCs:4][fclLatencyUs:4]
//    [rawPositionOffsetPu:4][endatCrcFailCount:4][isrTicker:4][checksum:1]
//
//  Uses blocking FIFO writes — at 115200 baud, 55 bytes = ~4.8 ms.
//  Call from a slow background task (C2, every ~450 us cycle) with a
//  rate divider so you don't saturate the link.
// ---------------------------------------------------------------------------
void UART_Link_sendStatus(MOTOR_Vars_t *pMotor)
{
    txBufReset();

    // Header
    txBufPutByte(RX_SYNC_BYTE);                 // 0x55
    txBufPutByte(FRAME_ID_STATUS_DIAG);         // 0x12

    // Motor state
    txBufPutByte((uint16_t)pMotor->runMotor);   // 1 byte
    txBufPutByte((uint16_t)pMotor->ctrlState);  // 1 byte
    txBufPutU16BE(pMotor->tripFlagDMC);         // 2 bytes

    // Float telemetry (each 4 bytes, big-endian)
    txBufPutFloatBE(pMotor->speedRef);
    txBufPutFloatBE(pMotor->speed.Speed);
    txBufPutFloatBE(pMotor->posMechTheta);
    txBufPutFloatBE(pMotor->Vdcbus);
    txBufPutFloatBE(pMotor->pi_id.fbk);          // Id feedback
    txBufPutFloatBE(pMotor->ptrFCL->pi_iq.fbk);  // Iq feedback
    txBufPutFloatBE(pMotor->offset_currentBs);
    txBufPutFloatBE(pMotor->offset_currentCs);
    txBufPutFloatBE(pMotor->fclLatencyInMicroSec);
    txBufPutFloatBE(gEndatRuntimeState.rawPositionOffsetPu);

    // 32-bit diagnostics
    txBufPutU32BE(gEndatCrcFailCount);
    // ISR heartbeat (4 bytes, big-endian)
    txBufPutU32BE(pMotor->isrTicker);

    // Checksum (sum of all preceding bytes, lower 8 bits)
    txBufPutByte(txBufChecksum());

    // Transmit the complete frame
    SCI_writeCharArray(UART_LINK_BASE, txBuf, STATUS_FRAME_LEN);

    uartStats.txBytes += STATUS_FRAME_LEN;
    uartStats.txFrames++;
}

// ---------------------------------------------------------------------------
//  RX helpers — big-endian deserialization
// ---------------------------------------------------------------------------

static inline float32_t rxParseFloatBE(const uint16_t *buf)
{
    // buf[0..3] each hold one byte (lower 8 bits), MSB first
    uint32_t raw = ((uint32_t)buf[0] << 24) |
                   ((uint32_t)buf[1] << 16) |
                   ((uint32_t)buf[2] <<  8) |
                   ((uint32_t)buf[3]);
    float32_t f;
    memcpy(&f, &raw, sizeof(f));
    return f;
}

// ---------------------------------------------------------------------------
//  RX state machine
//
//  Accumulates bytes one at a time from the SCI FIFO into rxBuf[].
//  States:
//    RX_WAIT_SYNC  — scanning for 0xAA
//    RX_WAIT_ID    — next byte must be a valid frame ID
//    RX_ACCUM      — collecting remaining payload bytes
//    (frame complete → validate checksum → apply)
// ---------------------------------------------------------------------------
typedef enum {
    RX_WAIT_SYNC = 0,
    RX_WAIT_ID   = 1,
    RX_ACCUM     = 2
} RxState_e;

static uint16_t rxBuf[UART_RX_BUF_SIZE];
static uint16_t rxPos   = 0;
static uint16_t rxExpected = 0;   // total frame length we're collecting
static RxState_e rxState = RX_WAIT_SYNC;

// ---------------------------------------------------------------------------
//  applyMotorCmd()
//
//  Frame layout (16 bytes, big-endian):
//    [0] 0xAA  sync
//    [1] 0x01  frame ID
//    [2] ctrlState   (1 byte)
//    [3..6]  speedRef  (float32, big-endian)
//    [7..10] idRef     (float32, big-endian)
//    [11..14] iqRef    (float32, big-endian)
//    [15] checksum
// ---------------------------------------------------------------------------
static void applyMotorCmd(void)
{
    ctrlState = (CtrlState_e)rxBuf[2];
    speedRef  = rxParseFloatBE(&rxBuf[3]);
    IdRef     = rxParseFloatBE(&rxBuf[7]);
    IqRef     = rxParseFloatBE(&rxBuf[11]);
}

// ---------------------------------------------------------------------------
//  UART_Link_pollCommand()
//
//  Drains the SCI RX FIFO and feeds bytes into the state machine.
//  Returns true when a valid motor-command frame has been applied.
// ---------------------------------------------------------------------------
bool UART_Link_pollCommand(void)
{
    bool applied = false;
    uint16_t byte;

    //
    // Check for FIFO overflow (diagnostic)
    //
    if(SCI_getOverflowStatus(UART_LINK_BASE))
    {
        SCI_clearOverflowStatus(UART_LINK_BASE);
        SCI_resetRxFIFO(UART_LINK_BASE);
        uartStats.overflowErrors++;
        rxState = RX_WAIT_SYNC;
        rxPos   = 0;
    }

    //
    // Process all available bytes
    //
    while(SCI_getRxFIFOStatus(UART_LINK_BASE) != SCI_FIFO_RX0)
    {
        byte = SCI_readCharNonBlocking(UART_LINK_BASE) & 0xFFU;
        uartStats.rxBytes++;

        switch(rxState)
        {
        case RX_WAIT_SYNC:
            if(byte == TX_SYNC_BYTE)    // 0xAA from host
            {
                rxBuf[0] = byte;
                rxPos = 1;
                rxState = RX_WAIT_ID;
            }
            break;

        case RX_WAIT_ID:
            if(byte == FRAME_ID_MOTOR_CMD)
            {
                rxBuf[1] = byte;
                rxPos = 2;
                rxExpected = MOTOR_CMD_LEN;     // 16
                rxState = RX_ACCUM;
            }
            else if(byte == TX_SYNC_BYTE)
            {
                // Another sync — restart (handles back-to-back syncs)
                rxBuf[0] = byte;
                rxPos = 1;
                // stay in RX_WAIT_ID
            }
            else
            {
                // Unknown frame ID — discard and resync
                rxState = RX_WAIT_SYNC;
                rxPos = 0;
            }
            break;

        case RX_ACCUM:
            rxBuf[rxPos++] = byte;

            if(rxPos >= rxExpected)
            {
                // Frame complete — validate checksum
                uint16_t i;
                uint16_t sum = 0;

                for(i = 0; i < rxExpected - 1; i++)
                {
                    sum += rxBuf[i];
                }
                sum &= 0xFFU;

                if(sum == rxBuf[rxExpected - 1])
                {
                    // Valid frame — apply
                    applyMotorCmd();
                    uartStats.rxFrames++;
                    applied = true;
                }
                else
                {
                    uartStats.checksumErrors++;
                }

                // Reset for next frame
                rxState = RX_WAIT_SYNC;
                rxPos = 0;
            }
            break;
        }
    }

    return applied;
}
