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
// TITLE:   dual-axis motor drive on the related kits (not dual-axis anymore tho)
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
#endif

#ifdef __cplusplus
#pragma CODE_SECTION(".TI.ramfunc");
#endif
#endif

//
//  Prototype statements for Local Functions
//
//#pragma INTERRUPT (motor1ControlISR, HPI)
__interrupt void motor1ControlISR(void);

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
MOTOR_Vars_t motorVars[2] = {MOTOR1_DEFAULTS_NO_IU};

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
volatile uint16_t enableFlag = false;

uint16_t backTicker = 0;

uint16_t led1Cnt = 0;
uint16_t led2Cnt = 0;

// Variables for Field Oriented Control
float32_t VdTesting = 0.0;          // Vd reference (pu)
float32_t VqTesting = 0.20;         // Vq reference (pu)

// V/f profile variables for open-loop IPM control
float32_t vfBoost  = 0.03f;   // minimum voltage at zero speed
float32_t vfSlope  = 0.65f;   // V/f ratio (Vq per unit speed)
float32_t vfVqMax  = 0.95f;   // maximum Vq clamp
volatile uint16_t vfEnable = false;  // true = V/f active, false = manual VdTesting/VqTesting

// Desynchronization detection variables
float32_t desyncAngleError = 0.0f;
float32_t desyncThreshold  = 0.25f;  // 0.25 = 90 electrical degrees
volatile uint16_t desyncFlag = false;

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

// system-level control references
float32_t speedRef = 0.02;
float32_t IdRef = 0.0;
float32_t IqRef = 0.10;
uint32_t rampDelayMax = 0;

MotorRunStop_e runMotor = MOTOR_STOP;
CtrlState_e ctrlState = CTRL_STOP;
bool flagSyncRun = true;

volatile uint32_t endatPosRaw = 0;   // raw position word from EnDat encoder
volatile uint16_t endatInitDone = 0; // flag: 1 = init succeeded
volatile uint32_t endatCrcFailCount = 0;

#define ENDAT_OFFSET_CAL_SAMPLE_COUNT  8U

volatile bool endatOffsetCalEnable = false;
volatile bool endatOffsetCalInProgress = false;
volatile bool endatOffsetCalibrated = false;
volatile bool endatOffsetValid = false;
volatile uint16_t endatOffsetCalSampleCount = 0U;
volatile uint32_t endatOffsetCounts = 0U;
volatile float32_t endatOffsetMechPu = 0.0F;
volatile float32_t endatOffsetElecPu = 0.0F;

volatile uint16_t dbg_tzFlag[3] = {0U, 0U, 0U};
volatile uint16_t dbg_tzOstFlag[3] = {0U, 0U, 0U};
volatile uint16_t dbg_cmpssStatus[3] = {0U, 0U, 0U};
volatile uint32_t dbg_gpio24 = 0U;
volatile uint16_t dbg_tripSource = 0U;
volatile uint16_t dbg_xbarFlags = 0U;
volatile uint16_t dbg_adcRawIv = 0U;
volatile uint16_t dbg_adcRawIw = 0U;
volatile uint16_t dbg_curHi = 0U;
volatile uint16_t dbg_curLo = 0U;

static uint32_t sEndatOffsetBaseCounts = 0U;
static uint64_t sEndatOffsetCountSpan = 0ULL;
static int64_t sEndatOffsetAccumDeltaCounts = 0LL;

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


    // Disable sync(Freeze clock to PWM as well). GTBCLKSYNC is applicable
    // only for multiple core devices. Uncomment the below statement if
    // applicable.
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // set the driver parameters
    HAL_setParams(halHandle);

    // set the driver parameters for motor 1
    HAL_setMotorParams(halMtrHandle[MTR_1]);


    // Enable sync and clock to PWM
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // initialize motor parameters for motor_1
    initMotorParameters(&motorVars[0], halMtrHandle[0]);


    // initialize motor control variables for motor_1
    initControlVars(&motorVars[0]);


    motorVars[0].currentLimit = M1_MAXIMUM_CURRENT;


    #ifndef DISABLE_MOTOR_FAULTS
    // setup faults protection for motor_1
    HAL_setupMotorFaultProtection(halMtrHandle[MTR_1],
                                  motorVars[MTR_1].currentLimit);

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


    // clear any spurious OST & DCAEVT1 flags for motor_1
    HAL_clearTZFlag(halMtrHandle[MTR_1]);


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
    motorVars[0].speedRef = 0.05;  // conservative start for IPM
    motorVars[0].lsw1Speed = 0.02;

    motorVars[0].posPtr = 0;
    motorVars[0].posPtrMax = posPtrMax;
    motorVars[0].posCntrMax = 5000;
    motorVars[0].posSlewRate =  0.001;
    motorVars[0].fclClrCntr = 1;


//
// Initialize Datalog module for motor 1
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

    #ifndef ENDAT_HACK
    // Waiting for enable flag set
    while(enableFlag == false)
    {
        backTicker++;
    }
    #endif
    //find out the FCL SW version information
    while(FCL_getSwVersion() != 0x00000008)
    {
        backTicker++;
    }

    // Configure interrupt for motor_1
    HAL_setupInterrupts(halMtrHandle[MTR_1]);


    // current feedback offset calibration for motor_1
    runOffsetsCalculation(&motorVars[0]);


    // EnDat encoder initialization test

#ifndef DISABLE_ENDAT
    EnDat_Init();
    endat21_runCommandSet();      // exercises basic EnDat 2.1 command set
#if(ENCODER_TYPE == 22)
    endat22_setupAddlData();      // configure 2 additional data words
#endif
    EnDat_initDelayComp();        // cable propagation delay calibration
    PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER);
    DELAY_US(800L);
    endat21_initProducer(motorVars[0].ptrFCL->qep.PolePairs);
    endatInitDone = 1;
    endat21_startProducer();
#endif

    // Configure interrupt for motor_1
    HAL_enableInterrupts(halMtrHandle[MTR_1]);


    //Clear the latch flag
    motorVars[0].clearTripFlagDMC = 1;

    // Disable Driver Gate
    GPIO_writePin(motorVars[0].drvEnableGateGPIO, DISABLE_GATE);


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
        // GPIO_togglePin(motorVars[0].drvEnableGateGPIO);   // LED
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

static inline void syncEndatAngleOffsetFromMotor(MOTOR_Vars_t *pMotor);

static inline bool updateMotorPositionFeedback(MOTOR_Num_e motorNum)
{
    EndatPositionSample sample = {0};
    MOTOR_Vars_t *pMotor = &motorVars[motorNum];

    syncEndatAngleOffsetFromMotor(pMotor);

    endatCrcFailCount = gEndatCrcFailCount;

    if((endatInitDone == 0U) || !endat21_getPublishedPosition(&sample))
    {
        // Keep the previous valid position until a new published snapshot exists.
        return false;
    }

    pMotor->posMechTheta = sample.mechThetaPu;
    pMotor->posElecTheta = sample.elecThetaPu;
    pMotor->speed.ElecTheta = sample.elecThetaPu;
    endatPosRaw = sample.rawPosition;

    return true;
}

static inline float32_t endatCountsToMechThetaPu(uint32_t rawCounts)
{
    uint64_t maxCount;

    if(gEndatRuntimeState.positionClocks == 0U)
    {
        return 0.0F;
    }

    maxCount = (gEndatRuntimeState.positionClocks >= 32U) ?
            4294967296ULL :
            (1ULL << gEndatRuntimeState.positionClocks);

    return (float32_t)rawCounts / (float32_t)maxCount;
}

static inline uint64_t endatGetCountSpan(uint16_t positionClocks)
{
    if(positionClocks == 0U)
    {
        return 0ULL;
    }

    return (positionClocks >= 32U) ?
            4294967296ULL :
            (1ULL << positionClocks);
}

static inline uint32_t endatWrapCounts(int64_t count, uint64_t span)
{
    int64_t wrappedCount;

    if(span == 0ULL)
    {
        return 0U;
    }

    wrappedCount = count % (int64_t)span;

    if(wrappedCount < 0LL)
    {
        wrappedCount += (int64_t)span;
    }

    return (uint32_t)wrappedCount;
}

static inline int64_t endatShortestDeltaCounts(uint32_t baseCounts,
                                               uint32_t sampleCounts,
                                               uint64_t span)
{
    uint64_t forwardDistance;
    uint64_t reverseDistance;

    if(span == 0ULL)
    {
        return 0LL;
    }

    if(sampleCounts >= baseCounts)
    {
        forwardDistance = (uint64_t)sampleCounts - (uint64_t)baseCounts;
        reverseDistance = span - forwardDistance;
    }
    else
    {
        reverseDistance = (uint64_t)baseCounts - (uint64_t)sampleCounts;
        forwardDistance = span - reverseDistance;
    }

    return (forwardDistance <= reverseDistance) ?
            (int64_t)forwardDistance :
            -(int64_t)reverseDistance;
}

static inline void publishEndatAngleOffset(MOTOR_Vars_t *pMotor,
                                           uint32_t offsetCounts)
{
    pMotor->ptrFCL->qep.CalibratedAngle = (int32_t)offsetCounts;
    endat21_setAngleOffsetCounts((int32_t)offsetCounts);

    endatOffsetValid = true;
    endatOffsetCounts = offsetCounts;
    endatOffsetMechPu = endatCountsToMechThetaPu(offsetCounts);
    endatOffsetElecPu = endatOffsetMechPu *
            (float32_t)pMotor->ptrFCL->qep.PolePairs;
    endatOffsetElecPu -= floorf(endatOffsetElecPu);
}

static inline void syncEndatAngleOffsetFromMotor(MOTOR_Vars_t *pMotor)
{
#if defined(DISABLE_ENDAT) || !POSITION_ENCODER_IS_ENDAT
    (void)pMotor;
#else
    if((endatOffsetValid == true) && (gEndatRuntimeState.angleOffsetValid == 0U))
    {
        publishEndatAngleOffset(pMotor,
                (uint32_t)pMotor->ptrFCL->qep.CalibratedAngle);
    }
#endif
}

static inline bool isEndatOffsetCalibrationRequested(void)
{
#if defined(DISABLE_ENDAT) || !POSITION_ENCODER_IS_ENDAT
    return false;
#else
    return ((endatInitDone != 0U) &&
            ((endatOffsetCalEnable == true) || (endatOffsetValid == false)));
#endif
}

static inline void resetEndatOffsetCalibrationWork(MOTOR_Vars_t *pMotor)
{
    (void)pMotor;

    endatOffsetCalInProgress = false;
    endatOffsetCalSampleCount = 0U;
    sEndatOffsetBaseCounts = 0U;
    sEndatOffsetCountSpan = 0ULL;
    sEndatOffsetAccumDeltaCounts = 0LL;
}

static inline void beginEndatOffsetCalibration(MOTOR_Vars_t *pMotor)
{
    if((isEndatOffsetCalibrationRequested() == false) ||
       (endatOffsetCalInProgress == true))
    {
        return;
    }

    endatOffsetCalInProgress = true;
    endatOffsetCalibrated = false;
    endatOffsetCalSampleCount = 0U;
    sEndatOffsetBaseCounts = 0U;
    sEndatOffsetCountSpan = 0ULL;
    sEndatOffsetAccumDeltaCounts = 0LL;
    endatOffsetMechPu = 0.0F;
    endatOffsetElecPu = 0.0F;
    pMotor->alignCntr = 0U;
}

static inline bool runEndatOffsetCalibration(MOTOR_Vars_t *pMotor)
{
#if defined(DISABLE_ENDAT) || !POSITION_ENCODER_IS_ENDAT
    (void)pMotor;
    return false;
#else
    EndatPositionSample sample = {0};
    uint32_t offsetCounts;
    int64_t averageDeltaCounts;

    if(endatOffsetCalInProgress == false)
    {
        return false;
    }

    if(pMotor->alignCntr < pMotor->alignCnt)
    {
        pMotor->alignCntr++;
        return false;
    }

    if(!endat21_getPublishedPosition(&sample))
    {
        return false;
    }

    if(endatOffsetCalSampleCount == 0U)
    {
        sEndatOffsetBaseCounts = sample.rawPosition;
        sEndatOffsetCountSpan = endatGetCountSpan(gEndatRuntimeState.positionClocks);

        if(sEndatOffsetCountSpan == 0ULL)
        {
            return false;
        }
    }

    sEndatOffsetAccumDeltaCounts += endatShortestDeltaCounts(sEndatOffsetBaseCounts,
                                                             sample.rawPosition,
                                                             sEndatOffsetCountSpan);
    endatOffsetCalSampleCount++;

    if(endatOffsetCalSampleCount < ENDAT_OFFSET_CAL_SAMPLE_COUNT)
    {
        return false;
    }

    averageDeltaCounts = sEndatOffsetAccumDeltaCounts /
            (int64_t)endatOffsetCalSampleCount;
    offsetCounts = endatWrapCounts((int64_t)sEndatOffsetBaseCounts +
            averageDeltaCounts, sEndatOffsetCountSpan);

    publishEndatAngleOffset(pMotor, offsetCounts);

    endatOffsetCalEnable = false;
    endatOffsetCalInProgress = false;
    endatOffsetCalibrated = true;
    pMotor->alignCntr = 0U;
    sEndatOffsetBaseCounts = 0U;
    sEndatOffsetCountSpan = 0ULL;
    sEndatOffsetAccumDeltaCounts = 0LL;

    return true;
#endif
}

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
#if(POSITION_ENCODER == QEP_POS_ENCODER)
    FCL_runQEPWrap_M1(); // to wrap up the CLA functions in library
#endif

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

        // absolute encoders can proceed immediately after alignment, while
        // incremental encoders still need the index-search state.
        motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
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
//  Apply voltage commands: V/f profile or manual VdTesting/VqTesting
// ----------------------------------------------------------------------------
    if(vfEnable)
    {
        float32_t absSpeed = fabsf(motorVars[0].rc.SetpointValue);
        float32_t vqCmd = vfBoost + vfSlope * absSpeed;
        if(vqCmd > vfVqMax) vqCmd = vfVqMax;
        motorVars[0].ipark.Qs = vqCmd;
        motorVars[0].ipark.Ds = VdTesting;
    }
    else
    {
        motorVars[0].ipark.Ds = VdTesting;
        motorVars[0].ipark.Qs = VqTesting;
    }

// ----------------------------------------------------------------------------
// Connect inputs of the INV_PARK module and call the inverse park module
// ----------------------------------------------------------------------------
    motorVars[0].ipark.Sine = motorVars[0].park.Sine;
    motorVars[0].ipark.Cosine = motorVars[0].park.Cosine;
    runIPark(&motorVars[0].ipark);

// ----------------------------------------------------------------------------
// Position encoder suite module
// ----------------------------------------------------------------------------
#if(POSITION_ENCODER == QEP_POS_ENCODER)
    FCL_runQEPWrap_M1();

    // Position Sensing is performed in CLA
    updateMotorPositionFeedback(MTR_1);
#endif

// ----------------------------------------------------------------------------
// Connect inputs of the SPEED_FR module and call the speed calculation module
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
//  Desynchronization detection (diagnostic only, no automatic trip)
// ----------------------------------------------------------------------------
#if !defined(DISABLE_ENDAT)
    {
        float32_t openLoopAngle = motorVars[0].ptrFCL->rg.Out;
        float32_t encoderAngle  = motorVars[0].posElecTheta;
        // Normalize open-loop angle to [0, 1]
        float32_t olNorm = openLoopAngle - floorf(openLoopAngle);
        desyncAngleError = olNorm - encoderAngle;
        // Wrap to [-0.5, 0.5]
        if(desyncAngleError > 0.5f)  desyncAngleError -= 1.0f;
        if(desyncAngleError < -0.5f) desyncAngleError += 1.0f;
        desyncFlag = (fabsf(desyncAngleError) > desyncThreshold) ? true : false;
    }
#endif

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
        resetEndatOffsetCalibrationWork(&motorVars[0]);
        motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[0].pi_id.ref = 0;
        motorVars[0].IdRef = 0;
        FCL_resetController(&motorVars[0]);
    }
    else if(motorVars[0].ptrFCL->lsw == ENC_ALIGNMENT)
    {
        beginEndatOffsetCalibration(&motorVars[0]);
#if(POSITION_ENCODER_NEEDS_ALIGNMENT)
        // alignment current
        motorVars[0].IdRef = motorVars[0].IdRef_start;

        // set up an alignment and hold time for shaft to settle down
        if(motorVars[0].pi_id.ref >= motorVars[0].IdRef)
        {
            motorVars[0].alignCntr++;

            if(motorVars[0].alignCntr >= motorVars[0].alignCnt)
            {
                motorVars[0].alignCntr  = 0;

                // EnDat (absolute) -> ENC_CALIBRATION_DONE immediately
                // QEP (incremental) -> ENC_WAIT_FOR_INDEX
                motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
            }
        }
#else
        if(endatOffsetCalInProgress == true)
        {
            motorVars[0].IdRef = motorVars[0].IdRef_start;

            if(motorVars[0].pi_id.ref >= motorVars[0].IdRef)
            {
                if(runEndatOffsetCalibration(&motorVars[0]))
                {
                    motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
                    motorVars[0].IdRef = motorVars[0].IdRef_run;
                }
            }
        }
        else
        {
            // Absolute encoders already provide rotor position, so skip the
            // rotor-lock alignment hold and enable the current loop directly.
            motorVars[0].alignCntr = 0;
            motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
            motorVars[0].IdRef = motorVars[0].IdRef_run;
        }
#endif
    } // end else if(lsw == ENC_ALIGNMENT)
    else if(motorVars[0].ptrFCL->lsw == ENC_CALIBRATION_DONE)
    {
        // IdRef and IqRef are now user-controllable via debugger
        // (motorVars[0].IdRef_run, motorVars[0].IqRef) or via
        // the global IdRef/IqRef when flagSyncRun == true.
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
    #if(POSITION_ENCODER == QEP_POS_ENCODER)
    updateMotorPositionFeedback(MTR_1);
    #endif

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
            beginEndatOffsetCalibration(&motorVars[0]);
#if(POSITION_ENCODER_NEEDS_ALIGNMENT)
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

                    // absolute encoders can proceed immediately after
                    // alignment, while incremental encoders still need the
                    // index-search state.
                    motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
                }
            }
#else
            if(endatOffsetCalInProgress == true)
            {
                motorVars[0].rc.TargetValue = 0;
                motorVars[0].rc.SetpointValue = 0;
                motorVars[0].IdRef = motorVars[0].IdRef_start;

                if(motorVars[0].tempIdRef >= motorVars[0].IdRef)
                {
                    if(runEndatOffsetCalibration(&motorVars[0]))
                    {
                        motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
                        motorVars[0].IdRef = motorVars[0].IdRef_run;
                        motorVars[0].rc.TargetValue = motorVars[0].speedRef;
                    }
                }
            }
            else
            {
                motorVars[0].alignCntr = 0;
                motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
                motorVars[0].IdRef = motorVars[0].IdRef_run;
                motorVars[0].rc.TargetValue = motorVars[0].speedRef;
            }
#endif
        } // end else if(lsw == ENC_ALIGNMENT)
    }
    else
    {
        resetEndatOffsetCalibrationWork(&motorVars[0]);
        motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
        motorVars[0].alignCntr = 0U;
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
    #if(POSITION_ENCODER == QEP_POS_ENCODER)
    updateMotorPositionFeedback(MTR_1);
    #endif
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
        resetEndatOffsetCalibrationWork(&motorVars[0]);
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
        beginEndatOffsetCalibration(&motorVars[0]);
#if(POSITION_ENCODER_NEEDS_ALIGNMENT)
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

                // absolute encoders can proceed immediately after alignment,
                // while incremental encoders still need the index-search state.
                motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
            }
        }
#else
        if(endatOffsetCalInProgress == true)
        {
            motorVars[0].IdRef = motorVars[0].IdRef_start;
            motorVars[0].rc.TargetValue = 0;
            motorVars[0].rc.SetpointValue = 0;

            if(motorVars[0].pi_id.ref >= motorVars[0].IdRef)
            {
                if(runEndatOffsetCalibration(&motorVars[0]))
                {
                    motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
                    motorVars[0].IdRef = motorVars[0].IdRef_run;
                }
            }
        }
        else
        {
            motorVars[0].alignCntr = 0;
            motorVars[0].ptrFCL->lsw = getPostAlignmentEncoderState();
            motorVars[0].IdRef = motorVars[0].IdRef_run;
        }
#endif
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
    #if(POSITION_ENCODER == QEP_POS_ENCODER)
    updateMotorPositionFeedback(MTR_1);
    #endif
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
#ifndef DISABLE_ENDAT
#if(POSITION_ENCODER == ENDAT_POS_ENCODER)
    updateMotorPositionFeedback(MTR_1);
#endif
#endif

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
    // dlogCh3 = motorVars[0].clarke.As;
    // dlogCh4 = motorVars[0].clarke.Bs;
    dlogCh3 = motorVars[0].park.Ds;
    dlogCh4 = motorVars[0].park.Qs;

#ifdef DACOUT_EN
//-----------------------------------------------------------------------------
// Variable display on DACs
//-----------------------------------------------------------------------------
    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].ptrFCL->rg.Out));
    DAC_setShadowValue(hal.dacHandle[1],
                       DAC_MACRO_PU(motorVars[0].posElecTheta));
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
    // DAC_setShadowValue(hal.dacHandle[0],
    //                    DAC_MACRO_PU(motorVars[0].ptrFCL->pi_iq.ref));
    DAC_setShadowValue(hal.dacHandle[0],
                       DAC_MACRO_PU(motorVars[0].posElecTheta));
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
    dlogCh3 = motorVars[0].ptrFCL->pi_iq.ref;
    dlogCh4 = motorVars[0].ptrFCL->pi_iq.fbk;

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
                       DAC_MACRO_PU(motorVars[0].pi_pos.Ref));
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
    (void)obj;
    float32_t currentLimitClamped = pMotor->currentLimit;
    uint16_t cmpIdx;

    // *******************************************************
    // Current limit setting / tuning in Debug environment
    // *******************************************************
    if(currentLimitClamped < 0.0f)
    {
        currentLimitClamped = 0.0f;
    }
    else if(currentLimitClamped > M1_CURRENT_SENSE_MAX_POS_CURRENT)
    {
        currentLimitClamped = M1_CURRENT_SENSE_MAX_POS_CURRENT;
    }

    pMotor->currentThreshHi = M1_CMPSS_ZERO_COUNT +
    scaleCurrentValue(currentLimitClamped, pMotor->currentInvSF);
    pMotor->currentThreshLo = M1_CMPSS_ZERO_COUNT -
    scaleCurrentValue(currentLimitClamped, pMotor->currentInvSF);
    
    HAL_setupCMPSS_DACValue(mtrHandle,
        pMotor->currentThreshHi, pMotor->currentThreshLo);
        
    pMotor->Vdcbus = (pMotor->Vdcbus * 0.8) + (pMotor->FCL_params.Vdcbus * 0.2);
        
    #ifndef DISABLE_MOTOR_FAULTS
    
    #ifndef DISABLE_BUS_VOLTAGE_CHECK
    if( (pMotor->Vdcbus > pMotor->VdcbusMax) ||
            (pMotor->Vdcbus < pMotor->VdcbusMin) )
    {
        pMotor->tripFlagDMC |= 0x0002;
    }
    else
    {
        pMotor->tripFlagDMC &= (0xFFFF - 0x0002);
    }
    #endif // DISABLE_BUS_VOLTAGE_CHECK

    #ifndef DISABLE_OVERCURRENT_CHECK
    // ---- Persistent debug snapshot (survives past clear, inspect in debugger) ----
    //
    // dbg_tzFlag[n] bits:
    //   0x0002 = TZ_FLAG_CBC    (cycle-by-cycle trip)
    //   0x0004 = TZ_FLAG_OST    (one-shot trip)
    //   0x0008 = TZ_FLAG_DCAEVT1 (digital compare A event 1 — XBAR/CMPSS path)
    //
    // dbg_cmpssStatus[n] bits:
    //   0x0001 = COMPHSTS   (high comparator output, real-time)
    //   0x0002 = COMPHSTSL  (high comparator LATCHED — current exceeded curHi)
    //   0x0100 = COMPLSTS   (low comparator output, real-time)
    //   0x0200 = COMPLSTSL  (low comparator LATCHED)
    //            On this CMPINxN board, COMPL/CTRIPL is the active hardware
    //            OCP path and is configured as the positive current threshold.
    //
    // dbg_cmpssStatus[0] = CMPSS6 (Phase V, ADCINC3)
    // dbg_cmpssStatus[1] = CMPSS3 (Phase W, ADCINB3)
    //
    // dbg_gpio24: 0 = gate-driver nFAULT active, 1 = no fault
    //
    // dbg_tripSource: decoded trip origin
    //   bit 0 = gate-driver fault (GPIO24)
    //   bit 1 = CMPSS6 high (Phase V over-current positive)
    //   bit 2 = CMPSS6 low comparator event (active OCP on CMPIN6N board)
    //   bit 3 = CMPSS3 high (Phase W over-current positive)
    //   bit 4 = CMPSS3 low comparator event (active OCP on CMPIN3N board)
    //
    // Check for PWM trip due to over current
    if((EPWM_getTripZoneFlagStatus(obj->pwmHandle[0]) & EPWM_TZ_FLAG_OST) ||
       (EPWM_getTripZoneFlagStatus(obj->pwmHandle[1]) & EPWM_TZ_FLAG_OST) ||
       (EPWM_getTripZoneFlagStatus(obj->pwmHandle[2]) & EPWM_TZ_FLAG_OST))
    {
        // ---- CAPTURE BEFORE ANYTHING IS CLEARED ----
        dbg_tzFlag[0] = EPWM_getTripZoneFlagStatus(obj->pwmHandle[0]);
        dbg_tzFlag[1] = EPWM_getTripZoneFlagStatus(obj->pwmHandle[1]);
        dbg_tzFlag[2] = EPWM_getTripZoneFlagStatus(obj->pwmHandle[2]);
        dbg_tzOstFlag[0] = EPWM_getOneShotTripZoneFlagStatus(obj->pwmHandle[0]);
        dbg_tzOstFlag[1] = EPWM_getOneShotTripZoneFlagStatus(obj->pwmHandle[1]);
        dbg_tzOstFlag[2] = EPWM_getOneShotTripZoneFlagStatus(obj->pwmHandle[2]);

        dbg_cmpssStatus[0] = 0U;
        dbg_cmpssStatus[1] = 0U;
        dbg_cmpssStatus[2] = 0U;

        for(cmpIdx = 0; cmpIdx < COUNT_CURRENT_PROTECTION_CMPSS; cmpIdx++)
        {
            dbg_cmpssStatus[cmpIdx] = CMPSS_getStatus(obj->cmpssHandle[cmpIdx]);
        }

        dbg_gpio24 = GPIO_readPin(M1_XBAR_INPUT_GPIO);
        dbg_xbarFlags = 0U;
        if(XBAR_getInputFlagStatus(XBAR_INPUT_FLG_INPUT1))
            dbg_xbarFlags |= 0x0001;                        // INPUTXBAR1
        if(XBAR_getInputFlagStatus(XBAR_INPUT_FLG_CMPSS6_CTRIPH))
            dbg_xbarFlags |= 0x0002;                        // CMPSS6 high
        if(XBAR_getInputFlagStatus(XBAR_INPUT_FLG_CMPSS6_CTRIPL))
            dbg_xbarFlags |= 0x0004;                        // CMPSS6 low
        if(XBAR_getInputFlagStatus(XBAR_INPUT_FLG_CMPSS3_CTRIPH))
            dbg_xbarFlags |= 0x0008;                        // CMPSS3 high
        if(XBAR_getInputFlagStatus(XBAR_INPUT_FLG_CMPSS3_CTRIPL))
            dbg_xbarFlags |= 0x0010;                        // CMPSS3 low

        // Capture raw ADC values at time of trip
        dbg_adcRawIv = ADC_readResult(M1_IV_ADCRESULT_BASE, M1_IV_ADC_SOC_NUM);
        dbg_adcRawIw = ADC_readResult(M1_IW_ADCRESULT_BASE, M1_IW_ADC_SOC_NUM);
        dbg_curHi = pMotor->currentThreshHi;
        dbg_curLo = pMotor->currentThreshLo;

        // Decode trip source
        dbg_tripSource = 0;
        if(dbg_gpio24 == 0U)
            dbg_tripSource |= 0x0001;                       // gate-driver fault
        if(dbg_cmpssStatus[0] & 0x0002)
            dbg_tripSource |= 0x0002;                       // CMPSS6 high (Iv+)
        if(dbg_cmpssStatus[0] & 0x0200)
            dbg_tripSource |= 0x0004;                       // CMPSS6 low comparator
        if(dbg_cmpssStatus[1] & 0x0002)
            dbg_tripSource |= 0x0008;                       // CMPSS3 high (Iw+)
        if(dbg_cmpssStatus[1] & 0x0200)
            dbg_tripSource |= 0x0010;                       // CMPSS3 low comparator
        // -------------------------------------------------

        // if any EPwm's OST is set, force OST on all three to DISABLE inverter
        EPWM_forceTripZoneEvent(obj->pwmHandle[0], EPWM_TZ_FORCE_EVENT_OST);
        EPWM_forceTripZoneEvent(obj->pwmHandle[1], EPWM_TZ_FORCE_EVENT_OST);
        EPWM_forceTripZoneEvent(obj->pwmHandle[2], EPWM_TZ_FORCE_EVENT_OST);

        // Disable Driver Gate
        GPIO_writePin(pMotor->drvEnableGateGPIO, DISABLE_GATE);

        pMotor->tripFlagDMC |= 0x0001;      // over current fault trip
    }
    #endif // DISABLE_OVERCURRENT_CHECK

    pMotor->tripFlagPrev |= pMotor->tripFlagDMC;

    if(pMotor->tripFlagDMC != 0)
    {
        pMotor->runMotor = MOTOR_STOP;
        pMotor->ctrlState = CTRL_FAULT;

        // Disable Driver Gate
        GPIO_writePin(pMotor->drvEnableGateGPIO, DISABLE_GATE);
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
        // clear HLATCH/LLATCH - (not in TRIP gen path)
        //
        for(cmpIdx = 0; cmpIdx < COUNT_CURRENT_PROTECTION_CMPSS; cmpIdx++)
        {
            CMPSS_clearFilterLatchHigh(obj->cmpssHandle[cmpIdx]);
            CMPSS_clearFilterLatchLow(obj->cmpssHandle[cmpIdx]);
        }

        // clear XBAR input latches so the next debug snapshot reflects the next
        // actual event instead of accumulated sticky flags
        XBAR_clearInputFlag(XBAR_INPUT_FLG_INPUT1);
        XBAR_clearInputFlag(XBAR_INPUT_FLG_CMPSS6_CTRIPH);
        XBAR_clearInputFlag(XBAR_INPUT_FLG_CMPSS6_CTRIPL);
        XBAR_clearInputFlag(XBAR_INPUT_FLG_CMPSS3_CTRIPH);
        XBAR_clearInputFlag(XBAR_INPUT_FLG_CMPSS3_CTRIPL);

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
            GPIO_writePin(pMotor->drvEnableGateGPIO, ENABLE_GATE);
        }
    }
    else
    {
        if(pMotor->runMotor == MOTOR_RUN)
        {
            pMotor->runMotor = MOTOR_STOP;

            // Disable Driver Gate
            GPIO_writePin(pMotor->drvEnableGateGPIO, DISABLE_GATE);
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
        if(motorVars[0].tripFlagDMC == 0)
        {

#if(BUILDLEVEL != FCL_LEVEL5)
            motorVars[0].speedRef = speedRef;
#endif

#if(BUILDLEVEL == FCL_LEVEL3)
            motorVars[0].IdRef_run = IdRef;

            motorVars[0].IqRef = IqRef;
#endif

            motorVars[0].ctrlState = ctrlState;
        }
        else
        {
            motorVars[0].ctrlState = CTRL_STOP;
            motorVars[0].speedRef = 0.0;
        }

        if(motorVars[0].runMotor == MOTOR_RUN)
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
