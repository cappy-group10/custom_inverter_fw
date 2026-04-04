//#############################################################################
//
// FILE:    uart_link.h
//
// TITLE:   UART communication link between MCU and host PC
//
// DESCRIPTION:
//   Provides a modular UART transport layer for receiving motor commands
//   from the PC (xbox-controller app) and sending back status telemetry.
//
//   Built incrementally:
//     Phase 1 - Echo test (SCI init + echo any received byte)
//     Phase 2 - TX status frames to host
//     Phase 3 - RX command frame parsing
//     Phase 4 - Full bidirectional link
//
// Target: F2837xD (SCIA via XDS110 USB debug probe)
// Baud:   115200, 8N1
//
//#############################################################################

#ifndef UART_LINK_H
#define UART_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "fcl_cpu_cla_dm.h"

//
// Configuration
//
#define UART_LINK_BASE      SCIA_BASE
#define UART_LINK_BAUDRATE  115200U
#define UART_LINK_LSPCLK    DEVICE_LSPCLK_FREQ

// ---------------------------------------------------------------------------
//  Wire protocol constants (must match Python uart.py)
// ---------------------------------------------------------------------------
#define TX_SYNC_BYTE        0xAAU   // host -> MCU start-of-frame
#define RX_SYNC_BYTE        0x55U   // MCU -> host start-of-frame

// Frame IDs (host -> MCU)
#define FRAME_ID_MOTOR_CMD  0x01U
#define FRAME_ID_MUSIC_CMD  0x02U

// Frame IDs (MCU -> host)
#define FRAME_ID_STATUS     0x10U
#define FRAME_ID_FAULT      0x11U

// Frame sizes in bytes
#define MOTOR_CMD_LEN       16U     // sync+id+ctrl+3*float+chk
#define MUSIC_CMD_LEN       17U     // sync+id+ctrl+3*float+sustain+chk
#define STATUS_FRAME_LEN    47U     // sync+id+runMotor+ctrl+trip+8*float+ticker+chk
#define FAULT_FRAME_LEN     8U      // sync+id+tripFlag+tripCount+chk

// RX buffer size (must hold the largest incoming frame)
#define UART_RX_BUF_SIZE    32U

// ---------------------------------------------------------------------------
//  Diagnostic counters (visible in CCS watch window)
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t txBytes;
    uint32_t rxBytes;
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t checksumErrors;
    uint32_t overflowErrors;
} UART_Link_Stats_t;

// ---------------------------------------------------------------------------
//  API — Phase 1: Init + Echo
// ---------------------------------------------------------------------------

//! Initialize SCI-A for 115200 8N1 with FIFO enabled.
//! Call once from main() before the state machine loop starts.
extern void UART_Link_init(void);

//! Non-blocking echo task: reads any available bytes from SCI RX FIFO
//! and immediately writes them back to TX. Call from a background state
//! task (e.g. B2) for Phase 1 testing.
extern void UART_Link_echoTask(void);

// ---------------------------------------------------------------------------
//  API — Phase 2: TX status frame to host
// ---------------------------------------------------------------------------

//! Build and transmit a 47-byte status frame to the host.
//! All multi-byte fields are sent big-endian to match Python ">BBBBHfffffffffIB".
//! Call from a background state task (e.g. C2) at a moderate rate.
extern void UART_Link_sendStatus(MOTOR_Vars_t *pMotor);

// ---------------------------------------------------------------------------
//  API — Phase 3: RX command frame parsing
// ---------------------------------------------------------------------------

//! Poll the SCI RX FIFO for incoming bytes and attempt to assemble a
//! complete motor-command frame (16 bytes: 0xAA 0x01 ctrlState speedRef
//! idRef iqRef checksum). When a valid frame arrives, the global
//! variables speedRef, IdRef, IqRef, and ctrlState in
//! dual_axis_servo_drive.c are updated.
//!
//! runSyncControl() then mirrors those globals into motorVars[0] on the
//! next pass. Note that the current implementation applies ctrlState in
//! all build levels, applies speedRef in all build levels except
//! FCL_LEVEL5, and applies IdRef/IqRef only when BUILDLEVEL == FCL_LEVEL3.
//!
//! Call from a background state task (e.g. B2) every cycle.
//! Returns true when a complete, checksum-valid frame was applied.
extern bool UART_Link_pollCommand(void);

// ---------------------------------------------------------------------------
//  Access to diagnostic counters (for CCS watch / debug)
// ---------------------------------------------------------------------------
extern volatile UART_Link_Stats_t uartStats;

#endif // UART_LINK_H
