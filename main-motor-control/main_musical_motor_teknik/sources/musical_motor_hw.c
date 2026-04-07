//#############################################################################
//
// FILE:   musical_motor_hw.c
//
// TITLE:  Musical Motor hardware setup and PWM output helpers
//
//#############################################################################

#include "musical_motor_hw.h"

#define PWM_TBPRD      \
    (MUSICAL_MOTOR_HW_SYSCLK_FREQ_HZ / (2U * MUSICAL_MOTOR_HW_PWM_CARRIER_FREQ_HZ))
#define PWM_HALF_TBPRD (PWM_TBPRD / 2U)

#define PWM_DEADBAND_RED 100U
#define PWM_DEADBAND_FED 100U

#define GATE_DRIVER_ENABLE 1U

#define GPIO_DRV_EN 124U
#define GPIO_LED1   31U
#define GPIO_LED2   34U
#define GPIO_DBG1   24U
#define GPIO_DBG2   16U

static void MusicalMotorHw_initEPWMChannel(uint32_t base);
static void MusicalMotorHw_initEPWM1(void);
static void MusicalMotorHw_initEPWM2(void);
static void MusicalMotorHw_initEPWM3(void);

void MusicalMotorHw_initGPIO(void)
{
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPadConfig(0U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(0U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPadConfig(1U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(1U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPadConfig(2U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(2U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPadConfig(3U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(3U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPadConfig(4U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(4U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_5_EPWM3B);
    GPIO_setPadConfig(5U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(5U, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_setPadConfig(GPIO_DRV_EN, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DRV_EN, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DRV_EN, 0U);

    GPIO_setPinConfig(GPIO_31_GPIO31);
    GPIO_setPadConfig(GPIO_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED1, 1U);

    GPIO_setPinConfig(GPIO_34_GPIO34);
    GPIO_setPadConfig(GPIO_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_LED2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_LED2, 1U);

    GPIO_setPinConfig(GPIO_24_GPIO24);
    GPIO_setPadConfig(GPIO_DBG1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DBG1, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DBG1, 0U);

    GPIO_setPinConfig(GPIO_16_GPIO16);
    GPIO_setPadConfig(GPIO_DBG2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(GPIO_DBG2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(GPIO_DBG2, 0U);
}

void MusicalMotorHw_initEPWM(void)
{
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    MusicalMotorHw_initEPWM1();
    MusicalMotorHw_initEPWM2();
    MusicalMotorHw_initEPWM3();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

void MusicalMotorHw_initCPUTimer0(void)
{
    CPUTimer_setPeriod(CPUTIMER0_BASE,
                       (uint32_t)(MUSICAL_MOTOR_HW_SYSCLK_FREQ_HZ / 1000U));
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    CPUTimer_startTimer(CPUTIMER0_BASE);
}

void MusicalMotorHw_initCPUTimer1(void)
{
    CPUTimer_setPeriod(CPUTIMER1_BASE,
                       (uint32_t)(MUSICAL_MOTOR_HW_SYSCLK_FREQ_HZ / 2U));
    CPUTimer_setPreScaler(CPUTIMER1_BASE, 0U);
    CPUTimer_stopTimer(CPUTIMER1_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER1_BASE);
    CPUTimer_enableInterrupt(CPUTIMER1_BASE);
    CPUTimer_startTimer(CPUTIMER1_BASE);
}

void MusicalMotorHw_enableGateDriver(void)
{
    GPIO_writePin(GPIO_DRV_EN, GATE_DRIVER_ENABLE);
}

__interrupt void MusicalMotorHw_heartbeatISR(void)
{
    GPIO_togglePin(GPIO_LED1);
}

void MusicalMotorHw_setDebug1(uint16_t value)
{
    GPIO_writePin(GPIO_DBG1, value);
}

void MusicalMotorHw_writeTonePwm(float32_t tc, float32_t ta, float32_t tb)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)(((float32_t)PWM_HALF_TBPRD * tc) + (float32_t)PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)(((float32_t)PWM_HALF_TBPRD * ta) + (float32_t)PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A,
        (uint16_t)(((float32_t)PWM_HALF_TBPRD * tb) + (float32_t)PWM_HALF_TBPRD));
}

void MusicalMotorHw_writeSilentPwm(void)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A,
                                PWM_HALF_TBPRD);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A,
                                PWM_HALF_TBPRD);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A,
                                PWM_HALF_TBPRD);
}

static void MusicalMotorHw_initEPWMChannel(uint32_t base)
{
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, PWM_TBPRD);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_UP_AFTER_SYNC);

    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A, PWM_HALF_TBPRD);

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);

    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED,
                                  EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED,
                                  EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setRisingEdgeDelayCount(base, PWM_DEADBAND_RED);
    EPWM_setFallingEdgeDelayCount(base, PWM_DEADBAND_FED);

    EPWM_setEmulationMode(base, EPWM_EMULATION_FREE_RUN);
}

static void MusicalMotorHw_initEPWM1(void)
{
    MusicalMotorHw_initEPWMChannel(EPWM1_BASE);
    EPWM_setSyncOutPulseMode(EPWM1_BASE, EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);
    EPWM_setInterruptSource(EPWM1_BASE, EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);
}

static void MusicalMotorHw_initEPWM2(void)
{
    MusicalMotorHw_initEPWMChannel(EPWM2_BASE);
    EPWM_setSyncOutPulseMode(EPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM2_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM2_BASE);
}

static void MusicalMotorHw_initEPWM3(void)
{
    MusicalMotorHw_initEPWMChannel(EPWM3_BASE);
    EPWM_setSyncOutPulseMode(EPWM3_BASE, EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    EPWM_setPhaseShift(EPWM3_BASE, 0U);
    EPWM_enablePhaseShiftLoad(EPWM3_BASE);
}
