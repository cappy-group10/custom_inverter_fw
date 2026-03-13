//#############################################################################
//
// FILE:   main_pmsm_open_loop.c
// TITLE:  3-Phase PMSM Open-Loop Spin (Interrupt Based)
//
//#############################################################################

#include "driverlib.h"
#include "device.h"
#include <math.h>

// --- SETTINGS ---
#define PWM_FREQ        80000       // 20kHz switching
#define SYSTEM_CLOCK    200000000   // 200 MHz
#define PWM_PERIOD      (SYSTEM_CLOCK / PWM_FREQ / 2) // 5000 counts
#define DEADBAND_CYCLES 20          

// --- SAFETY ---
// Use this variable in CCS Expressions View to start the motor
volatile int gMotorRunFlag = 1; 
volatile float gTargetFreqHz = 5.0;  
volatile float gMotorVoltage = 0.15; // Start safer (15%)

// --- GLOBALS ---
float gCurrentTheta = 0;
float gThetaStep = 0;
const float PI = 3.14159265359f;

// --- PROTOTYPES ---
void initPWM(uint32_t base);
void initPWMInterrupt(void);
__interrupt void epwm1ISR(void);

void main(void)
{
    // 1. Initialize System
    Device_init();
    Device_initGPIO();

    // 2. Configure PWM GPIOs (Site 1: GPIO 0-5)
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPinConfig(GPIO_1_EPWM1B);

    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPinConfig(GPIO_3_EPWM2B);

    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPinConfig(GPIO_5_EPWM3B);

    // 3. Configure DRV8305 Enable Pin (GPIO 124 for Site 1)
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setDirectionMode(124, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(124, GPIO_PIN_TYPE_STD);
    GPIO_writePin(124, 0); // Enable Gate Driver

    // 4. Initialize Interrupts
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Interrupt_register(INT_EPWM1, &epwm1ISR); // Map ISR

    // 5. Initialize PWMs
    initPWM(EPWM1_BASE);
    initPWM(EPWM2_BASE);
    initPWM(EPWM3_BASE);
    initPWMInterrupt(); // Configure EPWM1 to generate IRQ

    // 6. Enable Interrupts
    Interrupt_enable(INT_EPWM1);
    EINT; // Enable Global Interrupts
    ERTM; // Enable Real-time Debug

    // --- IDLE LOOP ---
    while(1)
    {
        // Calculate Step size based on Target Freq
        // ISR runs at 20kHz (0.00005s)
        // Step = 2*PI * Freq * T_s
        gThetaStep = 2.0f * PI * gTargetFreqHz * 0.00005f;
        
        // Non-critical background tasks go here (e.g., LED blinking, CAN comms)
    }
}

// 
// INTERRUPT SERVICE ROUTINE (Runs at 20kHz)
//
__interrupt void epwm1ISR(void)
{
    if(gMotorRunFlag == 1)
    {
        // 1. Update Angle
        gCurrentTheta += gThetaStep;
        if(gCurrentTheta > 2.0f * PI) gCurrentTheta -= 2.0f * PI;

        // 2. Calculate Sine (Space Vector is better, but Sin works for test)
        // NOTE: Enable TMU in project settings for fast sinf()
        float valU = sinf(gCurrentTheta);
        float valV = sinf(gCurrentTheta - (2.0944f)); // 2*PI/3
        float valW = sinf(gCurrentTheta - (4.1888f)); // 4*PI/3

        // 3. Scale to Duty Cycle (0.0 to 1.0)
        float fDutyU = (valU * gMotorVoltage) + 0.5f;
        float fDutyV = (valV * gMotorVoltage) + 0.5f;
        float fDutyW = (valW * gMotorVoltage) + 0.5f;

        // 4. Update PWM Registers
        EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, (uint16_t)(fDutyU * PWM_PERIOD));
        EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, (uint16_t)(fDutyV * PWM_PERIOD));
        EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, (uint16_t)(fDutyW * PWM_PERIOD));
    }
    else
    {
        // Disable outputs if flag is 0 (50% duty = 0V line-to-line, or set to 0 for low side on)
        // Setting 50% usually holds the motor still (brake) or coasts depending on current
        EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, PWM_PERIOD / 2);
        EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, PWM_PERIOD / 2);
        EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, PWM_PERIOD / 2);
    }

    // 5. Clear Interrupt Flag
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

void initPWM(uint32_t base)
{
    EPWM_setTimeBasePeriod(base, PWM_PERIOD);
    EPWM_setPhaseShift(base, 0U);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);

    // Shadow load on Zero (Bottom of triangle)
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);

    // Active High Logic (Centered)
    // Up-Count match -> Set Low
    // Down-Count match -> Set High
    // Result: Pulse is High "around" the Zero point (Valley).
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    // Deadband (Active High Complementary)
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setRisingEdgeDelayCount(base, DEADBAND_CYCLES);
    EPWM_setFallingEdgeDelayCount(base, DEADBAND_CYCLES);
}

void initPWMInterrupt(void)
{
    // Trigger interrupt when Counter is at ZERO (start of PWM cycle)
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_enableInterrupt(EPWM1_BASE);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1); // Every 1st event
}
