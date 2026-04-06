//#############################################################################
//
// FILE:   main_musical_motor_teknik.c
//
// TITLE:  Musical Motor Controller - Teknik Motor (Open-Loop SVPWM)
//
// Target: TMS320F28379D (CPU1), LAUNCHXL-F28379D + BOOSTXL-3PhGaNInv
//
// Description:
//   Produces musical tones from the Teknik motor by driving all three phases
//   with open-loop SVPWM.  The PWM carrier stays at a fixed inaudible
//   frequency (~10 kHz).  A voltage vector of controlled amplitude rotates
//   in the alpha-beta plane at the desired audio note frequency, causing
//   the stator to vibrate and radiate sound.
//
//   Signal chain (runs every PWM ISR @ carrier freq):
//     noteFreq -> RAMPGEN (angle 0-1 pu) -> sin/cos
//              -> Inverse Park (Vd=0, Vq=amplitude -> Ualpha, Ubeta)
//              -> SVGEN (-> Ta, Tb, Tc)
//              -> EPWM1/2/3 CMPA registers
//
//   GPIO0/1 = EPWM1A/B (Phase A)
//   GPIO2/3 = EPWM2A/B (Phase B)
//   GPIO4/5 = EPWM3A/B (Phase C)
//
//#############################################################################

//
// Included Files
//
#include "device.h"
#include "driverlib.h"

// TI math blocks for motor control
#include "ipark.h"      // Inverse Park transform
#include "svgen.h"      // Space-Vector PWM generator
#include "rampgen.h"    // Sawtooth angle ramp

// ============================================================================
// System Configuration
// ============================================================================

#define SYSCLK_FREQ         DEVICE_SYSCLK_FREQ      // 200 MHz

//
// PWM carrier: fixed at 10 kHz (inaudible).
// TBCLK = SYSCLK (no prescaler) for maximum duty resolution.
// Up-down count mode: TBPRD = SYSCLK / (2 * Fcarrier)
//
#define PWM_CARRIER_FREQ    20000U                              // 10 kHz
#define PWM_TBPRD           (SYSCLK_FREQ / (2U * PWM_CARRIER_FREQ))  // 10000
#define PWM_HALF_TBPRD      (PWM_TBPRD / 2U)                         // 5000

//
// Dead-band in TBCLK counts (TBCLK = SYSCLK = 200 MHz, 1 count = 5 ns).
// 100 counts = 500 ns — matches BOOSTXL GaN FET requirements.
//
#define PWM_DEADBAND_RED    100U
#define PWM_DEADBAND_FED    100U

//
// ISR period (seconds) — used by the ramp generator.
// Ts = 1 / carrier_freq
//
#define ISR_FREQ_HZ         PWM_CARRIER_FREQ
#define ISR_PERIOD_S        (1.0f / (float32_t)ISR_FREQ_HZ)    // 100 us

// ============================================================================
// Tone / Volume Configuration
// ============================================================================

//
// Voltage amplitude for the rotating vector (per-unit, 0.0 to 1.0).
// Controls loudness.  Start LOW (~0.05) and increase gradually.
// At 0.10 with a 24 V bus, peak phase voltage ≈ 2.4 V.
//
#define TONE_VQ_DEFAULT     0.2f

// ============================================================================
// Hardware Pin Assignments
// ============================================================================

#define GPIO_DRV_EN         124U    // BOOSTXL gate-driver enable (active high)
#define GPIO_LED1           31U     // On-board blue LED  (active low)
#define GPIO_LED2           34U     // On-board red LED   (active low)
#define GPIO_DBG1           24U     // Debug / scope trigger
#define GPIO_DBG2           16U     // Debug output 2

// ============================================================================
// Note Frequencies (Hz)
// ============================================================================

#define NOTE_REST   0.0f

#define NOTE_D3     147.0f
#define NOTE_E3     165.0f
#define NOTE_F3     175.0f
#define NOTE_G3     196.0f
#define NOTE_A3     220.0f
#define NOTE_Bb3    233.0f
#define NOTE_B3     247.0f

#define NOTE_C4     262.0f
#define NOTE_D4     294.0f
#define NOTE_E4     330.0f
#define NOTE_F4     349.0f
#define NOTE_Fs4    370.0f
#define NOTE_G4     392.0f
#define NOTE_Ab4    415.0f
#define NOTE_A4     440.0f
#define NOTE_Bb4    466.0f
#define NOTE_B4     494.0f

#define NOTE_C5     523.0f
#define NOTE_D5     587.0f
#define NOTE_E5     659.0f
#define NOTE_F5     698.0f
#define NOTE_Fs5    740.0f
#define NOTE_G5     784.0f
#define NOTE_Ab5    831.0f
#define NOTE_A5     880.0f
#define NOTE_Bb5    932.0f
#define NOTE_B5     988.0f

#define NOTE_C6     1047.0f

// ============================================================================
// Melody Definition
// ============================================================================

typedef struct {
    float32_t freqHz;       // Note frequency in Hz (0 = rest)
    uint32_t  durationMs;   // Duration in milliseconds
} NoteEntry;

//
// Song selector: change this to pick which melody plays
//   0 = Super Mario Bros
//   1 = Megalovania (Undertale)
//
#define SONG_SELECT     0

// ============================================================================
// Duration macros (undef'd after use)
// ============================================================================
#define W   1200    // whole note
#define H   600     // half note
#define Q   300     // quarter note
#define Q_D 450     // dotted quarter
#define E   150     // eighth note
#define S   75      // sixteenth note

// ============================================================================
// Song 0: Super Mario Bros - Main Theme (~200 BPM)
// ============================================================================
static const NoteEntry melodyMario[] = {
    // Intro phrase
    { NOTE_E5,  E }, { NOTE_E5,  E }, { NOTE_REST, E }, { NOTE_E5,  E },
    { NOTE_REST, E }, { NOTE_C5, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_G5,  Q }, { NOTE_REST, Q },
    { NOTE_G4,  Q }, { NOTE_REST, Q },

    // Phrase 1
    { NOTE_C5,  Q_D }, { NOTE_G4, E }, { NOTE_REST, Q },
    { NOTE_E4,  Q_D }, { NOTE_A4, E }, { NOTE_REST, E },
    { NOTE_B4,  E }, { NOTE_REST, E }, { NOTE_Bb4, E }, { NOTE_A4,  E },
    { NOTE_REST, E },

    { NOTE_G4,  E }, { NOTE_E5,  E }, { NOTE_G5,  E },
    { NOTE_A5,  E }, { NOTE_REST, E }, { NOTE_F5,  E }, { NOTE_G5,  E },
    { NOTE_REST, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_C5,  E }, { NOTE_D5,  E }, { NOTE_B4,  Q_D },

    // Phrase 2
    { NOTE_C5,  Q_D }, { NOTE_G4, E }, { NOTE_REST, Q },
    { NOTE_E4,  Q_D }, { NOTE_A4, E }, { NOTE_REST, E },
    { NOTE_B4,  E }, { NOTE_REST, E }, { NOTE_Bb4, E }, { NOTE_A4,  E },
    { NOTE_REST, E },

    { NOTE_G4,  E }, { NOTE_E5,  E }, { NOTE_G5,  E },
    { NOTE_A5,  E }, { NOTE_REST, E }, { NOTE_F5,  E }, { NOTE_G5,  E },
    { NOTE_REST, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_C5,  E }, { NOTE_D5,  E }, { NOTE_B4,  Q_D },

    { NOTE_REST, H },
};
#define MARIO_LEN  (sizeof(melodyMario) / sizeof(melodyMario[0]))

// ============================================================================
// Song 1: Megalovania - Undertale (~120 BPM, swing feel)
// ============================================================================
static const NoteEntry melodyMegalovania[] = {
    // Iconic opening riff (D D D' A)
    { NOTE_D4,  S }, { NOTE_D4,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    // Second phrase (C C D' A)
    { NOTE_C4,  S }, { NOTE_C4,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    // Third phrase (B3 B3 D' A)
    { NOTE_B3,  S }, { NOTE_B3,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    // Fourth phrase (Bb3 Bb3 D' A)
    { NOTE_Bb3, S }, { NOTE_Bb3, S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    // Bridge / descending run
    { NOTE_D4,  E }, { NOTE_D4,  E }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  E }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  Q },

    { NOTE_D4,  S }, { NOTE_F4,  S }, { NOTE_G4,  E },
    { NOTE_D4,  S }, { NOTE_F4,  S }, { NOTE_G4,  E },
    { NOTE_E4,  S }, { NOTE_G4,  S }, { NOTE_A4,  E },
    { NOTE_A4,  E }, { NOTE_A4,  E }, { NOTE_G4,  E },

    { NOTE_REST, H },
};
#define MEGALOVANIA_LEN  (sizeof(melodyMegalovania) / sizeof(melodyMegalovania[0]))

#undef W
#undef H
#undef Q
#undef Q_D
#undef E
#undef S

// ============================================================================
// Active melody pointer and length (set by SONG_SELECT)
// ============================================================================
#if SONG_SELECT == 0
    static const NoteEntry * const melody = melodyMario;
    #define MELODY_LEN  MARIO_LEN
#elif SONG_SELECT == 1
    static const NoteEntry * const melody = melodyMegalovania;
    #define MELODY_LEN  MEGALOVANIA_LEN
#else
    #error "Invalid SONG_SELECT"
#endif

// ============================================================================
// Global State
// ============================================================================

//
// Math block instances
//
RAMPGEN  rampGen  = RAMPGEN_DEFAULTS;
IPARK    iPark    = IPARK_DEFAULTS;
SVGEN    svGen    = SVGEN_DEFAULTS;

//
// Tone state (written by melody sequencer, read by PWM ISR)
//
volatile float32_t toneFreqHz   = 0.0f;        // Current note frequency
volatile float32_t toneAmplitude = TONE_VQ_DEFAULT;  // Vq amplitude (pu)
volatile uint16_t  toneActive   = 0;           // 1 = playing, 0 = silent

//
// Melody sequencer state (used by Timer0 ISR only)
//
volatile uint16_t  noteIndex    = 0;
volatile uint32_t  noteTicksRemaining = 0;

// ============================================================================
// Function Prototypes
// ============================================================================

// Peripheral init
void initGPIO(void);
void initPIE(void);
void initEPWM(uint32_t base);
void initEPWM1(void);
void initEPWM2(void);
void initEPWM3(void);
void initCPUTimer0(void);

// Musical motor core
void toneInit(void);
void toneSetNote(float32_t freqHz);
void toneUpdate(void);
void toneWritePWM(void);

// ISRs
__interrupt void epwm1ISR(void);
__interrupt void cpuTimer0ISR(void);

// ============================================================================
// Main
// ============================================================================
void main(void)
{
    //
    // Device init: watchdog, PLL (200 MHz), peripheral clocks
    //
    Device_init();

    //
    // GPIO init: unlock ports, configure pins
    //
    Device_initGPIO();
    initGPIO();

    //
    // PIE init: clear vectors, initialize table
    //
    initPIE();

    //
    // Register ISRs
    //
    Interrupt_register(INT_EPWM1, &epwm1ISR);
    Interrupt_register(INT_TIMER0, &cpuTimer0ISR);

    //
    // EPWM setup (disable TBCLK sync during config)
    //
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    initEPWM1();
    initEPWM2();
    initEPWM3();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Initialize tone generator math blocks
    //
    toneInit();

    //
    // Load first note
    //
    toneSetNote(melody[0].freqHz);
    noteTicksRemaining = melody[0].durationMs;

    //
    // Start CPU Timer0 for melody sequencing (1 ms tick)
    //
    initCPUTimer0();

    //
    // Enable interrupts
    //
    Interrupt_enable(INT_EPWM1);
    Interrupt_enable(INT_TIMER0);

    //
    // Enable DRV gate (Active Low)
    //
    GPIO_writePin(GPIO_DRV_EN, 0);

    //
    // Global interrupt enable
    //
    EINT;
    ERTM;

    //
    // Background loop — heartbeat only; all real work is in ISRs
    //
    for(;;)
    {
        GPIO_togglePin(GPIO_LED1);
        DEVICE_DELAY_US(500000);
    }
}

// ============================================================================
// Tone Generator Functions
// ============================================================================

//
// toneInit - Initialize the ramp generator for the given ISR rate.
//
// The ramp generator produces a sawtooth 0 -> 1 (per-unit angle).
// StepAngleMax = Ts so that:  angle += Ts * freqHz  per ISR call,
// completing one full 0->1 cycle in (1/freqHz) seconds.
//
void toneInit(void)
{
    rampGen.Freq         = 0.0f;
    rampGen.StepAngleMax = ISR_PERIOD_S;    // = 1 / ISR_FREQ_HZ
    rampGen.Angle        = 0.0f;
    rampGen.Gain         = 1.0f;
    rampGen.Out          = 0.0f;
    rampGen.Offset       = 0.0f;
}

//
// toneSetNote - Change the current note frequency.
//              freqHz = 0 produces silence (outputs forced low).
//
void toneSetNote(float32_t freqHz)
{
    if(freqHz > 0.0f)
    {
        toneFreqHz = freqHz;
        rampGen.Freq = freqHz;
        toneActive = 1;
    }
    else
    {
        toneFreqHz = 0.0f;
        rampGen.Freq = 0.0f;
        toneActive = 0;
    }
}

//
// toneUpdate - Run one iteration of the open-loop SVPWM signal chain.
//             Called from the EPWM ISR at the carrier frequency.
//
// Signal flow:
//   1. RAMPGEN: advance angle at note frequency -> sawtooth 0..1
//   2. sin/cos of angle (per-unit, using TI FPU intrinsics)
//   3. Inverse Park: Vd=0, Vq=amplitude -> Ualpha, Ubeta
//   4. SVGEN: Ualpha, Ubeta -> Ta, Tb, Tc (switching functions)
//
void toneUpdate(void)
{
    //
    // Step 1: Advance the electrical angle
    //
    fclRampGen(&rampGen);

    //
    // Step 2: Compute sin/cos (per-unit angle input: 0-1 maps to 0-2*pi)
    //
    float32_t sinVal = __sinpuf32(rampGen.Out);
    float32_t cosVal = __cospuf32(rampGen.Out);

    //
    // Step 3: Inverse Park transform
    //   Ds (Vd) = 0      -> no radial flux forcing
    //   Qs (Vq) = amplitude -> tangential excitation = tone volume
    //
    iPark.Ds     = 0.0f;
    iPark.Qs     = toneAmplitude;
    iPark.Sine   = sinVal;
    iPark.Cosine = cosVal;
    runIPark(&iPark);

    //
    // Step 4: Space-Vector PWM generation
    //
    svGen.Ualpha = iPark.Alpha;
    svGen.Ubeta  = iPark.Beta;
    runSVGenDQ(&svGen);
}

//
// toneWritePWM - Write the SVGEN outputs to the EPWM compare registers.
//
// Maps Ta/Tb/Tc from [-1, +1] to [0, TBPRD]:
//   CMPA = (HALF_TBPRD * Tx) + HALF_TBPRD
//
// Note: SVGEN output order is Tc->EPWM1, Ta->EPWM2, Tb->EPWM3
//       (matches TI's standard phase mapping for the BOOSTXL).
//
void toneWritePWM(void)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)((PWM_HALF_TBPRD * svGen.Tc) + PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)((PWM_HALF_TBPRD * svGen.Ta) + PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)((PWM_HALF_TBPRD * svGen.Tb) + PWM_HALF_TBPRD));
}

// ============================================================================
// EPWM1 ISR — Runs at carrier frequency (10 kHz)
// ============================================================================
//
// This is the fast loop.  Every carrier cycle:
//   - If tone is active: compute SVPWM and update duty cycles
//   - If silent: hold outputs at 50% (zero voltage across phases)
//
__interrupt void epwm1ISR(void)
{
    GPIO_writePin(GPIO_DBG1, 1);

    if(toneActive)
    {
        toneUpdate();
        toneWritePWM();
    }
    else
    {
        //
        // Silence: set all phases to 50% duty (Vab = Vbc = Vca = 0)
        //
        EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
                                    PWM_HALF_TBPRD);
        EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A,
                                    PWM_HALF_TBPRD);
        EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A,
                                    PWM_HALF_TBPRD);
    }

    GPIO_writePin(GPIO_DBG1, 0);

    //
    // Clear EPWM1 INT flag and acknowledge PIE group 3
    //
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

// ============================================================================
// CPU Timer0 ISR — Melody Sequencer (1 ms tick)
// ============================================================================
__interrupt void cpuTimer0ISR(void)
{
    if(noteTicksRemaining > 0)
    {
        noteTicksRemaining--;
    }

    if(noteTicksRemaining == 0)
    {
        //
        // Advance to next note
        //
        noteIndex++;
        if(noteIndex >= MELODY_LEN)
        {
            noteIndex = 0;
        }

        toneSetNote(melody[noteIndex].freqHz);
        noteTicksRemaining = melody[noteIndex].durationMs;
    }

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

// ============================================================================
// GPIO Initialization
// ============================================================================
void initGPIO(void)
{
    // Phase A: EPWM1A/1B -> GPIO0/GPIO1
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPadConfig(0, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(0, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPadConfig(1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(1, GPIO_DIR_MODE_OUT);

    // Phase B: EPWM2A/2B -> GPIO2/GPIO3
    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPadConfig(2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(2, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPadConfig(3, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(3, GPIO_DIR_MODE_OUT);

    // Phase C: EPWM3A/3B -> GPIO4/GPIO5
    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPadConfig(4, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(4, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_5_EPWM3B);
    GPIO_setPadConfig(5, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(5, GPIO_DIR_MODE_OUT);

    // DRV gate enable — start disabled
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setPadConfig(GPIO_DRV_EN, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DRV_EN, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DRV_EN, 0);

    // LEDs — start OFF
    GPIO_setPinConfig(GPIO_31_GPIO31);
    GPIO_setPadConfig(GPIO_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED1, 1);

    GPIO_setPinConfig(GPIO_34_GPIO34);
    GPIO_setPadConfig(GPIO_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED2, 1);

    // Debug GPIOs
    GPIO_setPinConfig(GPIO_24_GPIO24);
    GPIO_setPadConfig(GPIO_DBG1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DBG1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DBG1, 0);

    GPIO_setPinConfig(GPIO_16_GPIO16);
    GPIO_setPadConfig(GPIO_DBG2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DBG2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DBG2, 0);
}

// ============================================================================
// PIE Initialization
// ============================================================================
void initPIE(void)
{
    Interrupt_initModule();
    Interrupt_initVectorTable();
}

// ============================================================================
// Common EPWM Configuration (per phase)
// ============================================================================
void initEPWM(uint32_t base)
{
    //
    // Time-Base: up-down count at full SYSCLK (200 MHz), no prescaler
    //
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, PWM_TBPRD);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_UP_AFTER_SYNC);

    //
    // Shadow load on counter == zero for glitch-free updates
    //
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    //
    // Start at 50% duty (zero net voltage)
    //
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, PWM_HALF_TBPRD);

    //
    // Action Qualifier: EPWMxA
    //   HIGH on CMPA up-count, LOW on CMPA down-count -> centered pulse
    //
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    //
    // Dead-Band: Active High Complementary (AHC)
    //   EPWMxA -> RED -> output A (non-inverted)
    //   EPWMxA -> FED -> output B (inverted)
    //
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED, EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setRisingEdgeDelayCount(base, PWM_DEADBAND_RED);
    EPWM_setFallingEdgeDelayCount(base, PWM_DEADBAND_FED);

    //
    // Free-run on emulation halt
    //
    EPWM_setEmulationMode(base, EPWM_EMULATION_FREE_RUN);
}

// ============================================================================
// Per-Phase EPWM Init
// ============================================================================
void initEPWM1(void)
{
    initEPWM(EPWM1_BASE);

    //
    // EPWM1 is sync master
    //
    EPWM_setSyncOutPulseMode(EPWM1_BASE, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);

    //
    // EPWM1 generates the ISR: interrupt on counter == zero, every cycle
    //
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
}

void initEPWM2(void)
{
    initEPWM(EPWM2_BASE);
    EPWM_setSyncOutPulseMode(EPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM2_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM2_BASE);
}

void initEPWM3(void)
{
    initEPWM(EPWM3_BASE);
    EPWM_setSyncOutPulseMode(EPWM3_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM3_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM3_BASE);
}

// ============================================================================
// CPU Timer0 — Melody Sequencer (1 ms tick)
// ============================================================================
void initCPUTimer0(void)
{
    //
    // 1 ms period = 200,000 counts at 200 MHz
    //
    CPUTimer_setPeriod(CPUTIMER0_BASE, (uint32_t)(SYSCLK_FREQ / 1000U));
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    CPUTimer_startTimer(CPUTIMER0_BASE);
}
