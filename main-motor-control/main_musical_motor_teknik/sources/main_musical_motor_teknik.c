//#############################################################################
//
// FILE:   main_musical_motor_teknik.c
//
// TITLE:  Musical Motor Controller - Teknik Motor
//
// Target: TMS320F28379D (CPU1), LAUNCHXL-F28379D + BOOSTXL-3PhGaNInv
//
// Description:
//   Boots the board, initialises the tone engine and UART link, then enters
//   a background loop that:
//     1. Polls the UART for incoming commands (song select, manual tone,
//        stop/pause/resume, volume).
//     2. Dispatches commands to the tone engine.
//     3. Continuously applies the global play/pause and volume state.
//     4. Periodically transmits a status frame back to the host.
//
//   Song data lives in musical_motor_songs.c, the tone engine (including the
//   manual-tone queue) lives in musical_motor_tone.c, and low-level PWM/GPIO
//   setup lives in musical_motor_hw.c.
//
//#############################################################################

#include "device.h"
#include "driverlib.h"

#include "musical_motor_hw.h"
#include "musical_motor_songs.h"
#include "musical_motor_tone.h"
#include "uart_link.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Global control variables
//
//  These are the authoritative play/pause and volume state.  UART commands
//  write to them; the background loop continuously applies them to the tone
//  engine.  They are also reported back to the host in the status frame.
// ═══════════════════════════════════════════════════════════════════════════
volatile uint16_t  g_playPause   = PLAY_STATE_IDLE;   // PlayState_e value
volatile float32_t g_soundVolume = 0.2f;              // Vq per-unit (0.0 – 1.0)

// ---------------------------------------------------------------------------
//  Defaults / configuration
// ---------------------------------------------------------------------------

//  Status TX rate divider: send a status frame every N background iterations.
//  The background loop runs at roughly the cpuTimer0 rate (1 kHz) but is
//  not cycle-accurate.  A divider of 100 ≈ every ~100 ms.
#define STATUS_TX_DIVIDER       100U

// ---------------------------------------------------------------------------
//  Forward declarations
// ---------------------------------------------------------------------------
static void initPIE(void);
static void dispatchUartCommand(void);
static void applyGlobalState(void);
static void buildAndSendStatus(void);

// ---------------------------------------------------------------------------
//  main()
// ---------------------------------------------------------------------------
void main(void)
{
    uint32_t bgLoopCount = 0U;

    Device_init();

    Device_initGPIO();
    MusicalMotorHw_initGPIO();

    initPIE();

    //
    // Register ISRs
    //
    Interrupt_register(INT_EPWM1,  &MusicalMotorTone_epwm1ISR);
    Interrupt_register(INT_TIMER0, &MusicalMotorTone_cpuTimer0ISR);
    Interrupt_register(INT_TIMER1, &MusicalMotorHw_heartbeatISR);

    //
    // Initialise tone engine — start idle (no song loaded)
    //
    MusicalMotorTone_init((const MusicalMotorMelody *)0, g_soundVolume);

    //
    // Initialise UART link (SCI-A, 115200 8N1)
    //
    UART_Link_init();

    //
    // Start timers & PWM
    //
    MusicalMotorHw_initEPWM();
    MusicalMotorHw_initCPUTimer0();
    MusicalMotorHw_initCPUTimer1();

    Interrupt_enable(INT_EPWM1);
    Interrupt_enable(INT_TIMER0);
    Interrupt_enable(INT_TIMER1);

    MusicalMotorHw_enableGateDriver();

    EINT;
    ERTM;

    //
    // ── Background loop ────────────────────────────────────────────────
    //
    for(;;)
    {
        //
        // 1. Poll UART for incoming command frames
        //
        UART_Link_pollCommand();

        //
        // 2. If a new command arrived, dispatch it to the tone engine
        //
        if(uartCmd.pending)
        {
            dispatchUartCommand();
            uartCmd.pending = false;
        }

        //
        // 3. Continuously apply the global play/pause and volume state
        //    so that changes take effect even without a new UART command
        //
        applyGlobalState();

        //
        // 4. Periodically send a status frame to the host
        //
        bgLoopCount++;
        if(bgLoopCount >= STATUS_TX_DIVIDER)
        {
            bgLoopCount = 0U;
            buildAndSendStatus();
        }
    }
}

// ---------------------------------------------------------------------------
//  initPIE()
// ---------------------------------------------------------------------------
static void initPIE(void)
{
    Interrupt_initModule();
    Interrupt_initVectorTable();
}

// ---------------------------------------------------------------------------
//  dispatchUartCommand()
//
//  Maps a decoded UART command onto the global control variables and/or the
//  tone-engine API.
// ---------------------------------------------------------------------------
static void dispatchUartCommand(void)
{
    switch(uartCmd.frameId)
    {
    // ── Song Select ─────────────────────────────────────────────────
    case FRAME_ID_SONG_CMD:
    {
        const MusicalMotorMelody *melody =
            MusicalMotorSongs_getById((MusicalMotorSongId)uartCmd.songId);

        g_soundVolume = uartCmd.amplitude;
        MusicalMotorTone_playSong(melody, g_soundVolume);
        g_playPause = PLAY_STATE_PLAYING;
        break;
    }

    // ── Manual Tone ─────────────────────────────────────────────────
    case FRAME_ID_TONE_CMD:
        MusicalMotorTone_enqueueTone(uartCmd.freqHz,
                                     g_soundVolume,
                                     uartCmd.durationMs);
        g_playPause = PLAY_STATE_PLAYING;
        break;

    // ── Control (stop / pause / resume) ─────────────────────────────
    case FRAME_ID_CTRL_CMD:
        switch(uartCmd.action)
        {
        case CTRL_ACTION_STOP:
            MusicalMotorTone_stop();
            g_playPause = PLAY_STATE_IDLE;
            break;

        case CTRL_ACTION_PAUSE:
            MusicalMotorTone_pause();
            g_playPause = PLAY_STATE_PAUSED;
            break;

        case CTRL_ACTION_RESUME:
            MusicalMotorTone_resume();
            g_playPause = PLAY_STATE_PLAYING;
            break;

        default:
            break;
        }
        break;

    // ── Volume ──────────────────────────────────────────────────────
    case FRAME_ID_VOL_CMD:
        g_soundVolume = uartCmd.volume;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
//  applyGlobalState()
//
//  Pushes the global volume into the tone engine every loop iteration.
//  This makes volume changes (including from CCS watch-window edits)
//  take effect immediately without requiring a new UART command.
// ---------------------------------------------------------------------------
static void applyGlobalState(void)
{
    MusicalMotorTone_setAmplitude(g_soundVolume);
}

// ---------------------------------------------------------------------------
//  buildAndSendStatus()
//
//  Snapshots the tone-engine state into a UART_Status_t and transmits it.
//  Uses g_playPause and g_soundVolume as the authoritative values so the
//  host sees what the MCU is actually applying.
// ---------------------------------------------------------------------------
static void buildAndSendStatus(void)
{
    UART_Status_t status;

    status.playState     = (PlayState_e)g_playPause;
    status.playMode      = (PlayMode_e)MusicalMotorTone_getMode();
    status.songId        = 0U;
    status.noteIndex     = MusicalMotorTone_getNoteIndex();
    status.noteTotal     = MusicalMotorTone_getNoteTotal();
    status.currentFreqHz = MusicalMotorTone_getCurrentFreq();
    status.amplitude     = g_soundVolume;
    status.isrTicker     = MusicalMotorTone_getIsrTicker();

    UART_Link_sendStatus(&status);
}
