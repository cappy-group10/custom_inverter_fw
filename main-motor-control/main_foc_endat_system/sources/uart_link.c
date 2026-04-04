//#############################################################################
//
// FILE:    uart_link.c
//
// TITLE:   UART communication link — MCU side
//
// DESCRIPTION:
//   Phase 1 implementation: SCI-A initialization and byte-level echo.
//   Proves that the physical UART path (MCU <-> XDS110 <-> USB <-> PC)
//   is working before adding framed protocol parsing.
//
// Target: F2837xD  (SCIA, GPIO42=TX, GPIO43=RX — via XDS110)
// Baud:   115200, 8-bit, no parity, 1 stop bit, FIFO enabled
//
//#############################################################################

#include "uart_link.h"

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
