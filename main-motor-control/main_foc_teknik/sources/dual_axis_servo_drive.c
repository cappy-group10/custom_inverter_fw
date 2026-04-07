//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated - http://www.ti.com/
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//#############################################################################

//------------------------------------------------------------------------------
//  Software:       Motor Control SDK
//
// FILE:    dual_axis_servo_dirve.c
//
// TITLE:   dual-axis motor drive on the related kits
//
// Group:   C2000
//
// Target Family: F2837x/F28004x/F28P55x/F28P65x
//
//-----------------------------------------------------------------------------

//
// includes
//
#include "dual_axis_servo_drive_settings.h"
#include "dual_axis_servo_drive_user.h"
#include "dual_axis_servo_drive_hal.h"
#include "dual_axis_servo_drive.h"

#include "sfra_settings.h"

#include "endat.h"

//
// Instrumentation code for timing verifications
// display variable A (in pu) on DAC
//
#define  DAC_MACRO_PU(A)  ((1.0f + A) * 2048)

//
// Functions
//
#ifdef _FLASH
#ifndef __cplusplus
#pragma CODE_SECTION(motor1ControlISR, ".TI.ramfunc");
#pragma CODE_SECTION(motor2ControlISR, ".TI.ramfunc");
#endif

#ifdef __cplusplus
#pragma CODE_SECTION(".TI.ramfunc");
#endif
#endif

//
//  Prototype statements for Local Functions
//
//#pragma INTERRUPT (motor1ControlISR, HPI)
//#pragma INTERRUPT (motor2ControlISR, HPI)
__interrupt void motor1ControlISR(void);
__interrupt void motor2ControlISR(void);

//
// Motor drive utility functions
//

#if(BUILDLEVEL > FCL_LEVEL2)
static inline void getFCLTime(MOTOR_Num_e motorNum);
#endif

//
// SFRA utility functions
//
#if(BUILDLEVEL == FCL_LEVEL6)
void injectSFRA(void);
void collectSFRA(MOTOR_Vars_t *pMotor);
#endif

//
// State Machine function prototypes
//

// Alpha states
void A0(void);  //state A0
void B0(void);  //state B0
void C0(void);  //state C0

// A branch states
void A1(void);  //state A1
void A2(void);  //state A2
void A3(void);  //state A3

// B branch states
void B1(void);  //state B1
void B2(void);  //state B2
void B3(void);  //state B3

// C branch states
void C1(void);  //state C1
void C2(void);  //state C2
void C3(void);  //state C3

// Variable declarations
void (*Alpha_State_Ptr)(void);  // Base States pointer
void (*A_Task_Ptr)(void);       // State pointer A branch
void (*B_Task_Ptr)(void);       // State pointer B branch
void (*C_Task_Ptr)(void);       // State pointer C branch

uint16_t vTimer0[4] = {0};  // Virtual Timers slaved off CPU Timer 0 (A events)
uint16_t vTimer1[4] = {0};  // Virtual Timers slaved off CPU Timer 1 (B events)
uint16_t vTimer2[4] = {0};  // Virtual Timers slaved off CPU Timer 2 (C events)
uint16_t serialCommsTimer = 0;

//
// USER Variables
//

//
// Global variables used in this system
//
MOTOR_Vars_t motorVars[2] = {MOTOR1_DEFAULTS, MOTOR2_DEFAULTS};

#pragma DATA_SECTION(motorVars, "ClaData");

//
// Variables for current measurement
//

//
// CMPSS parameters for Over Current Protection
//
uint16_t clkPrescale = 20;
uint16_t sampWin     = 30;
uint16_t thresh      = 18;

//
// Flag variables
//
volatile uint16_t enableFlag = true;

uint16_t backTicker = 0;

uint16_t led1Cnt = 0;
uint16_t led2Cnt = 0;

// Variables for Field Oriented Control
float32_t VdTesting = 0.0;          // Vd reference (pu)
float32_t VqTesting = 0.20;         // Vq reference (pu)

// Variables for position reference generation and control
float32_t posArray[8] = {2.5, -2.5, 3.5, -3.5, 5.0, -5.0, 8.0, -8.0};
float32_t posPtrMax = 4;

// Variables for Datalog module
float32_t DBUFF_4CH1[200] = {0};
float32_t DBUFF_4CH2[200] = {0};
float32_t DBUFF_4CH3[200] = {0};
float32_t DBUFF_4CH4[200] = {0};
float32_t dlogCh1 = 0;
float32_t dlogCh2 = 0;
float32_t dlogCh3 = 0;
float32_t dlogCh4 = 0;

// Create an instance of DATALOG Module
DLOG_4CH_F dlog_4ch1;

// Variables for SFRA module
#if(BUILDLEVEL == FCL_LEVEL6)
extern SFRA_F32 sfra1;
SFRATest_e      sfraTestLoop = SFRA_TEST_D_AXIS;  //speedLoop;
uint32_t        sfraCollectStart = 0;
float32_t       sfraNoiseD = 0;
float32_t       sfraNoiseQ = 0;
float32_t       sfraNoiseW = 0;
#endif

HAL_Handle    halHandle;    //!< the handle for the hardware abstraction layer
HAL_Obj       hal;          //!< the hardware abstraction layer object

HAL_MTR_Handle halMtrHandle[2];   //!< the handle for the hardware abstraction
                                  //!< layer to motor control
HAL_MTR_Obj    halMtr[2];         //!< the hardware abstraction layer object
                                  //!< to motor control

// FCL Latency variables
volatile uint16_t FCL_cycleCount[2];

// control dual motor with the same speed and acceleration at the same time
float32_t speedRef = 0.02;
float32_t IdRef = 0.0;
float32_t IqRef = 0.10;
uint32_t rampDelayMax = 0;

MotorRunStop_e runMotor = MOTOR_STOP;
CtrlState_e ctrlState = CTRL_STOP;
bool flagSyncRun = false;

//
// These are defined by the linker file
//
extern uint32_t Cla1funcsLoadStart;
extern uint32_t Cla1funcsLoadEnd;
extern uint32_t Cla1funcsRunStart;
extern uint32_t Cla1funcsLoadSize;

//
// main() function enter
//
void main(void)
{
    // initialize device clock and peripherals
    Device_init();

    // initialize the driver
    halHandle = HAL_init(&hal, sizeof(hal));

    // initialize the driver for motor 1
    halMtrHandle[MTR_1] =
            HAL_MTR_init(&halMtr[MTR_1], sizeof(halMtr[MTR_1]));

    // initialize the driver for motor 1
    halMtrHandle[MTR_2] =
            HAL_MTR_init(&halMtr[MTR_2], sizeof(halMtr[MTR_2]));

    // Disable sync(Freeze clock to PWM as well). GTBCLKSYNC is applicable
    // only for multiple core devices. Uncomment the below statement if
    // applicable.
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // set the driver parameters
    HAL_setParams(halHandle);

    // set the driver parameters for motor 1
    HAL_setMotorParams(halMtrHandle[MTR_1]);

    // set the driver parameters for motor 2
    HAL_setMotorParams(halMtrHandle[MTR_2]);

    // Enable sync and clock to PWM
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // initialize motor parameters for motor_1
    initMotorParameters(&motorVars[0], halMtrHandle[0]);

    // initialize motor parameters for motor_2
    initMotorParameters(&motorVars[1], halMtrHandle[1]);

    // initialize motor control variables for motor_1
    initControlVars(&motorVars[0]);

    // initialize motor control variables for motor_2
    initControlVars(&motorVars[1]);

    motorVars[0].currentLimit = 9.0;        // 9A
    motorVars[1].currentLimit = 9.0;        // 9A


    #ifndef DISABLE_MOTOR_FAULTS
    // setup faults protection for motor_1
    HAL_setupMotorFaultProtection(halMtrHandle[MTR_1],
                                  motorVars[MTR_1].currentLimit);

    // setup faults protection for motor_2
    HAL_setupMotorFaultProtection(halMtrHandle[MTR_2],
                                  motorVars[MTR_2].currentLimit);
    #endif // DISABLE_MOTOR_FAULTS

    // Note that the vectorial sum of d-q PI outputs should be less than 1.0 which
    // refers to maximum duty cycle for SVGEN. Another duty cycle limiting factor
    // is current sense through shunt resistors which depends on hardware/software
    // implementation. Depending on the application requirements 3,2 or a single
    // shunt resistor can be used for current waveform reconstruction. The higher
    // number of shunt resistors allow the higher duty cycle operation and better
    // dc bus utilization. The users should adjust the PI saturation levels
    // carefully during open loop tests (i.e pi_id.Umax, pi_iq.Umax and Umins) as
    // in project manuals. Violation of this procedure yields distorted  current
    // waveforms and unstable closed loop operations that may damage the inverter.
    // reset some control variables for motor_1
    resetControlVars(&motorVars[0]);

    // reset some control variables for motor_2
    resetControlVars(&motorVars[1]);

    // clear any spurious OST & DCAEVT1 flags for motor_1
    HAL_clearTZFlag(halMtrHandle[MTR_1]);

    // clear any spurious OST & DCAEVT1 flags for motor_2
    HAL_clearTZFlag(halMtrHandle[MTR_2]);

    // Clear LED counter
    led1Cnt = 0;
    led2Cnt = 0;

    // *************** SFRA & SFRA_GUI COMM INIT CODE START *******************
#if BUILDLEVEL == FCL_LEVEL6
    // ************************************************************************
    // NOTE:
    // =====
    // In configureSFRA() below, use 'SFRA_GUI_PLOT_GH_H' to get open loop and
    // plant Bode plots using SFRA_GUI and open loop and closed loop Bode plots
    // using SFRA_GUI_MC. 'SFRA_GUI_PLOT_GH_CL' gives same plots for both GUIs.
    // The CL plot inferences shown in SFRA_GUI is not according to
    // NEMA ICS16 or GBT-16439-2009, so it is not recommended for bandwidth
    // determination purposes in servo drive evaluations. Use SFRA_GUI_MC for
    // that. Recommended to use the default setting (SFRA_GUI_PLOT_GH_H).
    // ************************************************************************
    //
    // configure the SFRA module. SFRA module and settings found in
    // sfra_gui.c/.h
    //
#if SFRA_MOTOR == MOTOR_1
    // Plot GH & H plots using SFRA_GUI, GH & CL plots using SFRA_GUI_MC
    configureSFRA(SFRA_GUI_PLOT_GH_H, M1_SAMPLING_FREQ);
#endif

#if SFRA_MOTOR == MOTOR_2
    // Plot GH & H plots using SFRA_GUI, GH & CL plots using SFRA_GUI_MC
    configureSFRA(SFRA_GUI_PLOT_GH_H, M2_SAMPLING_FREQ);
#endif

#endif
    // **************** SFRA & SFRA_GUI COMM INIT CODE END ********************

    // Tasks State-machine init
    Alpha_State_Ptr = &A0;
    A_Task_Ptr = &A1;
    B_Task_Ptr = &B1;
    C_Task_Ptr = &C1;

    // Set up the initialization value for some variables
    motorVars[0].IdRef_start = 0.2;
    motorVars[0].IqRef = 0.1;
    motorVars[0].speedRef = 0.1;
    motorVars[0].lsw1Speed = 0.02;

    motorVars[0].posPtr = 0;
    motorVars[0].posPtrMax = posPtrMax;
    motorVars[0].posCntrMax = 5000;
    motorVars[0].posSlewRate =  0.001;
    motorVars[0].fclClrCntr = 1;

    motorVars[1].IdRef_start = 0.2;
    motorVars[1].IqRef = 0.1;
    motorVars[1].speedRef = 0.1;
    motorVars[1].lsw1Speed = 0.02;

    motorVars[1].posPtr = 0;
    motorVars[1].posPtrMax = posPtrMax;
    motorVars[1].posCntrMax = 5000;
    motorVars[1].posSlewRate =  0.001;
    motorVars[1].fclClrCntr = 1;

//
// Initialize Datalog module for motor 1 or motor 2
//
    DLOG_4CH_F_init(&dlog_4ch1);
    dlog_4ch1.input_ptr1 = &dlogCh1;    //data value
    dlog_4ch1.input_ptr2 = &dlogCh2;
    dlog_4ch1.input_ptr3 = &dlogCh3;
    dlog_4ch1.input_ptr4 = &dlogCh4;
    dlog_4ch1.output_ptr1 = &DBUFF_4CH1[0];
    dlog_4ch1.output_ptr2 = &DBUFF_4CH2[0];
    dlog_4ch1.output_ptr3 = &DBUFF_4CH3[0];
    dlog_4ch1.output_ptr4 = &DBUFF_4CH4[0];
    dlog_4ch1.size = 200;
    dlog_4ch1.pre_scalar = 5;
    dlog_4ch1.trig_value = 0.01;
    dlog_4ch1.status = 2;

#ifdef _FLASH
    enableFlag = true;

    flagSyncRun = true;
    ctrlState = CTRL_STOP;
#endif

    // Waiting for enable flag set
    while(enableFlag == false)
    {
        backTicker++;
    }
    //find out the FCL SW version information
    while(FCL_getSwVersion() != 0x00000008)
    {
        backTicker++;
    }

    // Configure interrupt for motor_1
    HAL_setupInterrupts(halMtrHandle[MTR_1]);

    // Configure interrupt for motor_2
    HAL_setupInterrupts(halMtrHandle[MTR_2]);

    // current feedback offset calibration for motor_1
    runOffsetsCalculation(&motorVars[0]);

    // current feedback offset calibration for motor_1
    // runOffsetsCalculation(&motorVars[1]);

    // Configure interrupt for motor_1
    HAL_enableInterrupts(halMtrHandle[MTR_1]);

    // Configure interrupt for motor_2
    // HAL_enableInterrupts(halMtrHandle[MTR_2]);

    //Clear the latch flag
    motorVars[0].clearTripFlagDMC = 1;
    // motorVars[1].clearTripFlagDMC = 1;

    // Disable Driver Gate
    GPIO_writePin(motorVars[0].drvEnableGateGPIO, 1);

    // Disable Driver Gate
    // GPIO_writePin(motorVars[1].drvEnableGateGPIO, 1);

    // enable global interrupt
    EINT;          // Enable Global interrupt INTM

    ERTM;          // Enable Global realtime interrupt DBGM

    //
    // Initializations COMPLETE
    //  - IDLE loop. Just loop forever
    //
    for(;;)  //infinite loop
    {
        // State machine entry & exit point
        //===========================================================
        (*Alpha_State_Ptr)();   // jump to an Alpha state (A0,B0,...)
        //===========================================================

        runSyncControl();
    }
} //END MAIN CODE

//=============================================================================
//  STATE-MACHINE SEQUENCING AND SYNCRONIZATION FOR SLOW BACKGROUND TASKS
//=============================================================================

//--------------------------------- FRAMEWORK ---------------------------------
void A0(void)
{
    // loop rate synchronizer for A-tasks
    if(CPUTimer_getTimerOverflowStatus(CPUTIMER0_BASE))
    {
        CPUTimer_clearOverflowFlag(CPUTIMER0_BASE);  // clear flag

        //-----------------------------------------------------------
        (*A_Task_Ptr)();        // jump to an A Task (A1,A2,A3,...)
        //-----------------------------------------------------------

        vTimer0[0]++;           // virtual timer 0, instance 0 (spare)
        serialCommsTimer++;
    }

    Alpha_State_Ptr = &B0;      // Comment out to allow only A tasks
}

void B0(void)
{
    // loop rate synchronizer for B-tasks
    if(CPUTimer_getTimerOverflowStatus(CPUTIMER1_BASE))
    {
        CPUTimer_clearOverflowFlag(CPUTIMER1_BASE);  // clear flag

        //-----------------------------------------------------------
        (*B_Task_Ptr)();        // jump to a B Task (B1,B2,B3,...)
        //-----------------------------------------------------------
        vTimer1[0]++;           // virtual timer 1, instance 0 (spare)
    }

    Alpha_State_Ptr = &C0;      // Allow C state tasks
}

void C0(void)
{
    // loop rate synchronizer for C-tasks
    if(CPUTimer_getTimerOverflowStatus(CPUTIMER2_BASE))
    {
        CPUTimer_clearOverflowFlag(CPUTIMER2_BASE);  // clear flag

        //-----------------------------------------------------------
        (*C_Task_Ptr)();        // jump to a C Task (C1,C2,C3,...)
        //-----------------------------------------------------------

        vTimer2[0]++;           //virtual timer 2, instance 0 (spare)
    }

    Alpha_State_Ptr = &A0;  // Back to State A0
}

//==============================================================================
//  A - TASKS (executed in every 50 usec)
//==============================================================================

//--------------------------------------------------------
void A1(void) // SPARE (not used)
//--------------------------------------------------------
{
    // motor_1 running logic control
    runMotorControl(&motorVars[0], halMtrHandle[0]);

    //-------------------
    //the next time CpuTimer0 'counter' reaches Period value go to A2
    A_Task_Ptr = &A2;
    //-------------------
}

//-----------------------------------------------------------------
void A2(void) // SPARE (not used)
//-----------------------------------------------------------------
{
    // motor_2 running logic control [FIX]
    // runMotorControl(&motorVars[1], halMtrHandle[1]);

    //-------------------
    //the next time CpuTimer0 'counter' reaches Period value go to A3
    A_Task_Ptr = &A3;
    //-------------------
}

//-----------------------------------------
void A3(void) // SPARE (not used)
//-----------------------------------------
{
    led1Cnt++;

    if(led1Cnt >= LPD_LED1_WAIT_TIME)
    {
        led1Cnt = 0;

        GPIO_togglePin(LPD_RED_LED1);   // LED
    }


    //-----------------
    //the next time CpuTimer0 'counter' reaches Period value go to A1
    A_Task_Ptr = &A1;
    //-----------------
}

//==============================================================================
//  B - TASKS (executed in every 100 usec)
//==============================================================================

//----------------------------------- USER -------------------------------------

//----------------------------------------
void B1(void) // Toggle GPIO-00
//----------------------------------------
{
#if BUILDLEVEL == FCL_LEVEL6
    //
    // SFRA test
    //
    SFRA_F32_runBackgroundTask(&sfra1);
    SFRA_GUI_runSerialHostComms(&sfra1);

#endif

    //-----------------
    //the next time CpuTimer1 'counter' reaches Period value go to B2
    B_Task_Ptr = &B2;
    //-----------------
}

//----------------------------------------
void B2(void) // SPARE
//----------------------------------------
{
    //-----------------
    //the next time CpuTimer1 'counter' reaches Period value go to B3
    B_Task_Ptr = &B3;
    //-----------------
}

//----------------------------------------
void B3(void) // SPARE
//----------------------------------------
{

    //-----------------
    //the next time CpuTimer1 'counter' reaches Period value go to B1
    B_Task_Ptr = &B1;
    //-----------------
}

//==============================================================================
//  C - TASKS (executed in every 150 usec)
//==============================================================================

//--------------------------------- USER ---------------------------------------

//----------------------------------------
void C1(void)   // Toggle GPIO-34
//----------------------------------------
{
    led2Cnt++;

    if(led2Cnt >= LPD_LED2_WAIT_TIME)
    {
        led2Cnt = 0;

        GPIO_togglePin(LPD_BLUE_LED2);   // LED
    }

    //-----------------
    //the next time CpuTimer2 'counter' reaches Period value go to C2
    C_Task_Ptr = &C2;

    //-----------------

}

//----------------------------------------
void C2(void) // SPARE
//----------------------------------------
{

    //-----------------
    //the next time CpuTimer2 'counter' reaches Period value go to C3
    C_Task_Ptr = &C3;
    //-----------------
}

//-----------------------------------------
void C3(void) // SPARE
//-----------------------------------------
{

    //-----------------
    //the next time CpuTimer2 'counter' reaches Period value go to C1
    C_Task_Ptr = &C1;
    //-----------------
}

//
//   Various Incremental Build levels
//

//****************************************************************************
// INCRBUILD 1
//****************************************************************************
//
#if(BUILDLEVEL == FCL_LEVEL1)

// =============================== FCL_LEVEL 1 =================================
// Level 1 verifies
//  - PWM Generation blocks and DACs
// =============================================================================
// build level 1 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel1_M1)
static inline void buildLevel1_M1(void)
{
// -------------------------------------------------------------------------
// control force angle generation based on 'runMotor'
// -------------------------------------------------------------------------
    if(motorVars[0].runMotor == MOTOR_STOP)
    {
        motorVars[0].rc.TargetValue = 0;
        motorVars[0].rc.SetpointValue = 0;
        motorVars[0].ipark.Ds = 0.0;
        motorVars[0].ipark.Qs = 0.0;
    }
    else
    {
        motorVars[0].rc.TargetValue = motorVars[0].speedRef;
        motorVars[0].ipark.Ds = VdTesting;
        motorVars[0].ipark.Qs = VqTesting;
    }

// -----------------------------------------------------------------------------
// Connect inputs of the RMP module and call the ramp control module
// -----------------------------------------------------------------------------
    fclRampControl(&motorVars[0].rc);

// -----------------------------------------------------------------------------
// Connect inputs of the RAMP GEN module and call the ramp generator module
// -----------------------------------------------------------------------------
    motorVars[0].ptrFCL->rg.Freq = motorVars[0].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[0].ptrFCL->rg);

// -----------------------------------------------------------------------------
// Connect inputs of the INV_PARK module and call the inverse park module
// -----------------------------------------------------------------------------
    motorVars[0].ipark.Sine = __sinpuf32(motorVars[0].ptrFCL->rg.Out);
    motorVars[0].ipark.Cosine = __cospuf32(motorVars[0].ptrFCL->rg.Out);
    runIPark(&motorVars[0].ipark);

// -----------------------------------------------------------------------------
// Position encoder suite module
// -----------------------------------------------------------------------------
    FCL_runQEPWrap_M1(); // to wrap up the CLA functions in library

// ----------------------------------------------------------------------------
//  Measure DC Bus voltage
// ----------------------------------------------------------------------------
    motorVars[0].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// -----------------------------------------------------------------------------
// Connect inputs of the SVGEN_DQ module and call the space-vector gen. module
// -----------------------------------------------------------------------------
    motorVars[0].svgen.Ualpha = motorVars[0].ipark.Alpha;
    motorVars[0].svgen.Ubeta  = motorVars[0].ipark.Beta;
    runSVGenDQ(&motorVars[0].svgen);

// -----------------------------------------------------------------------------
// Computed Duty and Write to CMPA register
// -----------------------------------------------------------------------------
    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[0], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Tc) +
                               M1_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[1], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Ta) +
                               M1_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[2], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Tb) +
                               M1_INV_PWM_HALF_TBPRD));
    return;
}

// build level 1 subroutine for motor_2
#pragma FUNC_ALWAYS_INLINE(buildLevel1_M2)
static inline void buildLevel1_M2(void)
{
// -------------------------------------------------------------------------
// control force angle generation based on 'runMotor'
// -------------------------------------------------------------------------
    if(motorVars[1].runMotor == MOTOR_STOP)
    {
        motorVars[1].rc.TargetValue = 0;
        motorVars[1].rc.SetpointValue = 0;
        motorVars[1].ipark.Ds = 0.0;
        motorVars[1].ipark.Qs = 0.0;
    }
    else
    {
        motorVars[1].ipark.Ds = VdTesting;
        motorVars[1].ipark.Qs = VqTesting;
        motorVars[1].rc.TargetValue = motorVars[1].speedRef;
    }

// -----------------------------------------------------------------------------
// Connect inputs of the RMP module and call the ramp control module
// -----------------------------------------------------------------------------
    fclRampControl(&motorVars[1].rc);

// -----------------------------------------------------------------------------
// Connect inputs of the RAMP GEN module and call the ramp generator module
// -----------------------------------------------------------------------------
    motorVars[1].ptrFCL->rg.Freq = motorVars[1].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[1].ptrFCL->rg);

// -----------------------------------------------------------------------------
// Connect inputs of the INV_PARK module and call the inverse park module
// -----------------------------------------------------------------------------
    motorVars[1].ipark.Sine = __sinpuf32(motorVars[1].ptrFCL->rg.Out);
    motorVars[1].ipark.Cosine = __cospuf32(motorVars[1].ptrFCL->rg.Out);
    runIPark(&motorVars[1].ipark);

// -----------------------------------------------------------------------------
// Position encoder suite module
// -----------------------------------------------------------------------------
    FCL_runQEPWrap_M2(); // to wrap up the CLA functions in library

// ----------------------------------------------------------------------------
//  Measure DC Bus voltage
// ----------------------------------------------------------------------------
    motorVars[1].FCL_params.Vdcbus = getVdc(&motorVars[1]);

// -----------------------------------------------------------------------------
// Connect inputs of the SVGEN_DQ module and call the space-vector gen. module
// -----------------------------------------------------------------------------
    motorVars[1].svgen.Ualpha = motorVars[1].ipark.Alpha;
    motorVars[1].svgen.Ubeta  = motorVars[1].ipark.Beta;
    runSVGenDQ(&motorVars[1].svgen);

// -----------------------------------------------------------------------------
// Computed Duty and Write to CMPA register
// -----------------------------------------------------------------------------
    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[0], EPWM_COUNTER_COMPARE_A,
                    (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Tc) +
                                M2_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[1], EPWM_COUNTER_COMPARE_A,
                    (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Ta) +
                                M2_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[2], EPWM_COUNTER_COMPARE_A,
                    (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Tb) +
                                M2_INV_PWM_HALF_TBPRD));
    return;
}
#endif // (BUILDLEVEL==FCL_LEVEL1)

//
//****************************************************************************
// INCRBUILD 2
//****************************************************************************
//
#if(BUILDLEVEL == FCL_LEVEL2)
// =============================== FCL_LEVEL 2 =================================
// Level 2 verifies
//   - verify inline shunt current sense schemes
//     - analog-to-digital conversion
//   - Current Limit Settings for over current protection
//   - Position sensor interface is taken care by FCL lib using QEP
//     - speed estimation
// =============================================================================
// build level 2 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel2_M1)

static inline void buildLevel2_M1(void)
{
    // -------------------------------------------------------------------------
    // Alignment Routine: this routine aligns the motor to zero electrical
    // angle and in case of QEP also finds the index location and initializes
    // the angle w.r.t. the index location
    // -------------------------------------------------------------------------
    if(motorVars[0].runMotor == MOTOR_STOP)
    {
        motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[0].IdRef = 0;
        motorVars[0].pi_id.ref = motorVars[0].IdRef;

        FCL_resetController(&motorVars[0]);

        motorVars[0].ipark.Ds = 0.0;
        motorVars[0].ipark.Qs = 0.0;
    }
    else if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // for restarting from (runMotor = STOP)
        motorVars[0].rc.TargetValue = 0;
        motorVars[0].rc.SetpointValue = 0;

        // for QEP, spin the motor to find the index pulse
        motorVars[0].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;

        motorVars[0].ipark.Ds = VdTesting;
        motorVars[0].ipark.Qs = VqTesting;
    } // end else if(lsw == ENC_ALIGNMENT)

// ----------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control module
// ----------------------------------------------------------------------------
    if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[0].rc.TargetValue = 0;
    }
    else
    {
        motorVars[0].rc.TargetValue = motorVars[0].speedRef;
    }

    fclRampControl(&motorVars[0].rc);

// ----------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator module
// ----------------------------------------------------------------------------
    motorVars[0].ptrFCL->rg.Freq = motorVars[0].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[0].ptrFCL->rg);

// ----------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5)
//  to (-1,+1). Connect inputs of the CLARKE module and call the clarke
//  transformation module
// ----------------------------------------------------------------------------

    //wait on ADC EOC
    while(ADC_getInterruptStatus(M1_IW_ADC_BASE, ADC_INT_NUMBER1) == 0);

    NOP;    //1 cycle delay for ADC PPB result

    motorVars[0].clarke.As = (float32_t)M1_IFB_V_PPB *
            motorVars[0].FCL_params.adcScale;

    motorVars[0].clarke.Bs = (float32_t)M1_IFB_W_PPB *
            motorVars[0].FCL_params.adcScale;

    runClarke(&motorVars[0].clarke);

// ----------------------------------------------------------------------------
//  Measure DC Bus voltage
// ----------------------------------------------------------------------------
    motorVars[0].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// ----------------------------------------------------------------------------
// Connect inputs of the PARK module and call the park module
// ----------------------------------------------------------------------------
    motorVars[0].park.Alpha  = motorVars[0].clarke.Alpha;
    motorVars[0].park.Beta   = motorVars[0].clarke.Beta;
    motorVars[0].park.Angle  = motorVars[0].ptrFCL->rg.Out;
    motorVars[0].park.Sine   = __sinpuf32(motorVars[0].park.Angle);
    motorVars[0].park.Cosine = __cospuf32(motorVars[0].park.Angle);
    runPark(&motorVars[0].park);

// ----------------------------------------------------------------------------
// Connect inputs of the INV_PARK module and call the inverse park module
// ----------------------------------------------------------------------------
    motorVars[0].ipark.Sine = motorVars[0].park.Sine;
    motorVars[0].ipark.Cosine = motorVars[0].park.Cosine;
    runIPark(&motorVars[0].ipark);

// ----------------------------------------------------------------------------
// Position encoder suite module
// ----------------------------------------------------------------------------
    FCL_runQEPWrap_M1();

    // Position Sensing is performed in CLA
    motorVars[0].posElecTheta = motorVars[0].ptrFCL->qep.ElecTheta;
    motorVars[0].posMechTheta = motorVars[0].ptrFCL->qep.MechTheta;

// ----------------------------------------------------------------------------
// Connect inputs of the SPEED_FR module and call the speed calculation module
// ----------------------------------------------------------------------------
    motorVars[0].speed.ElecTheta = motorVars[0].posElecTheta;
    runSpeedFR(&motorVars[0].speed);

// ----------------------------------------------------------------------------
// Connect inputs of the SVGEN_DQ module and call the space-vector gen. module
// ----------------------------------------------------------------------------
    motorVars[0].svgen.Ualpha = motorVars[0].ipark.Alpha;
    motorVars[0].svgen.Ubeta  = motorVars[0].ipark.Beta;
    runSVGenDQ(&motorVars[0].svgen);

// ----------------------------------------------------------------------------
//  Computed Duty and Write to CMPA register
// ----------------------------------------------------------------------------
    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[0], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Tc) +
                               M1_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[1], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Ta) +
                               M1_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[0].pwmHandle[2], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M1_INV_PWM_HALF_TBPRD * motorVars[0].svgen.Tb) +
                               M1_INV_PWM_HALF_TBPRD));
    return;
}

// build level 2 subroutine for motor_2
#pragma FUNC_ALWAYS_INLINE(buildLevel2_M2)

static inline void buildLevel2_M2(void)
{
    // -------------------------------------------------------------------------
    // Alignment Routine: this routine aligns the motor to zero electrical
    // angle and in case of QEP also finds the index location and initializes
    // the angle w.r.t. the index location
    // -------------------------------------------------------------------------
    if(motorVars[1].runMotor == MOTOR_STOP)
    {
        motorVars[1].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[1].IdRef = 0;
        motorVars[1].pi_id.ref = motorVars[1].IdRef;

        FCL_resetController(&motorVars[1]);

        motorVars[1].ipark.Ds = 0.0;
        motorVars[1].ipark.Qs = 0.0;
    }
    else if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // for restarting from (runMotor = STOP)
        motorVars[1].rc.TargetValue = 0;
        motorVars[1].rc.SetpointValue = 0;

        // for QEP, spin the motor to find the index pulse
        motorVars[1].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;

        motorVars[1].ipark.Ds = VdTesting;
        motorVars[1].ipark.Qs = VqTesting;
    } // end else if(lsw == ENC_ALIGNMENT)

// ----------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control module
// ----------------------------------------------------------------------------
    if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[1].rc.TargetValue = 0;
    }
    else
    {
        motorVars[1].rc.TargetValue = motorVars[1].speedRef;
    }

    fclRampControl(&motorVars[1].rc);

// ----------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator module
// ----------------------------------------------------------------------------
    motorVars[1].ptrFCL->rg.Freq = motorVars[1].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[1].ptrFCL->rg);

// ----------------------------------------------------------------------------
//  Measure phase currents, subtract the offset and normalize from (-0.5,+0.5)
//  to (-1,+1). Connect inputs of the CLARKE module and call the clarke
//  transformation module
// ----------------------------------------------------------------------------

    //wait on ADC EOC
    while(ADC_getInterruptStatus(M2_IW_ADC_BASE, ADC_INT_NUMBER2) == 0);

    NOP;    //1 cycle delay for ADC PPB result

    motorVars[1].clarke.As = (float32_t)M2_IFB_V_PPB *
            motorVars[1].FCL_params.adcScale;

    motorVars[1].clarke.Bs = (float32_t)M2_IFB_W_PPB *
            motorVars[1].FCL_params.adcScale;

    runClarke(&motorVars[1].clarke);

// ----------------------------------------------------------------------------
//  Measure DC Bus voltage using SDFM Filter3
// ----------------------------------------------------------------------------
    motorVars[1].FCL_params.Vdcbus = getVdc(&motorVars[1]);

// ----------------------------------------------------------------------------
// Connect inputs of the PARK module and call the park module
// ----------------------------------------------------------------------------
    motorVars[1].park.Alpha  = motorVars[1].clarke.Alpha;
    motorVars[1].park.Beta   = motorVars[1].clarke.Beta;
    motorVars[1].park.Angle  = motorVars[1].ptrFCL->rg.Out;
    motorVars[1].park.Sine   = __sinpuf32(motorVars[1].park.Angle);
    motorVars[1].park.Cosine = __cospuf32(motorVars[1].park.Angle);
    runPark(&motorVars[1].park);

// ----------------------------------------------------------------------------
// Connect inputs of the INV_PARK module and call the inverse park module
// ----------------------------------------------------------------------------
    motorVars[1].ipark.Sine = motorVars[1].park.Sine;
    motorVars[1].ipark.Cosine = motorVars[1].park.Cosine;
    runIPark(&motorVars[1].ipark);

// ----------------------------------------------------------------------------
// Position encoder suite module
// ----------------------------------------------------------------------------
    FCL_runQEPWrap_M2();

    // Position Sensing is performed in CLA
    motorVars[1].posElecTheta = motorVars[1].ptrFCL->qep.ElecTheta;
    motorVars[1].posMechTheta = motorVars[1].ptrFCL->qep.MechTheta;

// ----------------------------------------------------------------------------
// Connect inputs of the SPEED_FR module and call the speed calculation module
// ----------------------------------------------------------------------------
    motorVars[1].speed.ElecTheta = motorVars[1].posElecTheta;
    runSpeedFR(&motorVars[1].speed);

// ----------------------------------------------------------------------------
// Connect inputs of the SVGEN_DQ module and call the space-vector gen. module
// ----------------------------------------------------------------------------
    motorVars[1].svgen.Ualpha = motorVars[1].ipark.Alpha;
    motorVars[1].svgen.Ubeta  = motorVars[1].ipark.Beta;
    runSVGenDQ(&motorVars[1].svgen);

// ----------------------------------------------------------------------------
//  Computed Duty and Write to CMPA register
// ----------------------------------------------------------------------------
    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[0], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Tc) +
                               M2_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[1], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Ta) +
                               M2_INV_PWM_HALF_TBPRD));

    EPWM_setCounterCompareValue(halMtr[1].pwmHandle[2], EPWM_COUNTER_COMPARE_A,
                   (uint16_t)((M2_INV_PWM_HALF_TBPRD * motorVars[1].svgen.Tb) +
                               M2_INV_PWM_HALF_TBPRD));
    return;
}

#endif // (BUILDLEVEL==FCL_LEVEL2)


//
//****************************************************************************
// INCRBUILD 3
//****************************************************************************
//
#if(BUILDLEVEL == FCL_LEVEL3)
// =============================== FCL_LEVEL 3 ================================
//  Level 3 verifies the dq-axis current regulation performed by PID and speed
//  measurement modules
//  lsw = ENC_ALIGNMENT      : lock the rotor of the motor
//  lsw = ENC_WAIT_FOR_INDEX : close the current loop
//  NOTE:-
//      1. Iq loop is closed using actual QEP angle.
//         Therefore, motor speed races to high speed with lighter load. It is
//         better to ensure the motor is loaded during this test. Otherwise,
//         the motor will run at higher speeds where it can saturate.
//         It may be typically around the rated speed of the motor or higher.
//      2. clarke1.As and clarke1.Bs are not brought out from the FCL library
//         as of library release version 0x02
// ============================================================================

// build level 3 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel3_M1)

static inline void buildLevel3_M1(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M1(&motorVars[0]);
#endif

// ----------------------------------------------------------------------------
// FCL_cycleCount calculations for debug
// customer can remove the below code in final implementation
// ----------------------------------------------------------------------------
    getFCLTime(MTR_1);

// ----------------------------------------------------------------------------
// Measure DC Bus voltage using SDFM Filter3
// ----------------------------------------------------------------------------
    motorVars[0].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// ----------------------------------------------------------------------------
// Fast current loop controller wrapper
// ----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrlWrap_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrlWrap_M1(&motorVars[0]);
#endif

// ----------------------------------------------------------------------------
// Alignment Routine: this routine aligns the motor to zero electrical angle
// and in case of QEP also finds the index location and initializes the angle
// w.r.t. the index location
// ----------------------------------------------------------------------------
    if(motorVars[0].runMotor == MOTOR_STOP)
    {
        motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[0].pi_id.ref = 0;
        motorVars[0].IdRef = 0;
        FCL_resetController(&motorVars[0]);
    }
    else if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // alignment current
        motorVars[0].IdRef = motorVars[0].IdRef_start;  //0.1;

        // set up an alignment and hold time for shaft to settle down
        if(motorVars[0].pi_id.ref >= motorVars[0].IdRef)
        {
            motorVars[0].alignCntr++;

            if(motorVars[0].alignCntr >= motorVars[0].alignCnt)
            {
                motorVars[0].alignCntr  = 0;

                // for QEP, spin the motor to find the index pulse
                motorVars[0].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;
            }
        }

    } // end else if(lsw == ENC_ALIGNMENT)
    else if(motorVars[0].ptrFCL->lsw == ENC_CALIBRATION_DONE)
    {
        motorVars[0].IdRef = motorVars[0].IdRef_run;
    }

// ----------------------------------------------------------------------------
// Connect inputs of the RMP module and call the ramp control module
// ----------------------------------------------------------------------------
    if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[0].rc.TargetValue = 0;
        motorVars[0].rc.SetpointValue = 0;
    }
    else
    {
        motorVars[0].rc.TargetValue = motorVars[0].speedRef;
    }

    fclRampControl(&motorVars[0].rc);

// ----------------------------------------------------------------------------
// Connect inputs of the RAMP GEN module and call the ramp generator module
// ----------------------------------------------------------------------------
    motorVars[0].ptrFCL->rg.Freq = motorVars[0].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[0].ptrFCL->rg);

    motorVars[0].posElecTheta = motorVars[0].ptrFCL->qep.ElecTheta;
    motorVars[0].speed.ElecTheta = motorVars[0].posElecTheta;

    runSpeedFR(&motorVars[0].speed);

// ----------------------------------------------------------------------------
// setup iqref for FCL
// ----------------------------------------------------------------------------
    motorVars[0].ptrFCL->pi_iq.ref =
           (motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 : motorVars[0].IqRef;

// ----------------------------------------------------------------------------
// setup idref for FCL
// ----------------------------------------------------------------------------
    motorVars[0].pi_id.ref =
           ramper(motorVars[0].IdRef, motorVars[0].pi_id.ref, 0.00001);

    return;
}

// build level 3 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel3_M2)

static inline void buildLevel3_M2(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M2(&motorVars[1]);
#endif

// ----------------------------------------------------------------------------
// FCL_cycleCount calculations for debug
// customer can remove the below code in final implementation
// ----------------------------------------------------------------------------
    getFCLTime(MTR_2);

// ----------------------------------------------------------------------------
// Measure DC Bus voltage using SDFM Filter3
// ----------------------------------------------------------------------------
    motorVars[1].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// ----------------------------------------------------------------------------
// Fast current loop controller wrapper
// ----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrlWrap_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrlWrap_M2(&motorVars[1]);
#endif

// ----------------------------------------------------------------------------
// Alignment Routine: this routine aligns the motor to zero electrical angle
// and in case of QEP also finds the index location and initializes the angle
// w.r.t. the index location
// ----------------------------------------------------------------------------
    if(motorVars[1].runMotor == MOTOR_STOP)
    {
        motorVars[1].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[1].pi_id.ref = 0;
        motorVars[1].IdRef = 0;
        FCL_resetController(&motorVars[1]);

        // motorVars[1].state |= 0x8000; // FIXME
    }
    else if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // alignment current
        motorVars[1].IdRef = motorVars[1].IdRef_start;  //0.1;

        // motorVars[1].state |= 0x0001;

        // set up an alignment and hold time for shaft to settle down
        if(motorVars[1].pi_id.ref >= motorVars[1].IdRef)
        {
            // motorVars[1].state |= 0x0002;

            motorVars[1].alignCntr++;

            if(motorVars[1].alignCntr >= motorVars[1].alignCnt)
            {
                motorVars[1].alignCntr  = 0;

                // for QEP, spin the motor to find the index pulse
                motorVars[1].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;

                // motorVars[1].state |= 0x0004;
            }
        }
    } // end else if(lsw == ENC_ALIGNMENT)
    else if(motorVars[1].ptrFCL->lsw == ENC_CALIBRATION_DONE)
    {
        motorVars[1].IdRef = motorVars[1].IdRef_run;

        // motorVars[1].state |= 0x0010;
    }

// ----------------------------------------------------------------------------
// Connect inputs of the RMP module and call the ramp control module
// ----------------------------------------------------------------------------
    if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[1].rc.TargetValue = 0;
        motorVars[1].rc.SetpointValue = 0;
    }
    else
    {
        motorVars[1].rc.TargetValue = motorVars[1].speedRef;

        // motorVars[1].state |= 0x0020;
    }

    fclRampControl(&motorVars[1].rc);

// ----------------------------------------------------------------------------
// Connect inputs of the RAMP GEN module and call the ramp generator module
// ----------------------------------------------------------------------------
    motorVars[1].ptrFCL->rg.Freq = motorVars[1].rc.SetpointValue;
    fclRampGen((RAMPGEN *)&motorVars[1].ptrFCL->rg);

    motorVars[1].posElecTheta = motorVars[1].ptrFCL->qep.ElecTheta;
    motorVars[1].speed.ElecTheta = motorVars[1].posElecTheta;

    runSpeedFR(&motorVars[1].speed);

// ----------------------------------------------------------------------------
// setup iqref for FCL
// ----------------------------------------------------------------------------
    motorVars[1].ptrFCL->pi_iq.ref =
           (motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 : motorVars[1].IqRef;

// ----------------------------------------------------------------------------
// setup idref for FCL
// ----------------------------------------------------------------------------
    motorVars[1].pi_id.ref =
           ramper(motorVars[1].IdRef, motorVars[1].pi_id.ref, 0.00001);

    return;
}

#endif // (BUILDLEVEL==FCL_LEVEL3)

//
//****************************************************************************
// INCRBUILD 4
//****************************************************************************
//
#if((BUILDLEVEL == FCL_LEVEL4) || (BUILDLEVEL == FCL_LEVEL6) )
// =============================== FCL_LEVEL 4 ================================
// Level 4 verifies the speed regulator performed by PID module.
// The system speed loop is closed by using the measured speed as feedback
//  lsw = ENC_ALIGNMENT      : lock the rotor of the motor
//  lsw = ENC_WAIT_FOR_INDEX : - needed only with QEP encoders until first
//                               index pulse
//                             - Loops shown for 'lsw=ENC_CALIBRATION_DONE' are
//                               closed in this stage
//  lsw = ENC_CALIBRATION_DONE      : close speed loop and current loops Id, Iq
//
//  ****************************************************************
//
//  Level 6 verifies the SFRA functions used to verify bandwidth.
//  This demo code uses Level 4 code to perform SFRA analysis on
//  a current loop inside the speed loop
//
// ============================================================================
// build level 4/6 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel46_M1)

static inline void buildLevel46_M1(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M1(&motorVars[0]);
#endif

// ----------------------------------------------------------------------------
// FCL_cycleCount calculations for debug
// customer can remove the below code in final implementation
// ----------------------------------------------------------------------------
    getFCLTime(MTR_1);

// -----------------------------------------------------------------------------
// Measure DC Bus voltage using SDFM Filter3
// ----------------------------------------------------------------------------
    motorVars[0].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// ----------------------------------------------------------------------------
// Fast current loop controller wrapper
// ----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrlWrap_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrlWrap_M1(&motorVars[0]);
#endif

    // ------------------------------------------------------------------------
    // Alignment Routine: this routine aligns the motor to zero electrical
    // angle and in case of QEP also finds the index location and initializes
    // the angle w.r.t. the index location
    // ------------------------------------------------------------------------
    if(motorVars[0].runMotor == MOTOR_RUN)
    {
        if(motorVars[0].ptrFCL->lsw == ENC_CALIBRATION_DONE)
        {
            motorVars[0].IdRef = motorVars[0].IdRef_run;
            motorVars[0].rc.TargetValue = motorVars[0].speedRef;
        }
        else if(motorVars[0].ptrFCL->lsw == ENC_WAIT_FOR_INDEX)
        {
            motorVars[0].rc.TargetValue = motorVars[0].lsw1Speed *
                    (motorVars[0].speedRef > 0 ? 1 : -1);

            // -----------------------------------------------------------------------------
            //  Connect inputs of the RAMP GEN module and call the ramp generator module
            // -----------------------------------------------------------------------------
                motorVars[0].ptrFCL->rg.Freq = motorVars[0].rc.SetpointValue;
                fclRampGen((RAMPGEN *)&motorVars[0].ptrFCL->rg);

        }
        else if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
        {
            motorVars[0].rc.TargetValue = 0;
            motorVars[0].rc.SetpointValue = 0;

            // alignment current
            motorVars[0].IdRef = motorVars[0].IdRef_start;  //(0.1);

            // set up an alignment and hold time for shaft to settle down
            if(motorVars[0].tempIdRef >= motorVars[0].IdRef)
            {
                motorVars[0].alignCntr++;

                if(motorVars[0].alignCntr >= motorVars[0].alignCnt)
                {
                    motorVars[0].alignCntr  = 0;

                    // for QEP, spin the motor to find the index pulse
                    motorVars[0].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;
                }
            }
        } // end else if(lsw == ENC_ALIGNMENT)
    }
    else
    {
        motorVars[0].IdRef = 0;
        motorVars[0].tempIdRef = motorVars[0].IdRef;

        motorVars[0].rc.TargetValue = 0;

        FCL_resetController(&motorVars[0]);
    }

    //
    //  Connect inputs of the RMP module and call the ramp control module
    //
    fclRampControl(&motorVars[0].rc);

// -----------------------------------------------------------------------------
//  Connect inputs of the SPEED_FR module and call the speed calculation module
// -----------------------------------------------------------------------------
    motorVars[0].posElecTheta = motorVars[0].ptrFCL->qep.ElecTheta;
    motorVars[0].posMechTheta = motorVars[0].ptrFCL->qep.MechTheta;
    motorVars[0].speed.ElecTheta = motorVars[0].posElecTheta;
    runSpeedFR(&motorVars[0].speed);

#if((BUILDLEVEL == FCL_LEVEL6) && (SFRA_MOTOR == MOTOR_1))
// -----------------------------------------------------------------------------
//    SFRA collect routine, only to be called after SFRA inject has occurred 1st
// -----------------------------------------------------------------------------
    if(sfraCollectStart)
    {
        collectSFRA(&motorVars[0]);    // Collect noise feedback from loop
    }

// -----------------------------------------------------------------------------
//  SFRA injection
// -----------------------------------------------------------------------------
    injectSFRA();               // create SFRA Noise per 'sfraTestLoop'

    sfraCollectStart = 1;       // enable SFRA data collection
#endif

// -----------------------------------------------------------------------------
//    Connect inputs of the PI module and call the PID speed controller module
// -----------------------------------------------------------------------------
    motorVars[0].speedLoopCount++;

    if(motorVars[0].speedLoopCount >= motorVars[0].speedLoopPrescaler)
    {

#if((BUILDLEVEL == FCL_LEVEL6) && (SFRA_MOTOR == MOTOR_1))
        // SFRA Noise injection in speed loop
        motorVars[0].pid_spd.term.Ref =
                motorVars[0].rc.SetpointValue + sfraNoiseW;
#else       // if(BUILDLEVEL == FCL_LEVEL4)
        motorVars[0].pid_spd.term.Ref =
                motorVars[0].rc.SetpointValue;  //speedRef;
#endif

        motorVars[0].pid_spd.term.Fbk = motorVars[0].speed.Speed;
        runPID(&motorVars[0].pid_spd);

        motorVars[0].speedLoopCount = 0;
    }

    if((motorVars[0].ptrFCL->lsw != ENC_CALIBRATION_DONE) ||
            (motorVars[0].runMotor == MOTOR_STOP))
    {
        motorVars[0].pid_spd.data.d1 = 0;
        motorVars[0].pid_spd.data.d2 = 0;
        motorVars[0].pid_spd.data.i1 = 0;
        motorVars[0].pid_spd.data.ud = 0;
        motorVars[0].pid_spd.data.ui = 0;
        motorVars[0].pid_spd.data.up = 0;
    }

// -----------------------------------------------------------------------------
//    setup iqref and idref for FCL
// -----------------------------------------------------------------------------
#if((BUILDLEVEL == FCL_LEVEL6) && (SFRA_MOTOR == MOTOR_1))
    // SFRA Noise injection in Q axis
    motorVars[0].ptrFCL->pi_iq.ref =
            (motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[0].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[0].IqRef :
                            (motorVars[0].pid_spd.term.Out + sfraNoiseQ);

    // SFRA Noise injection in D axis
    motorVars[0].tempIdRef =
            ramper(motorVars[0].IdRef, motorVars[0].tempIdRef, 0.00001);

    motorVars[0].pi_id.ref = motorVars[0].tempIdRef + sfraNoiseD;
#else   // if(BUILDLEVEL == FCL_LEVEL4)
    // setup iqref
    motorVars[0].ptrFCL->pi_iq.ref =
            (motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[0].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[0].IqRef : motorVars[0].pid_spd.term.Out;

    // setup idref
    motorVars[0].tempIdRef = ramper(motorVars[0].IdRef,
                                    motorVars[0].tempIdRef, 0.00001);
    motorVars[0].pi_id.ref = motorVars[0].tempIdRef;
#endif

   return;
}

// build level 4/6 subroutine for motor_2
#pragma FUNC_ALWAYS_INLINE(buildLevel46_M2)

static inline void buildLevel46_M2(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M2(&motorVars[1]);
#endif

// ----------------------------------------------------------------------------
// FCL_cycleCount calculations for debug
// customer can remove the below code in final implementation
// ----------------------------------------------------------------------------
    getFCLTime(MTR_2);

// -----------------------------------------------------------------------------
// Measure DC Bus voltage using SDFM Filter3
// ----------------------------------------------------------------------------
    motorVars[1].FCL_params.Vdcbus = getVdc(&motorVars[1]);

// ----------------------------------------------------------------------------
// Fast current loop controller wrapper
// ----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrlWrap_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrlWrap_M2(&motorVars[1]);
#endif

    // ------------------------------------------------------------------------
    // Alignment Routine: this routine aligns the motor to zero electrical
    // angle and in case of QEP also finds the index location and initializes
    // the angle w.r.t. the index location
    // ------------------------------------------------------------------------
    if(motorVars[1].runMotor == MOTOR_RUN)
    {
        if(motorVars[1].ptrFCL->lsw == ENC_CALIBRATION_DONE)
        {
            motorVars[1].IdRef = motorVars[1].IdRef_run;
            motorVars[1].rc.TargetValue = motorVars[1].speedRef;
        }
        else if(motorVars[1].ptrFCL->lsw == ENC_WAIT_FOR_INDEX)
        {
            motorVars[1].rc.TargetValue = motorVars[1].lsw1Speed *
                    (motorVars[1].speedRef > 0 ? 1 : -1);

            // -----------------------------------------------------------------------------
            //  Connect inputs of the RAMP GEN module and call the ramp generator module
            // -----------------------------------------------------------------------------
                motorVars[1].ptrFCL->rg.Freq = motorVars[1].rc.SetpointValue;
                fclRampGen((RAMPGEN *)&motorVars[1].ptrFCL->rg);

        }
        else if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
        {
            motorVars[1].rc.TargetValue = 0;
            motorVars[1].rc.SetpointValue = 0;

            // alignment current
            motorVars[1].IdRef = motorVars[1].IdRef_start;  //(0.1);

            // set up an alignment and hold time for shaft to settle down
            if(motorVars[1].tempIdRef >= motorVars[1].IdRef)
            {
                motorVars[1].alignCntr++;

                if(motorVars[1].alignCntr >= motorVars[1].alignCnt)
                {
                    motorVars[1].alignCntr  = 0;

                    // for QEP, spin the motor to find the index pulse
                    motorVars[1].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;
                }
            }
        } // end else if(lsw == ENC_ALIGNMENT)
    }
    else
    {
        motorVars[1].IdRef = 0;
        motorVars[1].tempIdRef = motorVars[1].IdRef;

        motorVars[1].rc.TargetValue = 0;

        FCL_resetController(&motorVars[1]);
    }

// -----------------------------------------------------------------------------
//  Connect inputs of the RMP module and call the ramp control module
// -----------------------------------------------------------------------------
    fclRampControl(&motorVars[1].rc);

// -----------------------------------------------------------------------------
//  Connect inputs of the SPEED_FR module and call the speed calculation module
// -----------------------------------------------------------------------------
    motorVars[1].posElecTheta = motorVars[1].ptrFCL->qep.ElecTheta;
    motorVars[1].posMechTheta = motorVars[1].ptrFCL->qep.MechTheta;
    motorVars[1].speed.ElecTheta = motorVars[1].posElecTheta;
    runSpeedFR(&motorVars[1].speed);

#if((BUILDLEVEL == FCL_LEVEL6)  && (SFRA_MOTOR == MOTOR_2))
// -----------------------------------------------------------------------------
//    SFRA collect routine, only to be called after SFRA inject has occurred 1st
// -----------------------------------------------------------------------------
    if(sfraCollectStart)
    {
        collectSFRA(&motorVars[1]);    // Collect noise feedback from loop
    }

// -----------------------------------------------------------------------------
//  SFRA injection
// -----------------------------------------------------------------------------
    injectSFRA();               // create SFRA Noise per 'sfraTestLoop'
    sfraCollectStart = 1;       // enable SFRA data collection
#endif

// -----------------------------------------------------------------------------
//    Connect inputs of the PI module and call the PID speed controller module
// -----------------------------------------------------------------------------
    motorVars[1].speedLoopCount++;

    if(motorVars[1].speedLoopCount >= motorVars[1].speedLoopPrescaler)
    {

#if((BUILDLEVEL == FCL_LEVEL6) && (SFRA_MOTOR == MOTOR_2))
        // SFRA Noise injection in speed loop
        motorVars[1].pid_spd.term.Ref =
                motorVars[1].rc.SetpointValue + sfraNoiseW;
#else   // #if(BUILDLEVEL == FCL_LEVEL4)
        motorVars[1].pid_spd.term.Ref =
                motorVars[1].rc.SetpointValue;  //speedRef;
#endif

        motorVars[1].pid_spd.term.Fbk = motorVars[1].speed.Speed;
        runPID(&motorVars[1].pid_spd);

        motorVars[1].speedLoopCount = 0;
    }

    if((motorVars[1].ptrFCL->lsw != ENC_CALIBRATION_DONE) ||
            (motorVars[1].runMotor == MOTOR_STOP))
    {
        motorVars[1].pid_spd.data.d1 = 0;
        motorVars[1].pid_spd.data.d2 = 0;
        motorVars[1].pid_spd.data.i1 = 0;
        motorVars[1].pid_spd.data.ud = 0;
        motorVars[1].pid_spd.data.ui = 0;
        motorVars[1].pid_spd.data.up = 0;
    }

// -----------------------------------------------------------------------------
//    setup iqref and idref for FCL
// -----------------------------------------------------------------------------
#if((BUILDLEVEL == FCL_LEVEL6) && (SFRA_MOTOR == MOTOR_2))
    // SFRA Noise injection in Q axis
    motorVars[1].ptrFCL->pi_iq.ref =
            (motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[1].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[1].IqRef :
                            (motorVars[1].pid_spd.term.Out + sfraNoiseQ);

    // SFRA Noise injection in D axis
    motorVars[1].tempIdRef =
            ramper(motorVars[1].IdRef, motorVars[1].tempIdRef, 0.00001);

    motorVars[1].pi_id.ref = motorVars[1].tempIdRef + sfraNoiseD;
#else   // #if(BUILDLEVEL == FCL_LEVEL4)
    // setup iqref
    motorVars[1].ptrFCL->pi_iq.ref =
            (motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[1].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[1].IqRef : motorVars[1].pid_spd.term.Out;

    // setup idref
    motorVars[1].tempIdRef = ramper(motorVars[1].IdRef,
                                    motorVars[1].tempIdRef, 0.00001);
    motorVars[1].pi_id.ref = motorVars[1].tempIdRef;
#endif

    return;
 }
#endif // ( (BUILDLEVEL==FCL_LEVEL4) || (BUILDLEVEL == FCL_LEVEL6) )

//
//****************************************************************************
// INCRBUILD 5
//****************************************************************************
//
#if(BUILDLEVEL == FCL_LEVEL5)
// =============================== FCL_LEVEL 5 =================================
//  Level 5 verifies the position control
//  Position references generated locally from a posArray
//  lsw = ENC_ALIGNMENT      : lock the rotor of the motor
//  lsw = ENC_WAIT_FOR_INDEX : - needed only with QEP encoders until first
//                               index pulse
//                             - Loops shown for 'lsw=ENC_CALIBRATION_DONE' are
//                               closed in this stage
//  lsw = ENC_CALIBRATION_DONE : close all loops, position/speed/currents(Id/Iq)
//
//    NOTE:-
//       clarke1.As and clarke1.Bs are not brought out from the FCL library
//       as of library release version 0x02
//
// =============================================================================
// build level 5 subroutine for motor_1
#pragma FUNC_ALWAYS_INLINE(buildLevel5_M1)

static inline void buildLevel5_M1(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M1(&motorVars[0]);
#endif

// -----------------------------------------------------------------------------
//    FCL_cycleCount calculations for debug
//    customer can remove the below code in final implementation
// -----------------------------------------------------------------------------
    getFCLTime(MTR_1);

// -----------------------------------------------------------------------------
//  Measure DC Bus voltage using SDFM Filter3
// -----------------------------------------------------------------------------
    motorVars[0].FCL_params.Vdcbus = getVdc(&motorVars[0]);

// -----------------------------------------------------------------------------
// Fast current loop controller wrapper
// -----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
   FCL_runPICtrlWrap_M1(&motorVars[0]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
   FCL_runComplexCtrlWrap_M1(&motorVars[0]);
#endif

// -----------------------------------------------------------------------------
//  Alignment Routine: this routine aligns the motor to zero electrical angle
//  and in case of QEP also finds the index location and initializes the angle
//  w.r.t. the index location
// -----------------------------------------------------------------------------
    if(motorVars[0].runMotor == MOTOR_STOP)
    {
        motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[0].lsw2EntryFlag = 0;
        motorVars[0].alignCntr = 0;
        motorVars[0].posCntr = 0;
        motorVars[0].posPtr = 0;
        motorVars[0].IdRef = 0;
        motorVars[0].pi_id.ref = motorVars[0].IdRef;
        FCL_resetController(&motorVars[0]);
    }
    else if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // alignment curretnt
        motorVars[0].IdRef = motorVars[0].IdRef_start;  //(0.1);

        // for restarting from (runMotor = STOP)
        motorVars[0].rc.TargetValue = 0;
        motorVars[0].rc.SetpointValue = 0;

        // set up an alignment and hold time for shaft to settle down
        if(motorVars[0].pi_id.ref >= motorVars[0].IdRef)
        {
            motorVars[0].alignCntr++;

            if(motorVars[0].alignCntr >= motorVars[0].alignCnt)
            {
                motorVars[0].alignCntr  = 0;

                // for QEP, spin the motor to find the index pulse
                motorVars[0].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;
            }
        }
    } // end else if(lsw == ENC_ALIGNMENT)
    else if(motorVars[0].ptrFCL->lsw == ENC_CALIBRATION_DONE)
    {
        motorVars[0].IdRef = motorVars[0].IdRef_run;
    }

// -----------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator module
// -----------------------------------------------------------------------------
    motorVars[0].ptrFCL->rg.Freq = motorVars[0].speedRef * 0.1;
    fclRampGen((RAMPGEN *)&motorVars[0].ptrFCL->rg);

// -----------------------------------------------------------------------------
//   Connect inputs of the SPEED_FR module and call the speed calculation module
// -----------------------------------------------------------------------------
    motorVars[0].posElecTheta = motorVars[0].ptrFCL->qep.ElecTheta;
    motorVars[0].posMechTheta = motorVars[0].ptrFCL->qep.MechTheta;
    motorVars[0].speed.ElecTheta = motorVars[0].posElecTheta;
    runSpeedFR(&motorVars[0].speed);

// -----------------------------------------------------------------------------
//    Connect inputs of the PID module and call the PID speed controller module
// -----------------------------------------------------------------------------
    motorVars[0].speedLoopCount++;

    if(motorVars[0].speedLoopCount >= motorVars[0].speedLoopPrescaler)
    {
        if(motorVars[0].ptrFCL->lsw == ENC_CALIBRATION_DONE)
        {
            if(!motorVars[0].lsw2EntryFlag)
            {
                motorVars[0].lsw2EntryFlag = 1;
                motorVars[0].rc.TargetValue = motorVars[0].posMechTheta;
                motorVars[0].pi_pos.Fbk = motorVars[0].rc.TargetValue;
                motorVars[0].pi_pos.Ref = motorVars[0].pi_pos.Fbk;
            }
            else
            {
                // ========== reference position setting =========
                // choose between 1 of 2 position commands
                // The user can choose between a position reference table
                // used within refPosGen() or feed it in from rg1.Out
                // Position command read from a table
                motorVars[0].rc.TargetValue =
                        refPosGen(motorVars[0].rc.TargetValue, &motorVars[0]);

                motorVars[0].rc.SetpointValue = motorVars[0].rc.TargetValue -
                             (float32_t)((int32_t)motorVars[0].rc.TargetValue);

                // Rolling in angle within 0 to 1pu
                if(motorVars[0].rc.SetpointValue < 0)
                {
                    motorVars[0].rc.SetpointValue += 1.0;
                }

                motorVars[0].pi_pos.Ref = motorVars[0].rc.SetpointValue;
                motorVars[0].pi_pos.Fbk = motorVars[0].posMechTheta;
            }

            runPIPos(&motorVars[0].pi_pos);

            // speed PI regulator
            motorVars[0].pid_spd.term.Ref = motorVars[0].pi_pos.Out;
            motorVars[0].pid_spd.term.Fbk = motorVars[0].speed.Speed;
            runPID(&motorVars[0].pid_spd);
        }

        motorVars[0].speedLoopCount = 0;
    }

    if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[0].rc.SetpointValue = 0;  // position = 0 deg
        motorVars[0].pid_spd.data.d1 = 0;
        motorVars[0].pid_spd.data.d2 = 0;
        motorVars[0].pid_spd.data.i1 = 0;
        motorVars[0].pid_spd.data.ud = 0;
        motorVars[0].pid_spd.data.ui = 0;
        motorVars[0].pid_spd.data.up = 0;

        motorVars[0].pi_pos.ui = 0;
        motorVars[0].pi_pos.i1 = 0;

        motorVars[0].ptrFCL->rg.Out = 0;
        motorVars[0].lsw2EntryFlag = 0;
    }

// -----------------------------------------------------------------------------
//  Setup iqref for FCL
// -----------------------------------------------------------------------------
    motorVars[0].ptrFCL->pi_iq.ref =
            (motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[0].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[0].IqRef : motorVars[0].pid_spd.term.Out;

// -----------------------------------------------------------------------------
//  Setup idref for FCL
// -----------------------------------------------------------------------------
    motorVars[0].pi_id.ref =
            ramper(motorVars[0].IdRef, motorVars[0].pi_id.ref, 0.00001);

    return;
}

// build level 5 subroutine for motor_2
#pragma FUNC_ALWAYS_INLINE(buildLevel5_M2)

static inline void buildLevel5_M2(void)
{

#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrl_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrl_M2(&motorVars[1]);
#endif

// -----------------------------------------------------------------------------
//    FCL_cycleCount calculations for debug
//    customer can remove the below code in final implementation
// -----------------------------------------------------------------------------
    getFCLTime(MTR_2);

// -----------------------------------------------------------------------------
//  Measure DC Bus voltage using SDFM Filter3
// -----------------------------------------------------------------------------
    motorVars[1].FCL_params.Vdcbus = getVdc(&motorVars[1]);

// -----------------------------------------------------------------------------
// Fast current loop controller wrapper
// -----------------------------------------------------------------------------
#if(FCL_CNTLR ==  PI_CNTLR)
    FCL_runPICtrlWrap_M2(&motorVars[1]);
#endif

#if(FCL_CNTLR ==  CMPLX_CNTLR)
    FCL_runComplexCtrlWrap_M2(&motorVars[1]);
#endif

// -----------------------------------------------------------------------------
//  Alignment Routine: this routine aligns the motor to zero electrical angle
//  and in case of QEP also finds the index location and initializes the angle
//  w.r.t. the index location
// -----------------------------------------------------------------------------
    if(motorVars[1].runMotor == MOTOR_STOP)
    {
        motorVars[1].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[1].lsw2EntryFlag = 0;
        motorVars[1].alignCntr = 0;
        motorVars[1].posCntr = 0;
        motorVars[1].posPtr = 0;
        motorVars[1].IdRef = 0;
        motorVars[1].pi_id.ref = motorVars[1].IdRef;
        FCL_resetController(&motorVars[1]);
    }
    else if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        // alignment curretnt
        motorVars[1].IdRef = motorVars[1].IdRef_start;  //(0.1);

        // for restarting from (runMotor = STOP)
        motorVars[1].rc.TargetValue = 0;
        motorVars[1].rc.SetpointValue = 0;

        // set up an alignment and hold time for shaft to settle down
        if(motorVars[1].pi_id.ref >= motorVars[1].IdRef)
        {
            motorVars[1].alignCntr++;

            if(motorVars[1].alignCntr >= motorVars[1].alignCnt)
            {
                motorVars[1].alignCntr  = 0;

                // for QEP, spin the motor to find the index pulse
                motorVars[1].ptrFCL->lsw = ENC_WAIT_FOR_INDEX;
            }
        }
    } // end else if(lsw == ENC_ALIGNMENT)
    else if(motorVars[1].ptrFCL->lsw == ENC_CALIBRATION_DONE)
    {
        motorVars[1].IdRef = motorVars[1].IdRef_run;
    }

// -----------------------------------------------------------------------------
//  Connect inputs of the RAMP GEN module and call the ramp generator module
// -----------------------------------------------------------------------------
    motorVars[1].ptrFCL->rg.Freq = motorVars[1].speedRef * 0.1;
    fclRampGen((RAMPGEN *)&motorVars[1].ptrFCL->rg);

// -----------------------------------------------------------------------------
//  Connect inputs of the SPEED_FR module and call the speed calculation module
// -----------------------------------------------------------------------------
    motorVars[1].posElecTheta = motorVars[1].ptrFCL->qep.ElecTheta;
    motorVars[1].posMechTheta = motorVars[1].ptrFCL->qep.MechTheta;
    motorVars[1].speed.ElecTheta = motorVars[1].posElecTheta;
    runSpeedFR(&motorVars[1].speed);

// -----------------------------------------------------------------------------
//    Connect inputs of the PID module and call the PID speed controller module
// -----------------------------------------------------------------------------
    motorVars[1].speedLoopCount++;

    if(motorVars[1].speedLoopCount >= motorVars[1].speedLoopPrescaler)
    {
        if(motorVars[1].ptrFCL->lsw == ENC_CALIBRATION_DONE)
        {
            if(!motorVars[1].lsw2EntryFlag)
            {
                motorVars[1].lsw2EntryFlag = 1;
                motorVars[1].rc.TargetValue = motorVars[1].posMechTheta;
                motorVars[1].pi_pos.Fbk = motorVars[1].rc.TargetValue;
                motorVars[1].pi_pos.Ref = motorVars[1].pi_pos.Fbk;
            }
            else
            {
                // ========== reference position setting =========
#if(BUILDLEVEL == FCL_LEVEL5)
                // choose between 1 of 2 position commands
                // The user can choose between a position reference table
                // used within refPosGen() or feed it in from rg1.Out
                // Position command read from a table
                motorVars[1].rc.TargetValue =
                        refPosGen(motorVars[1].rc.TargetValue, &motorVars[1]);

#endif

                motorVars[1].rc.SetpointValue = motorVars[1].rc.TargetValue -
                             (float32_t)((int32_t)motorVars[1].rc.TargetValue);

                // Rolling in angle within 0 to 1pu
                if(motorVars[1].rc.SetpointValue < 0)
                {
                    motorVars[1].rc.SetpointValue += 1.0;
                }

                motorVars[1].pi_pos.Ref = motorVars[1].rc.SetpointValue;
                motorVars[1].pi_pos.Fbk = motorVars[1].posMechTheta;
            }

            runPIPos(&motorVars[1].pi_pos);

            // speed PI regulator
            motorVars[1].pid_spd.term.Ref = motorVars[1].pi_pos.Out;
            motorVars[1].pid_spd.term.Fbk = motorVars[1].speed.Speed;
            runPID(&motorVars[1].pid_spd);
        }

        motorVars[1].speedLoopCount = 0;
    }

    if(motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        motorVars[1].rc.SetpointValue = 0;  // position = 0 deg
        motorVars[1].pid_spd.data.d1 = 0;
        motorVars[1].pid_spd.data.d2 = 0;
        motorVars[1].pid_spd.data.i1 = 0;
        motorVars[1].pid_spd.data.ud = 0;
        motorVars[1].pid_spd.data.ui = 0;
        motorVars[1].pid_spd.data.up = 0;
        motorVars[1].pi_pos.ui = 0;
        motorVars[1].pi_pos.i1 = 0;
        motorVars[1].ptrFCL->rg.Out = 0;
        motorVars[1].lsw2EntryFlag = 0;
    }

// -----------------------------------------------------------------------------
//  Setup iqref for FCL
// -----------------------------------------------------------------------------
    motorVars[1].ptrFCL->pi_iq.ref =
            (motorVars[1].ptrFCL->lsw == ENC_ALIGNMENT) ? 0 :
                    (motorVars[1].ptrFCL->lsw == ENC_WAIT_FOR_INDEX) ?
                            motorVars[1].IqRef : motorVars[1].pid_spd.term.Out;

// -----------------------------------------------------------------------------
//  Setup idref for FCL
// -----------------------------------------------------------------------------
    motorVars[1].pi_id.ref =
            ramper(motorVars[1].IdRef, motorVars[1].pi_id.ref, 0.00001);

    return;
}
#endif // (BUILDLEVEL==FCL_LEVEL5)

// ****************************************************************************
// ****************************************************************************
// Motor Control ISR
// ****************************************************************************
// ****************************************************************************

#pragma CODE_ALIGN(motor1ControlISR, 2)
// motor1ControlISR
__interrupt void motor1ControlISR(void)
{

#if(BUILDLEVEL == FCL_LEVEL1)
    buildLevel1_M1();

// -----------------------------------------------------------------------------
// Connect inputs of the DATALOG module
// -----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].ptrFCL->rg.Out;
    dlogCh2 = motorVars[0].svgen.Ta;
    dlogCh3 = motorVars[0].svgen.Tb;
    dlogCh4 = motorVars[0].svgen.Tc;

#ifdef DACOUT_EN
//------------------------------------------------------------------------------
// Variable display on DACs
//------------------------------------------------------------------------------
    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].svgen.Ta));
    DAC_setShadowValue(hal.dacHandle[1],
                       DAC_MACRO_PU(motorVars[0].svgen.Tb));
#endif   // DACOUT_EN

#elif(BUILDLEVEL == FCL_LEVEL2)
    buildLevel2_M1();

// ----------------------------------------------------------------------------
//    Connect inputs of the DATALOG module
// ----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].ptrFCL->rg.Out;
    dlogCh2 = motorVars[0].speed.ElecTheta;
    dlogCh3 = motorVars[0].clarke.As;
    dlogCh4 = motorVars[0].clarke.Bs;

#ifdef DACOUT_EN
//-----------------------------------------------------------------------------
// Variable display on DACs
//-----------------------------------------------------------------------------
    // DAC_setShadowValue(hal.dacHandle[0],
    //                    DAC_MACRO_PU(motorVars[0].ptrFCL->rg.Out));
    // DAC_setShadowValue(hal.dacHandle[1],
    //                    DAC_MACRO_PU(motorVars[0].posElecTheta));

    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].svgen.Ta));
    DAC_setShadowValue(hal.dacHandle[1],
                       DAC_MACRO_PU(motorVars[0].svgen.Tb));
#endif   // DACOUT_EN

#elif(BUILDLEVEL == FCL_LEVEL3)
    buildLevel3_M1();

// ----------------------------------------------------------------------------
// Connect inputs of the DATALOG module
// ----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].posElecTheta;
    dlogCh2 = motorVars[0].ptrFCL->rg.Out;
    dlogCh3 = motorVars[0].ptrFCL->pi_iq.ref;
    dlogCh4 = motorVars[0].ptrFCL->pi_iq.fbk;

#ifdef DACOUT_EN
//-----------------------------------------------------------------------------
// Variable display on DACs
//-----------------------------------------------------------------------------
    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].ptrFCL->pi_iq.ref));
    DAC_setShadowValue(hal.dacHandle[1],
                       DAC_MACRO_PU(motorVars[0].ptrFCL->pi_iq.fbk));
#endif   // DACOUT_EN

#elif(BUILDLEVEL == FCL_LEVEL4)
    buildLevel46_M1();

// -----------------------------------------------------------------------------
//    Connect inputs of the DATALOG module
// -----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].posElecTheta;
    dlogCh2 = motorVars[0].speed.Speed;
    dlogCh3 = motorVars[1].posElecTheta;
    dlogCh4 = motorVars[1].speed.Speed;

#ifdef DACOUT_EN
//------------------------------------------------------------------------------
// Variable display on DACs
//------------------------------------------------------------------------------
   DAC_setShadowValue(hal.dacHandle[0],
                      DAC_MACRO_PU(motorVars[0].ptrFCL->pi_iq.fbk));
   DAC_setShadowValue(hal.dacHandle[1],
                      DAC_MACRO_PU(motorVars[0].speed.Speed));
#endif   // DACOUT_EN

#elif(BUILDLEVEL == FCL_LEVEL5)
    buildLevel5_M1();

// -----------------------------------------------------------------------------
//  Connect inputs of the DATALOG module
// -----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].pi_pos.Ref;
    dlogCh2 = motorVars[0].pi_pos.Fbk;
    dlogCh3 = motorVars[0].pi_id.fbk;
    dlogCh4 = motorVars[0].ptrFCL->pi_iq.fbk;

#ifdef DACOUT_EN
//------------------------------------------------------------------------------
// Variable display on DACs B and C
//------------------------------------------------------------------------------
    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].pi_pos.Fbk));
    DAC_setShadowValue(hal.dacHandle[1],
                       DAC_MACRO_PU(motorVars[1].pi_pos.Fbk));
#endif   // DACOUT_EN

#elif(BUILDLEVEL == FCL_LEVEL6)
    buildLevel46_M1();

// -----------------------------------------------------------------------------
//    Connect inputs of the DATALOG module
// -----------------------------------------------------------------------------
    dlogCh1 = motorVars[0].posElecTheta;
    dlogCh2 = motorVars[0].speed.Speed;
    dlogCh3 = motorVars[0].pi_id.fbk;
    dlogCh4 = motorVars[0].ptrFCL->pi_iq.fbk;

#ifdef DACOUT_EN
//------------------------------------------------------------------------------
// Variable display on DACs
//------------------------------------------------------------------------------
       DAC_setShadowValue(hal.dacHandle[0],
                          DAC_MACRO_PU(motorVars[0].ptrFCL->pi_iq.fbk));
       DAC_setShadowValue(hal.dacHandle[1],
                          DAC_MACRO_PU(motorVars[0].pi_id.fbk));
#endif   // DACOUT_EN

#endif


// ----------------------------------------------------------------------------
//    Call the DATALOG update function.
// ----------------------------------------------------------------------------
    DLOG_4CH_F_FUNC(&dlog_4ch1);

    // Acknowledges an interrupt
    HAL_ackInt_M1(halMtrHandle[MTR_1]);

    motorVars[0].isrTicker++;

} // motor1ControlISR Ends Here


#pragma CODE_ALIGN(motor2ControlISR, 2)
//  motor2ControlISR()
__interrupt void motor2ControlISR(void)
{

#if(BUILDLEVEL == FCL_LEVEL1)
    buildLevel1_M2();

#elif(BUILDLEVEL == FCL_LEVEL2)
    buildLevel2_M2();

#elif(BUILDLEVEL == FCL_LEVEL3)
    buildLevel3_M2();

#elif(BUILDLEVEL == FCL_LEVEL4)
    buildLevel46_M2();

#elif(BUILDLEVEL == FCL_LEVEL5)
    buildLevel5_M2();

#elif(BUILDLEVEL == FCL_LEVEL6)
    buildLevel46_M2();
#endif


    // Acknowledges an interrupt
    HAL_ackInt_M2(halMtrHandle[MTR_2]);

    motorVars[1].isrTicker++;
} // motor2ControlISR Ends Here

//
// POSITION LOOP UTILITY FUNCTIONS
//

// slew programmable ramper
float32_t ramper(float32_t in, float32_t out, float32_t rampDelta)
{
    float32_t err;

    err = in - out;

    if(err > rampDelta)
    {
        return(out + rampDelta);
    }
    else if(err < -rampDelta)
    {
        return(out - rampDelta);
    }
    else
    {
        return(in);
    }
}

//
// Reference Position Generator for position loop
//
float32_t refPosGen(float32_t out, MOTOR_Vars_t *pMotor)
{
    float32_t in = posArray[pMotor->posPtr];

    out = ramper(in, out, pMotor->posSlewRate);

    if(in == out)
    {
        pMotor->posCntr++;

        if(pMotor->posCntr > pMotor->posCntrMax)
        {
            pMotor->posCntr = 0;

            pMotor->posPtr++;

            if(pMotor->posPtr >= pMotor->posPtrMax)
            {
                pMotor->posPtr = 0;
            }
        }
    }

    return(out);
}

//
// run the motor control
//
void runMotorControl(MOTOR_Vars_t *pMotor, HAL_MTR_Handle mtrHandle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)mtrHandle;

    #ifndef DISABLE_MOTOR_FAULTS
    // *******************************************************
    // Current limit setting / tuning in Debug environment
    // *******************************************************
    pMotor->currentThreshHi = 2048 +
            scaleCurrentValue(pMotor->currentLimit, pMotor->currentInvSF);
    pMotor->currentThreshLo = 2048 -
            scaleCurrentValue(pMotor->currentLimit, pMotor->currentInvSF);

    HAL_setupCMPSS_DACValue(mtrHandle,
                            pMotor->currentThreshHi, pMotor->currentThreshLo);

    pMotor->Vdcbus = (pMotor->Vdcbus * 0.8) + (pMotor->FCL_params.Vdcbus * 0.2);

    if( (pMotor->Vdcbus > pMotor->VdcbusMax) ||
            (pMotor->Vdcbus < pMotor->VdcbusMin) )
    {
        pMotor->tripFlagDMC |= 0x0002;
    }
    else
    {
        pMotor->tripFlagDMC &= (0xFFFF - 0x0002);
    }

    // Check for PWM trip due to over current
    if((EPWM_getTripZoneFlagStatus(obj->pwmHandle[0]) & EPWM_TZ_FLAG_OST) ||
       (EPWM_getTripZoneFlagStatus(obj->pwmHandle[1]) & EPWM_TZ_FLAG_OST) ||
       (EPWM_getTripZoneFlagStatus(obj->pwmHandle[2]) & EPWM_TZ_FLAG_OST))
    {
        // if any EPwm's OST is set, force OST on all three to DISABLE inverter
        EPWM_forceTripZoneEvent(obj->pwmHandle[0], EPWM_TZ_FORCE_EVENT_OST);
        EPWM_forceTripZoneEvent(obj->pwmHandle[1], EPWM_TZ_FORCE_EVENT_OST);
        EPWM_forceTripZoneEvent(obj->pwmHandle[2], EPWM_TZ_FORCE_EVENT_OST);

        // Disable Driver Gate
        GPIO_writePin(pMotor->drvEnableGateGPIO, 1);

        pMotor->tripFlagDMC |= 0x0001;      // over current fault trip
    }

    pMotor->tripFlagPrev |= pMotor->tripFlagDMC;

    if(pMotor->tripFlagDMC != 0)
    {
        pMotor->runMotor = MOTOR_STOP;
        pMotor->ctrlState = CTRL_FAULT;

        // Disable Driver Gate
        GPIO_writePin(pMotor->drvEnableGateGPIO, 1);
    }

    if((pMotor->tripFlagDMC != 0) && (pMotor->clearTripFlagDMC == true))
    {
        pMotor->tripCountDMC++;
    }

    // If clear cmd received, reset PWM trip
    if(pMotor->clearTripFlagDMC == true)
    {
        // clear EPWM trip flags
        DEVICE_DELAY_US(1L);

        // clear OST & DCAEVT1 flags
        EPWM_clearTripZoneFlag(obj->pwmHandle[0],
                               (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

        EPWM_clearTripZoneFlag(obj->pwmHandle[1],
                               (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

        EPWM_clearTripZoneFlag(obj->pwmHandle[2],
                               (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

        //
        // clear HLATCH - (not in TRIP gen path)
        //
        CMPSS_clearFilterLatchHigh(obj->cmpssHandle[0]);
        CMPSS_clearFilterLatchHigh(obj->cmpssHandle[1]);
        CMPSS_clearFilterLatchHigh(obj->cmpssHandle[2]);

        // clear LLATCH - (not in TRIP gen path)
        CMPSS_clearFilterLatchLow(obj->cmpssHandle[0]);
        CMPSS_clearFilterLatchLow(obj->cmpssHandle[1]);
        CMPSS_clearFilterLatchLow(obj->cmpssHandle[2]);

        // clear the ocp
        pMotor->tripFlagDMC = 0;
        pMotor->clearTripFlagDMC = 0;
        pMotor->ctrlState = CTRL_STOP;
        pMotor->ptrFCL->lsw = ENC_ALIGNMENT;
    }
    #endif // DISABLE_MOTOR_FAULTS

    // Gate driver logic runs regardless — kept outside the guard
    // so motor start/stop state machine still functions in bench mode
    if(pMotor->ctrlState == CTRL_RUN)
    {
        if(pMotor->runMotor == MOTOR_STOP)
        {
            pMotor->runMotor = MOTOR_RUN;

            // Enable Driver Gate
            GPIO_writePin(pMotor->drvEnableGateGPIO, 0);
        }
    }
    else
    {
        if(pMotor->runMotor == MOTOR_RUN)
        {
            pMotor->runMotor = MOTOR_STOP;

            // Disable Driver Gate
            GPIO_writePin(pMotor->drvEnableGateGPIO, 1);
        }
    }

    return;
}

//------------------------------------------------------------------------------
// runSyncControl()
void runSyncControl(void)
{
    
    if(flagSyncRun == true)
    {
        if((motorVars[0].tripFlagDMC == 0) && (motorVars[1].tripFlagDMC == 0))
        {

#if(BUILDLEVEL != FCL_LEVEL5)
            motorVars[0].speedRef = speedRef;
            motorVars[1].speedRef = speedRef;
#endif

#if(BUILDLEVEL == FCL_LEVEL3)
            motorVars[0].IdRef_run = IdRef;
            motorVars[1].IdRef_run = IdRef;

            motorVars[0].IqRef = IqRef;
            motorVars[1].IqRef = IqRef;
#endif

            motorVars[0].ctrlState = ctrlState;
            motorVars[1].ctrlState = ctrlState;
        }
        else
        {
            motorVars[0].ctrlState = CTRL_STOP;
            motorVars[1].ctrlState = CTRL_STOP;
            motorVars[0].speedRef = 0.0;
            motorVars[1].speedRef = 0.0;
        }

        if((motorVars[0].runMotor == MOTOR_RUN) &&
                (motorVars[1].runMotor == MOTOR_RUN))
        {
            runMotor = MOTOR_RUN;
        }
        else
        {
            runMotor= MOTOR_STOP;
        }
    }

    return;
}

//*****************************************************************************
//*****************************************************************************
// Build level 6 : SFRA support functions
//*****************************************************************************
//*****************************************************************************
#if(BUILDLEVEL == FCL_LEVEL6)
// *************************************************************************
// Using SFRA tool :
// =================
//      - INJECT noise
//      - RUN the controller
//      - CAPTURE or COLLECT the controller output
// From a controller analysis standpoint, this sequence will reveal the
// output of controller for a given input, and therefore, good for analysis
// *************************************************************************
void injectSFRA(void)
{
    if(sfraTestLoop == SFRA_TEST_D_AXIS)
    {
        sfraNoiseD = SFRA_F32_inject(0.0);
    }
    else if(sfraTestLoop == SFRA_TEST_Q_AXIS)
    {
        sfraNoiseQ = SFRA_F32_inject(0.0);
    }
    else if(sfraTestLoop == SFRA_TEST_SPEEDLOOP)
    {
        sfraNoiseW = SFRA_F32_inject(0.0);
    }

    return;
}

// ****************************************************************************
void collectSFRA(MOTOR_Vars_t *pMotor)
{
    if(sfraTestLoop == SFRA_TEST_D_AXIS)
    {
        SFRA_F32_collect(&pMotor->pi_id.out,
                         &pMotor->pi_id.fbk);
    }
    else if(sfraTestLoop == SFRA_TEST_Q_AXIS)
    {
        SFRA_F32_collect(&pMotor->ptrFCL->pi_iq.out,
                         &pMotor->ptrFCL->pi_iq.fbk);
    }
    else if(sfraTestLoop == SFRA_TEST_SPEEDLOOP)
    {
        SFRA_F32_collect(&pMotor->pid_spd.term.Out,
                         &pMotor->pid_spd.term.Fbk);
    }

    return;
}
#endif

//
// End of Code
//
