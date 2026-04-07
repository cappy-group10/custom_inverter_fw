//#############################################################################
//
// FILE:    uart_link.c
//
// TITLE:   UART communication link — Musical Motor MCU side
//
// DESCRIPTION:
//   SCI-A driver and frame parser for the musical motor controller.
//
//   RX frames (host -> MCU):
//     Song Select  (0x20)  8 B : [AA][20][songId][amplitude:4BE][chk]
//     Manual Tone  (0x21) 13 B : [AA][21][freqHz:4BE][amplitude:4BE][durMs:2BE][chk]
//     Control      (0x22)  4 B : [AA][22][action][chk]
//     Volume       (0x23)  7 B : [AA][23][volume:4BE][chk]
//
//   TX frames (MCU -> host):
//     Status       (0x30) 22 B : [55][30][playState][playMode][songId]
//                                [noteIdx:2BE][noteTotal:2BE][curFreq:4BE]
//                                [amplitude:4BE][isrTicker:4BE][chk]
//
// Target: F2837xD  (SCIA, GPIO42=TX, GPIO43=RX — via FT2232H)
// Baud:   115200, 8-bit, no parity, 1 stop bit, FIFO enabled
//
//#############################################################################

#include <string.h>
#include "uart_link.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Shared globals
// ═══════════════════════════════════════════════════════════════════════════
#ifdef UART_LINK_ENABLE_DEFAULT_CMD
volatile UART_Cmd_t uartCmd = {
    true,
    UART_LINK_DEFAULT_CMD_FRAME_ID,
    UART_LINK_DEFAULT_CMD_SONG_ID,
    UART_LINK_DEFAULT_CMD_AMPLITUDE,
    UART_LINK_DEFAULT_CMD_FREQ_HZ,
    UART_LINK_DEFAULT_CMD_DURATION_MS,
    UART_LINK_DEFAULT_CMD_ACTION,
    UART_LINK_DEFAULT_CMD_VOLUME
};
#else
volatile UART_Cmd_t uartCmd = {
    false,
    0U,
    0U,
    0.0f,
    0.0f,
    0U,
    CTRL_ACTION_STOP,
    0.0f
};
#endif

volatile UART_Link_Stats_t uartStats = {0, 0, 0, 0, 0, 0};

// ═══════════════════════════════════════════════════════════════════════════
//  UART_Link_init()
// ═══════════════════════════════════════════════════════════════════════════
void UART_Link_init(void)
{
    //
    // Route SCIA onto the selected GPIO pair so the UART waveform is
    // observable on the board, even when debugging without the host UI.
    //
    GPIO_setPinConfig(UART_LINK_TX_GPIO_CFG);
    GPIO_setPadConfig(UART_LINK_TX_GPIO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(UART_LINK_TX_GPIO_PIN, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(UART_LINK_RX_GPIO_CFG);
    GPIO_setPadConfig(UART_LINK_RX_GPIO_PIN, GPIO_PIN_TYPE_PULLUP);
    GPIO_setDirectionMode(UART_LINK_RX_GPIO_PIN, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(UART_LINK_RX_GPIO_PIN, GPIO_QUAL_ASYNC);

#if UART_LINK_ENABLE_TX_ACTIVITY_PROBE
    GPIO_setPinConfig(UART_LINK_TX_ACTIVITY_PROBE_GPIO_CFG);
    GPIO_setPadConfig(UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN, GPIO_DIR_MODE_OUT);
    GPIO_writePin(UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN, 0U);
#endif

    //
    // Reset SCI module
    //
    SCI_performSoftwareReset(UART_LINK_BASE);

    //
    // 115200 8N1
    //
    SCI_setConfig(UART_LINK_BASE,
                  UART_LINK_LSPCLK,
                  UART_LINK_BAUDRATE,
                  (SCI_CONFIG_WLEN_8 |
                   SCI_CONFIG_STOP_ONE |
                   SCI_CONFIG_PAR_NONE));

    //
    // Enable 16-deep FIFO
    //
    SCI_enableFIFO(UART_LINK_BASE);
    SCI_resetTxFIFO(UART_LINK_BASE);
    SCI_resetRxFIFO(UART_LINK_BASE);

    //
    // Bring SCI out of reset
    //
    SCI_enableModule(UART_LINK_BASE);

    //
    // Clear stale status
    //
    SCI_clearOverflowStatus(UART_LINK_BASE);

    //
    // Zero diagnostics
    //
    memset((void *)&uartStats, 0, sizeof(uartStats));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Echo task (Phase 1 bring-up)
// ═══════════════════════════════════════════════════════════════════════════
void UART_Link_echoTask(void)
{
    uint16_t rxByte;

    if(SCI_getOverflowStatus(UART_LINK_BASE))
    {
        SCI_clearOverflowStatus(UART_LINK_BASE);
        SCI_resetRxFIFO(UART_LINK_BASE);
        uartStats.overflowErrors++;
    }

    while(SCI_getRxFIFOStatus(UART_LINK_BASE) != SCI_FIFO_RX0)
    {
        rxByte = SCI_readCharNonBlocking(UART_LINK_BASE);
        uartStats.rxBytes++;

        SCI_writeCharBlockingFIFO(UART_LINK_BASE, rxByte);
        uartStats.txBytes++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TX helpers — big-endian serialization into txBuf[]
// ═══════════════════════════════════════════════════════════════════════════

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
    txBuf[txIdx++] = (val >> 8) & 0xFFU;
    txBuf[txIdx++] = val & 0xFFU;
}

static inline void txBufPutU32BE(uint32_t val)
{
    uint16_t hi = (uint16_t)(val >> 16);
    uint16_t lo = (uint16_t)(val & 0xFFFFU);

    txBuf[txIdx++] = (hi >> 8) & 0xFFU;
    txBuf[txIdx++] = hi & 0xFFU;
    txBuf[txIdx++] = (lo >> 8) & 0xFFU;
    txBuf[txIdx++] = lo & 0xFFU;
}

static inline void txBufPutFloatBE(float f)
{
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

// ═══════════════════════════════════════════════════════════════════════════
//  UART_Link_sendStatus()
//
//  Transmits a 22-byte status frame to the host.
//  Python format string: ">BBBBBHHffIB"
//
//  Byte map:
//    [ 0]     0x55         sync
//    [ 1]     0x30         frame ID
//    [ 2]     playState    (1 byte)
//    [ 3]     playMode     (1 byte)
//    [ 4]     songId       (1 byte)
//    [ 5.. 6] noteIndex    (uint16 BE)
//    [ 7.. 8] noteTotal    (uint16 BE)
//    [ 9..12] currentFreq  (float32 BE)
//    [13..16] amplitude    (float32 BE)
//    [17..20] isrTicker    (uint32 BE)
//    [21]     checksum
//
//  At 115200 baud, 22 bytes ≈ 1.9 ms.
// ═══════════════════════════════════════════════════════════════════════════
void UART_Link_sendStatus(const UART_Status_t *pStatus)
{
    txBufReset();

    // Header
    txBufPutByte(RX_SYNC_BYTE);
    txBufPutByte(FRAME_ID_STATUS);

    // State
    txBufPutByte((uint16_t)pStatus->playState);
    txBufPutByte((uint16_t)pStatus->playMode);
    txBufPutByte(pStatus->songId);

    // Note progress
    txBufPutU16BE(pStatus->noteIndex);
    txBufPutU16BE(pStatus->noteTotal);

    // Telemetry
    txBufPutFloatBE(pStatus->currentFreqHz);
    txBufPutFloatBE(pStatus->amplitude);

    // Heartbeat
    txBufPutU32BE(pStatus->isrTicker);

    // Checksum (sum of bytes [0..txIdx-1])
    txBufPutByte(txBufChecksum());

    // Transmit
#if UART_LINK_ENABLE_TX_ACTIVITY_PROBE
    GPIO_writePin(UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN, 1U);
#endif
    SCI_writeCharArray(UART_LINK_BASE, txBuf, STATUS_FRAME_LEN);
#if UART_LINK_ENABLE_TX_ACTIVITY_PROBE
    GPIO_writePin(UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN, 0U);
#endif

    uartStats.txBytes += STATUS_FRAME_LEN;
    uartStats.txFrames++;
}

// ═══════════════════════════════════════════════════════════════════════════
//  RX helpers — big-endian deserialization
// ═══════════════════════════════════════════════════════════════════════════

static inline float rxParseFloatBE(const uint16_t *buf)
{
    uint32_t raw = ((uint32_t)buf[0] << 24) |
                   ((uint32_t)buf[1] << 16) |
                   ((uint32_t)buf[2] <<  8) |
                   ((uint32_t)buf[3]);
    float f;
    memcpy(&f, &raw, sizeof(f));
    return f;
}

static inline uint16_t rxParseU16BE(const uint16_t *buf)
{
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

// ═══════════════════════════════════════════════════════════════════════════
//  RX state machine
//
//  Byte-by-byte accumulation from the SCI FIFO.
//  States:
//    RX_WAIT_SYNC  — scanning for 0xAA
//    RX_WAIT_ID    — next byte determines frame type & expected length
//    RX_ACCUM      — collecting payload until rxExpected bytes received
//    (frame complete → validate checksum → populate uartCmd)
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    RX_WAIT_SYNC = 0,
    RX_WAIT_ID   = 1,
    RX_ACCUM     = 2
} RxState_e;

static uint16_t  rxBuf[UART_RX_BUF_SIZE];
static uint16_t  rxPos      = 0;
static uint16_t  rxExpected = 0;
static RxState_e rxState    = RX_WAIT_SYNC;

// ---------------------------------------------------------------------------
//  Frame-specific apply functions
// ---------------------------------------------------------------------------

//  Song Select frame layout (8 bytes):
//    [0] 0xAA  [1] 0x20  [2] songId  [3..6] amplitude(f32 BE)  [7] chk
static void applySongCmd(void)
{
    uartCmd.frameId   = FRAME_ID_SONG_CMD;
    uartCmd.songId    = rxBuf[2];
    uartCmd.amplitude = rxParseFloatBE(&rxBuf[3]);
    uartCmd.pending   = true;
}

//  Manual Tone frame layout (13 bytes):
//    [0] 0xAA  [1] 0x21  [2..5] freqHz(f32 BE)  [6..9] amplitude(f32 BE)
//    [10..11] durationMs(u16 BE)  [12] chk
static void applyToneCmd(void)
{
    uartCmd.frameId    = FRAME_ID_TONE_CMD;
    uartCmd.freqHz     = rxParseFloatBE(&rxBuf[2]);
    uartCmd.amplitude  = rxParseFloatBE(&rxBuf[6]);
    uartCmd.durationMs = rxParseU16BE(&rxBuf[10]);
    uartCmd.pending    = true;
}

//  Control frame layout (4 bytes):
//    [0] 0xAA  [1] 0x22  [2] action  [3] chk
static void applyCtrlCmd(void)
{
    uartCmd.frameId = FRAME_ID_CTRL_CMD;
    uartCmd.action  = (CtrlAction_e)rxBuf[2];
    uartCmd.pending = true;
}

//  Volume frame layout (7 bytes):
//    [0] 0xAA  [1] 0x23  [2..5] volume(f32 BE)  [6] chk
static void applyVolCmd(void)
{
    uartCmd.frameId = FRAME_ID_VOL_CMD;
    uartCmd.volume  = rxParseFloatBE(&rxBuf[2]);
    uartCmd.pending = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  UART_Link_pollCommand()
//
//  Drains the SCI RX FIFO and feeds bytes into the state machine.
//  Returns true when a valid command frame has been applied.
// ═══════════════════════════════════════════════════════════════════════════
bool UART_Link_pollCommand(void)
{
    bool applied = false;
    uint16_t byte;

    //
    // Handle FIFO overflow
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
        // -----------------------------------------------------------------
        case RX_WAIT_SYNC:
            if(byte == TX_SYNC_BYTE)
            {
                rxBuf[0] = byte;
                rxPos    = 1;
                rxState  = RX_WAIT_ID;
            }
            break;

        // -----------------------------------------------------------------
        case RX_WAIT_ID:
            switch(byte)
            {
            case FRAME_ID_SONG_CMD:
                rxBuf[1]   = byte;
                rxPos      = 2;
                rxExpected = SONG_CMD_LEN;
                rxState    = RX_ACCUM;
                break;

            case FRAME_ID_TONE_CMD:
                rxBuf[1]   = byte;
                rxPos      = 2;
                rxExpected = TONE_CMD_LEN;
                rxState    = RX_ACCUM;
                break;

            case FRAME_ID_CTRL_CMD:
                rxBuf[1]   = byte;
                rxPos      = 2;
                rxExpected = CTRL_CMD_LEN;
                rxState    = RX_ACCUM;
                break;

            case FRAME_ID_VOL_CMD:
                rxBuf[1]   = byte;
                rxPos      = 2;
                rxExpected = VOL_CMD_LEN;
                rxState    = RX_ACCUM;
                break;

            case TX_SYNC_BYTE:
                // Another sync byte — restart (handles back-to-back syncs)
                rxBuf[0] = byte;
                rxPos    = 1;
                break;

            default:
                // Unknown frame ID — discard and resync
                rxState = RX_WAIT_SYNC;
                rxPos   = 0;
                break;
            }
            break;

        // -----------------------------------------------------------------
        case RX_ACCUM:
            rxBuf[rxPos++] = byte;

            if(rxPos >= rxExpected)
            {
                //
                // Frame complete — validate checksum
                //
                uint16_t i;
                uint16_t sum = 0;

                for(i = 0; i < rxExpected - 1; i++)
                {
                    sum += rxBuf[i];
                }
                sum &= 0xFFU;

                if(sum == rxBuf[rxExpected - 1])
                {
                    //
                    // Valid frame — dispatch by frame ID
                    //
                    switch(rxBuf[1])
                    {
                    case FRAME_ID_SONG_CMD:  applySongCmd(); break;
                    case FRAME_ID_TONE_CMD:  applyToneCmd(); break;
                    case FRAME_ID_CTRL_CMD:  applyCtrlCmd(); break;
                    case FRAME_ID_VOL_CMD:   applyVolCmd();  break;
                    default: break;
                    }

                    uartStats.rxFrames++;
                    applied = true;
                }
                else
                {
                    uartStats.checksumErrors++;
                }

                // Reset for next frame
                rxState = RX_WAIT_SYNC;
                rxPos   = 0;
            }
            break;
        }
    }

    return applied;
}
