//#############################################################################
//
// FILE:    uart_link.h
//
// TITLE:   UART communication link — Musical Motor MCU <-> Host PC
//
// DESCRIPTION:
//   Provides a UART transport layer for the musical motor controller.
//   The host PC can:
//     1. Select and play a predefined song  (Song Select command)
//     2. Play a single tone at a given frequency (Manual Tone command)
//     3. Stop / pause / resume playback       (Control command)
//   The MCU periodically sends status telemetry back to the host.
//
//   All multi-byte fields use big-endian byte order on the wire to match
//   Python struct.unpack(">...") on the host side.
//
// Target: F2837xD  (SCIA, GPIO42=TX, GPIO43=RX — via FT2232H)
// Baud:   115200, 8-bit, no parity, 1 stop bit, FIFO enabled
//
//#############################################################################

#ifndef UART_LINK_H
#define UART_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"

// ═══════════════════════════════════════════════════════════════════════════
//  SCI configuration
// ═══════════════════════════════════════════════════════════════════════════
#define UART_LINK_BASE      SCIA_BASE
#define UART_LINK_BAUDRATE  115200U
#define UART_LINK_LSPCLK    DEVICE_LSPCLK_FREQ

// Explicit SCI pin routing. Override these at build time if you want to probe
// SCIA on a different mux-capable GPIO than the device defaults.
#ifndef UART_LINK_TX_GPIO_PIN
#define UART_LINK_TX_GPIO_PIN   DEVICE_GPIO_PIN_SCITXDA
#endif

#ifndef UART_LINK_RX_GPIO_PIN
#define UART_LINK_RX_GPIO_PIN   DEVICE_GPIO_PIN_SCIRXDA
#endif

#ifndef UART_LINK_TX_GPIO_CFG
#define UART_LINK_TX_GPIO_CFG   DEVICE_GPIO_CFG_SCITXDA
#endif

#ifndef UART_LINK_RX_GPIO_CFG
#define UART_LINK_RX_GPIO_CFG   DEVICE_GPIO_CFG_SCIRXDA
#endif

// Optional debug probe: drives the already-configured debug GPIO high while a
// status frame is queued into the SCI TX path. This is not the UART waveform;
// it is a simple "status send attempted" probe for a scope or logic analyzer.
#ifndef UART_LINK_ENABLE_TX_ACTIVITY_PROBE
#define UART_LINK_ENABLE_TX_ACTIVITY_PROBE  1U
#endif

#ifndef UART_LINK_TX_ACTIVITY_PROBE_GPIO_CFG
#define UART_LINK_TX_ACTIVITY_PROBE_GPIO_CFG GPIO_16_GPIO16
#endif

#ifndef UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN
#define UART_LINK_TX_ACTIVITY_PROBE_GPIO_PIN 16U
#endif

// ═══════════════════════════════════════════════════════════════════════════
//  Wire protocol constants  (must match Python host code)
// ═══════════════════════════════════════════════════════════════════════════
#define TX_SYNC_BYTE        0xAAU   // host  -> MCU  start-of-frame
#define RX_SYNC_BYTE        0x55U   // MCU   -> host start-of-frame

// ---------------------------------------------------------------------------
//  Frame IDs — Host -> MCU  (RX from MCU's perspective)
// ---------------------------------------------------------------------------
#define FRAME_ID_SONG_CMD       0x20U   // Select & play a predefined song
#define FRAME_ID_TONE_CMD       0x21U   // Play a single tone (manual mode)
#define FRAME_ID_CTRL_CMD       0x22U   // Stop / pause / resume
#define FRAME_ID_VOL_CMD        0x23U   // Set sound volume

// ---------------------------------------------------------------------------
//  Frame IDs — MCU -> Host  (TX from MCU's perspective)
// ---------------------------------------------------------------------------
#define FRAME_ID_STATUS         0x30U   // Periodic status telemetry

// ---------------------------------------------------------------------------
//  Frame sizes (bytes, including sync + id + checksum)
//
//  Song Select : [0xAA][0x20][songId:1][amplitude:4]               [chk:1] = 8
//  Manual Tone : [0xAA][0x21][freqHz:4][amplitude:4][durationMs:2] [chk:1] = 13
//  Control     : [0xAA][0x22][action:1]                            [chk:1] = 4
//  Volume      : [0xAA][0x23][volume:4]                            [chk:1] = 7
//  Status (TX) : [0x55][0x30][playState:1][playMode:1][songId:1]
//                [noteIdx:2][noteTotal:2][curFreq:4][volume:4]
//                [isrTicker:4]                                     [chk:1] = 22
// ---------------------------------------------------------------------------
#define SONG_CMD_LEN        8U
#define TONE_CMD_LEN        13U
#define CTRL_CMD_LEN        4U
#define VOL_CMD_LEN         7U
#define STATUS_FRAME_LEN    22U

// RX buffer size — must hold the largest incoming frame
#define UART_RX_BUF_SIZE    16U

// ═══════════════════════════════════════════════════════════════════════════
//  Musical-motor domain enums
// ═══════════════════════════════════════════════════════════════════════════

//! Playback state reported in status frames and controlled by host commands
typedef enum {
    PLAY_STATE_IDLE    = 0,     // Motor silent, no song loaded
    PLAY_STATE_PLAYING = 1,     // Actively playing a song or tone
    PLAY_STATE_PAUSED  = 2      // Song paused, can be resumed
} PlayState_e;

//! Which mode the controller is currently operating in
typedef enum {
    PLAY_MODE_SONG   = 0,       // Playing a predefined melody
    PLAY_MODE_TONE   = 1        // Playing a manually-specified single tone
} PlayMode_e;

//! Control actions sent by the host (in CTRL_CMD frame)
typedef enum {
    CTRL_ACTION_STOP   = 0,     // Stop playback, return to IDLE
    CTRL_ACTION_PAUSE  = 1,     // Pause current playback
    CTRL_ACTION_RESUME = 2      // Resume from paused state
} CtrlAction_e;

// ═══════════════════════════════════════════════════════════════════════════
//  Optional startup default command
//
//  Useful when debugging from CCS without a host attached.  When enabled, the
//  global uartCmd starts in the "pending" state and main() dispatches it on
//  the first background-loop pass exactly like a real UART frame.
//
//  You can override any of these from the build by adding --define entries, or
//  edit them here for local bring-up.
// ═══════════════════════════════════════════════════════════════════════════
#ifndef UART_LINK_ENABLE_DEFAULT_CMD
#define UART_LINK_ENABLE_DEFAULT_CMD      0U
#endif

#ifndef UART_LINK_DEFAULT_CMD_FRAME_ID
#define UART_LINK_DEFAULT_CMD_FRAME_ID    FRAME_ID_SONG_CMD
#endif

#ifndef UART_LINK_DEFAULT_CMD_SONG_ID
#define UART_LINK_DEFAULT_CMD_SONG_ID     0U
#endif

#ifndef UART_LINK_DEFAULT_CMD_AMPLITUDE
#define UART_LINK_DEFAULT_CMD_AMPLITUDE   0.2f
#endif

#ifndef UART_LINK_DEFAULT_CMD_FREQ_HZ
#define UART_LINK_DEFAULT_CMD_FREQ_HZ     440.0f
#endif

#ifndef UART_LINK_DEFAULT_CMD_DURATION_MS
#define UART_LINK_DEFAULT_CMD_DURATION_MS 500U
#endif

#ifndef UART_LINK_DEFAULT_CMD_ACTION
#define UART_LINK_DEFAULT_CMD_ACTION      CTRL_ACTION_STOP
#endif

#ifndef UART_LINK_DEFAULT_CMD_VOLUME
#define UART_LINK_DEFAULT_CMD_VOLUME      0.2f
#endif

// ═══════════════════════════════════════════════════════════════════════════
//  Shared command struct — written by UART RX, consumed by main loop
// ═══════════════════════════════════════════════════════════════════════════

//! Populated by UART_Link_pollCommand() when a valid frame arrives.
//! The main loop reads .pending, acts on the command, then clears .pending.
typedef struct {
    volatile bool       pending;        // true when a new command is ready

    // Which frame was received (FRAME_ID_SONG_CMD / TONE_CMD / CTRL_CMD / VOL_CMD)
    uint16_t            frameId;

    // Song Select fields
    uint16_t            songId;         // MusicalMotorSongId enum value
    float               amplitude;      // Vq per-unit for song start (0.0 – 1.0)

    // Manual Tone fields
    float               freqHz;         // Tone frequency in Hz
    uint16_t            durationMs;     // Duration in ms per tone burst

    // Control fields
    CtrlAction_e        action;

    // Volume fields
    float               volume;         // Sound volume, Vq per-unit (0.0 – 1.0)
} UART_Cmd_t;

// ═══════════════════════════════════════════════════════════════════════════
//  Status snapshot — filled by main loop, sent by UART TX
// ═══════════════════════════════════════════════════════════════════════════

//! The main loop keeps this up to date; UART_Link_sendStatus() serializes
//! it onto the wire.
typedef struct {
    PlayState_e         playState;
    PlayMode_e          playMode;
    uint16_t            songId;
    uint16_t            noteIndex;      // Current note position in melody
    uint16_t            noteTotal;      // Total notes in active melody
    float               currentFreqHz;  // Frequency being generated right now
    float               amplitude;      // Current Vq amplitude (pu)
    uint32_t            isrTicker;      // Free-running ISR counter (heartbeat)
} UART_Status_t;

// ═══════════════════════════════════════════════════════════════════════════
//  Diagnostic counters (visible in CCS watch window)
// ═══════════════════════════════════════════════════════════════════════════
typedef struct {
    uint32_t txBytes;
    uint32_t rxBytes;
    uint32_t rxFrames;
    uint32_t txFrames;
    uint32_t checksumErrors;
    uint32_t overflowErrors;
} UART_Link_Stats_t;

// ═══════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════

//! Initialize SCI-A for 115200 8N1 with FIFO enabled.
//! Call once from main() before the background loop starts.
extern void UART_Link_init(void);

//! Non-blocking echo task for bring-up testing.
extern void UART_Link_echoTask(void);

//! Poll the SCI RX FIFO and run the frame parser state machine.
//! When a complete, checksum-valid frame arrives the fields in uartCmd
//! are populated and uartCmd.pending is set to true.
//! Returns true if a new command was applied this call.
extern bool UART_Link_pollCommand(void);

//! Serialize and transmit a status frame (22 bytes) to the host.
//! Call from the background loop at a moderate rate (e.g. every 50–100 ms).
extern void UART_Link_sendStatus(const UART_Status_t *pStatus);

// ═══════════════════════════════════════════════════════════════════════════
//  Shared globals (defined in uart_link.c)
// ═══════════════════════════════════════════════════════════════════════════
extern volatile UART_Cmd_t        uartCmd;
extern volatile UART_Link_Stats_t uartStats;

#endif // UART_LINK_H
