//#############################################################################
//
// FILE:   main_musical_motor_teknik.c
//
// TITLE:  Musical Motor Controller - Teknik Motor
//
// Target: TMS320F28379D (CPU1), LAUNCHXL-F28379D + BOOSTXL-3PhGaNInv
//
// Description:
//   Drives the Teknik motor coils at audio frequencies so they act as a
//   speaker.  PWM carrier frequency = note frequency.  EPWM1/2/3 (A+B)
//   output complementary PWM with dead-band through the BOOSTXL half-bridges.
//   CPU Timer0 ISR advances through a melody table at configurable tempo.
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

// ============================================================================
// Defines
// ============================================================================

#define SYSCLK_FREQ         DEVICE_SYSCLK_FREQ      // 200 MHz

//
// TBCLK prescaler: SYSCLK / (CLKDIV * HSCLKDIV) = 200M / (8*1) = 25 MHz
// This lets TBPRD fit 16 bits for audio range (≥~190 Hz).
//
#define PWM_CLKDIV          EPWM_CLOCK_DIVIDER_8
#define PWM_HSCLKDIV        EPWM_HSCLOCK_DIVIDER_1
#define TBCLK_FREQ          (SYSCLK_FREQ / 8U)     // 25 MHz

//
// Dead-band in TBCLK counts.  At 25 MHz, 1 count = 40 ns.
// 13 counts ≈ 520 ns — adequate for GaN FETs on the BOOSTXL.
//
#define PWM_DEADBAND_RED    13U
#define PWM_DEADBAND_FED    13U

//
// Duty cycle percentage (keep low to limit coil current)
// In up-down mode with AQ set high on CMPA-up, low on CMPA-down:
//   duty = (TBPRD - CMPA) / TBPRD
//   => CMPA = TBPRD * (100 - DUTY_PCT) / 100
//
#define DUTY_PCT            50U

//
// BOOSTXL DRV gate enable (verify pin for your board revision)
//
#define GPIO_DRV_EN         124U

//
// Status LED / Debug GPIO
//
#define GPIO_LED1           31U     // On-board blue LED (active low)
#define GPIO_LED2           34U     // On-board red LED  (active low)
#define GPIO_DBG1           24U     // Debug output 1
#define GPIO_DBG2           16U     // Debug output 2

// ============================================================================
// Note Table — TBPRD values for musical notes
// ============================================================================
//
// TBPRD = TBCLK_FREQ / (2 * freq_hz)   (up-down count mode)
//
// NOTE_REST sets period to 0 which freezes the counter (silence).
//
#define NOTE_REST   0U

#define NOTE_C4     47710U  // 262 Hz
#define NOTE_D4     42517U  // 294 Hz
#define NOTE_E4     37879U  // 330 Hz
#define NOTE_F4     35754U  // 349 Hz
#define NOTE_G4     31887U  // 392 Hz
#define NOTE_A4     28409U  // 440 Hz
#define NOTE_B4     25303U  // 494 Hz

#define NOTE_C5     23889U  // 523 Hz
#define NOTE_D5     21277U  // 587 Hz
#define NOTE_E5     18939U  // 659 Hz
#define NOTE_F5     17877U  // 698 Hz
#define NOTE_G5     15944U  // 784 Hz
#define NOTE_A5     14205U  // 880 Hz
#define NOTE_B5     12652U  // 988 Hz

#define NOTE_C6     11905U  // 1047 Hz

// ============================================================================
// Melody Definition
// ============================================================================
//
// Each entry: { TBPRD value, duration in timer ticks }
// One tick = note sequencer interval (configured below).
//
typedef struct {
    uint16_t tbprd;         // Note period (0 = rest/silence)
    uint16_t duration;      // Duration in timer ticks
} NoteEntry;

//
// "Twinkle Twinkle Little Star" — a recognizable test melody
//
// static const NoteEntry melody[] = {
//     { NOTE_C4, 2 }, { NOTE_C4, 2 }, { NOTE_G4, 2 }, { NOTE_G4, 2 },
//     { NOTE_A4, 2 }, { NOTE_A4, 2 }, { NOTE_G4, 4 },
//     { NOTE_F4, 2 }, { NOTE_F4, 2 }, { NOTE_E4, 2 }, { NOTE_E4, 2 },
//     { NOTE_D4, 2 }, { NOTE_D4, 2 }, { NOTE_C4, 4 },
//     { NOTE_REST, 2 },  // brief pause before repeat
// };
static const NoteEntry melody[] = {
    { NOTE_C4, 2 }
};

#define MELODY_LEN  (sizeof(melody) / sizeof(melody[0]))

//
// Tempo: timer ISR fires at this rate; each "tick" = this many microseconds.
// 125000 µs = 125 ms per tick.  A quarter note (duration=2) = 250 ms → 240 BPM.
//
#define NOTE_TICK_US    125000U

// ============================================================================
// Globals
// ============================================================================
volatile uint16_t noteIndex     = 0;
volatile uint16_t tickCounter   = 0;

// ============================================================================
// Function Prototypes
// ============================================================================
void initGPIO(void);
void initPIE(void);

void initEPWM(uint32_t base, uint16_t tbprd);
void initEPWM1(void);
void initEPWM2(void);
void initEPWM3(void);

void initCPUTimer0(void);

void setNote(uint16_t tbprd);

__interrupt void cpuTimer0ISR(void);

// ============================================================================
// Main
// ============================================================================
void main(void)
{
    //
    // Initialize device clock, PLL, watchdog, and peripheral clocks
    //
    Device_init();

    //
    // Unlock and configure GPIOs
    //
    Device_initGPIO();
    initGPIO();

    //
    // Initialize the PIE module and vector table
    //
    initPIE();

    //
    // Register Timer0 ISR
    //
    Interrupt_register(INT_TIMER0, &cpuTimer0ISR);

    //
    // Disable TBCLK sync while configuring all EPWM modules
    //
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Initialize EPWM1/2/3 with the first note
    //
    initEPWM1();
    initEPWM2();
    initEPWM3();

    //
    // Sync-enable all EPWM time-base clocks
    //
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Configure and start the note sequencer timer
    //
    initCPUTimer0();

    //
    // Enable Timer0 interrupt in PIE
    //
    Interrupt_enable(INT_TIMER0);

    //
    // Enable DRV gate 
    //
    GPIO_writePin(GPIO_DRV_EN, 0);

    //
    // Enable global interrupts
    //
    EINT;
    ERTM;

    //
    // Main loop — heartbeat LED only; all work is in ISR
    //
    for(;;)
    {
        GPIO_togglePin(GPIO_LED1);
        DEVICE_DELAY_US(500000);
    }
}

// ============================================================================
// Set all three phases to the same note
// ============================================================================
void setNote(uint16_t tbprd)
{
    uint16_t cmpa;

    if(tbprd == NOTE_REST)
    {
        //
        // Silence: freeze counters, force outputs low
        //
        EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
        EPWM_setTimeBaseCounterMode(EPWM2_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
        EPWM_setTimeBaseCounterMode(EPWM3_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);

        EPWM_setActionQualifierSWAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW);
        EPWM_setActionQualifierSWAction(EPWM1_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW);
        EPWM_setActionQualifierSWAction(EPWM2_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW);
        EPWM_setActionQualifierSWAction(EPWM2_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW);
        EPWM_setActionQualifierSWAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW);
        EPWM_setActionQualifierSWAction(EPWM3_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW);

        EPWM_forceActionQualifierSWAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A);
        EPWM_forceActionQualifierSWAction(EPWM1_BASE, EPWM_AQ_OUTPUT_B);
        EPWM_forceActionQualifierSWAction(EPWM2_BASE, EPWM_AQ_OUTPUT_A);
        EPWM_forceActionQualifierSWAction(EPWM2_BASE, EPWM_AQ_OUTPUT_B);
        EPWM_forceActionQualifierSWAction(EPWM3_BASE, EPWM_AQ_OUTPUT_A);
        EPWM_forceActionQualifierSWAction(EPWM3_BASE, EPWM_AQ_OUTPUT_B);
        return;
    }

    //
    // CMPA for desired duty: duty% = (TBPRD - CMPA) / TBPRD
    //
    cmpa = (uint16_t)((uint32_t)tbprd * (100U - DUTY_PCT) / 100U);

    //
    // Update period and compare (shadow registers, loads on next zero)
    //
    EPWM_setTimeBasePeriod(EPWM1_BASE, tbprd);
    EPWM_setTimeBasePeriod(EPWM2_BASE, tbprd);
    EPWM_setTimeBasePeriod(EPWM3_BASE, tbprd);

    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, cmpa);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, cmpa);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, cmpa);

    //
    // Resume counting if previously frozen
    //
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounterMode(EPWM2_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounterMode(EPWM3_BASE, EPWM_COUNTER_MODE_UP_DOWN);
}

// ============================================================================
// CPU Timer0 ISR — Note Sequencer
// ============================================================================
__interrupt void cpuTimer0ISR(void)
{
    GPIO_writePin(GPIO_DBG1, 1);    // scope trigger

    tickCounter++;

    if(tickCounter >= melody[noteIndex].duration)
    {
        tickCounter = 0;
        noteIndex++;
        if(noteIndex >= MELODY_LEN)
        {
            noteIndex = 0;
        }
        setNote(melody[noteIndex].tbprd);
    }

    GPIO_writePin(GPIO_DBG1, 0);

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

// ============================================================================
// GPIO Initialization
// ============================================================================
void initGPIO(void)
{
    //
    // EPWM1A/1B -> GPIO0/GPIO1 (Phase A)
    //
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPadConfig(0, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(0, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPadConfig(1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(1, GPIO_DIR_MODE_OUT);

    //
    // EPWM2A/2B -> GPIO2/GPIO3 (Phase B)
    //
    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPadConfig(2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(2, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPadConfig(3, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(3, GPIO_DIR_MODE_OUT);

    //
    // EPWM3A/3B -> GPIO4/GPIO5 (Phase C)
    //
    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPadConfig(4, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(4, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_5_EPWM3B);
    GPIO_setPadConfig(5, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(5, GPIO_DIR_MODE_OUT);

    //
    // DRV gate enable — active high, start disabled
    //
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setPadConfig(GPIO_DRV_EN, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DRV_EN, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DRV_EN, 0);

    //
    // Status LEDs — start OFF (active low on LaunchPad)
    //
    GPIO_setPinConfig(GPIO_31_GPIO31);
    GPIO_setPadConfig(GPIO_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED1, 1);

    GPIO_setPinConfig(GPIO_34_GPIO34);
    GPIO_setPadConfig(GPIO_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED2, 1);

    //
    // Debug GPIOs — scope probing
    //
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
// Common EPWM configuration (called per phase)
// ============================================================================
void initEPWM(uint32_t base, uint16_t tbprd)
{
    //
    // Time-Base: up-down count, prescaled to 25 MHz TBCLK
    //
    EPWM_setClockPrescaler(base, PWM_CLKDIV, PWM_HSCLKDIV);
    EPWM_setTimeBasePeriod(base, tbprd);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_UP_AFTER_SYNC);

    //
    // Shadow load on zero for glitch-free note changes
    //
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    //
    // Initial compare value for DUTY_PCT
    //
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A,
                                (uint16_t)((uint32_t)tbprd * (100U - DUTY_PCT) / 100U));

    //
    // Action Qualifier: EPWMxA
    //   HIGH on CMPA up-count, LOW on CMPA down-count
    //   → centered pulse, duty = (TBPRD - CMPA) / TBPRD
    //
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    //
    // Dead-Band: Active High Complementary (AHC)
    //   EPWMxA source → RED on output A, inverted FED on output B
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
// Per-phase init — EPWM1 is sync master, EPWM2/3 slave
// ============================================================================
void initEPWM1(void)
{
    initEPWM(EPWM1_BASE, melody[0].tbprd);
    EPWM_setSyncOutPulseMode(EPWM1_BASE, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);
}

void initEPWM2(void)
{
    initEPWM(EPWM2_BASE, melody[0].tbprd);
    EPWM_setSyncOutPulseMode(EPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM2_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM2_BASE);
}

void initEPWM3(void)
{
    initEPWM(EPWM3_BASE, melody[0].tbprd);
    EPWM_setSyncOutPulseMode(EPWM3_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM3_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM3_BASE);
}

// ============================================================================
// CPU Timer0 — Note Sequencer Clock
// ============================================================================
void initCPUTimer0(void)
{
    //
    // Timer period = SYSCLK_FREQ * NOTE_TICK_US / 1e6
    // At 200 MHz, 125 ms tick = 25,000,000 counts
    //
    CPUTimer_setPeriod(CPUTIMER0_BASE,
                       (uint32_t)((float)SYSCLK_FREQ / 1e6f * NOTE_TICK_US));
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);

    //
    // Enable interrupt and start
    //
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    CPUTimer_startTimer(CPUTIMER0_BASE);
}
