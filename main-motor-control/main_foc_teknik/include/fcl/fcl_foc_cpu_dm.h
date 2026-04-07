//#############################################################################
// FILE:   fcl_foc_cpu.h
// TITLE:  Header file to be shared between example and library for CPU data.
//
//  Group:         C2000
// Target Family:  F2837x/F2838x/F28004x/F28002x
//
//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

#ifndef FCL_FOC_CPU_H
#define FCL_FOC_CPU_H

//
// Include project specific include files.
//
#include <math.h>

#include "stdbool.h"
#include "stdint.h"

#if defined(F2837x_DEVICE) || defined(F28004x_DEVICE) || defined(F28002x_DEVICE)
#include "F28x_Project.h"
#else
#include "f28x_project.h"
#endif

#include "device.h"
#include "f28x_bmsk.h"

#include "fcl_enum.h"
#include "fcl_cmplx_ctrl.h"

#include "qep_defs.h"

//===============================================
// peripheral headers used in library
#include "epwm.h"
#include "eqep.h"

#include "rampgen.h"            // Include header for the RAMPGEN object
#include "rmp_cntl.h"           // Include header for the RMPCNTL object
#include "ipark.h"              // Include header for the IPARK object
#include "clarke.h"
#include "park.h"
#include "speed_fr.h"
#include "svgen.h"
#include "pi.h"                 // Include header for the PI  object
#include "pid_grando.h"

#include "speed_observer.h"

// DCL Library header files
#include "DCLF32.h"

#if(SPD_CNTLR == SPD_NLPID_CNTLR)
#include "DCL_NLPID.h"
#endif

#include "dual_axis_servo_drive_settings.h"
#include "dual_axis_servo_drive_user.h"
#include "dual_axis_servo_drive_hal.h"

#ifndef PI
#define PI 3.14159265358979
#endif

#define BITFIELD_MODE       0
#define DRIVERLIB_MODE      1

#define DRIVER_MODULE       DRIVERLIB_MODE  //  BITFIELD_MODE       //

//
// typedefs
//
//
//!  \brief typedefs for motorVars

typedef struct _FCL_Parameters_t_
{
    float32_t   carrierMid;     // Mid point value of carrier count
    float32_t   adcPPBScale;    // ADC conversion scale to pu
    float32_t   adcAlphaScale;  // ADC conversion scale to pu
    float32_t   adcBetaScale;    // ADC conversion scale to pu
    float32_t   cmidsqrt3;      // internal variable

    float32_t   tSamp;          // sampling time
    float32_t   Rd;             // Motor resistance in D axis
    float32_t   Rq;             // Motor resistance in Q axis
    float32_t   Ld;             // Motor inductance in D axis
    float32_t   Lq;             // Motor inductance in Q axis
    float32_t   Vbase;          // Base voltage for the controller
    float32_t   Ibase;          // Base current for the controller
    float32_t   invZbase;       // Inverter Base voltage for the controller
    float32_t   wccD;           // D axis current controller bandwidth
    float32_t   wccQ;           // Q axis current controller bandwidth
    float32_t   Vdcbus;         // DC bus voltage
    float32_t   BemfK;          // Motor Bemf constant
    float32_t   Wbase;          // Controller base frequency (Motor) in rad/sec
} FCL_Parameters_t;

#define FCL_PARS_DEFAULTS {                                                    \
    0, /* carrierMid */                                                        \
    0, /* adcScale */                                                          \
    0, /* cmidsqrt3 */                                                         \
    0, /* tSamp */                                                             \
    0, /* Rd */                                                                \
    0, /* Rq */                                                                \
    0, /* Ld */                                                                \
    0, /* Lq */                                                                \
    0, /* Vbase */                                                             \
    0, /* Ibase */                                                             \
    0, /* wccD */                                                              \
    0, /* wccQ */                                                              \
    0, /* Vdcbus */                                                            \
    0, /* BemfK */                                                             \
    0  /* Wbase */                                                             \
}

//
//!  \brief typedefs for motorVars
//
typedef struct _MOTOR_Vars_t_
{
    uint32_t posCntr;
    uint32_t posCntrMax;
    float32_t posSlewRate;

    float32_t baseFreq;
    float32_t polePairs;
    float32_t posMechScaler;        // Parameter: 0.9999/total count

    float32_t Ts;                   // Samping period (sec)
    float32_t maxModIndex;          // Maximum module index
    float32_t pwmHalfPeriod;        // Maximum module index

    float32_t voltageLimit;         // limit voltage
    float32_t currentLimit;         // limit current
    float32_t speedRefStart;        // startup reference speed
    float32_t speedRefMax;          // maximum reference speed
    float32_t IdRefSet;             // Id reference setting (pu)
    float32_t IqRefSet;             // Iq reference setting (pu)

    float32_t IdRef_start;          // Id reference (pu) for startup
    float32_t IdRef_run;            // Id reference (pu) for running
    float32_t IqRef_start;          // Iq reference (pu) for startup

    float32_t IdRef;                // Id reference (pu)
    float32_t IqRef;                // Iq reference (pu)

    float32_t ctrlIdRef;            // control Id reference (pu)
    float32_t ctrlIqRef;            // control Iq reference (pu)

    float32_t speedRef;             // For Closed Loop tests
    float32_t positionRef;          // For Position Loop tests
    float32_t lsw1Speed;            // initial force rotation speed in search
                                    // of QEP index pulse

    float32_t offset_currentAs;     // offset in current A fbk channel
    float32_t offset_currentBs;     // offset in current B fbk channel
    float32_t offset_currentCs;     // offset in current C fbk channel

    float32_t currentAs;            // phase A
    float32_t currentBs;            // phase B
    float32_t currentCs;            // phase C

    float32_t currentScale;         // current scaling coefficient
    float32_t voltageScale;         // voltage scaling coefficient
    float32_t adcScale;             // ADC scale for current and voltage
    float32_t currentInvSF;         // current inverse scaling coefficient
    float32_t voltageInvSF;         // voltage inverse scaling coefficient

    float32_t posElecTheta;
    float32_t posMechTheta;
    float32_t speedWe;
    float32_t speedWePrev;
    float32_t pangle;               // rotor angle
    float32_t speedWeObs;

    float32_t Vdcbus;               // DC bus voltage
    float32_t VdcbusMax;            // Maximum DC bus voltage
    float32_t VdcbusMin;            // Minimum DC bus voltage

    RAMPGEN rg;                       // a ramp generator to simulate an Angle
    RMPCNTL rc;                       // ramp control
    CLARKE clarke;                    // clarke transform
    PARK park;                        // park transform
    IPARK ipark;                      // inv park transform
    SVGEN svgen;

    SPEED_MEAS_QEP speed;
    SPD_OBSERVER   speedObs;

    PID_CONTROLLER  pid_spd;
    PI_CONTROLLER pi_pos;

    FCL_cmplxCtrl_t cmplx_Id;
    FCL_cmplxCtrl_t cmplx_Iq;
    FCL_Parameters_t FCL_params;

    float32_t fclUpdateLatency_us;     // PWM update latency since sampling
    float32_t focExecutionTime_us;     // FOC execution time since sampling

    uint32_t isrTicker;

    volatile uint32_t *pwmCompA;
    volatile uint32_t *pwmCompB;
    volatile uint32_t *pwmCompC;

    volatile uint32_t curA_PPBRESULT;
    volatile uint32_t curB_PPBRESULT;
    volatile uint32_t curC_PPBRESULT;
    volatile uint32_t volDC_PPBRESULT;

    volatile union ADCINTFLG_REG *pADCIntFlag;
    volatile struct EQEP_REGS *pQEPRegs;  // Aligned to lower 16-bits

    uint32_t curA_resultBase;
    uint32_t curB_resultBase;
    uint32_t curC_resultBase;

    uint32_t pwmBaseU;
    uint32_t pwmBaseV;
    uint32_t pwmBaseW;
    uint32_t adcBaseW;

    ADC_PPBNumber curA_PPBNumber;
    ADC_PPBNumber curB_PPBNumber;
    ADC_PPBNumber curC_PPBNumber;

    ADC_IntNumber adcIntNumber;

    uint16_t  fclClrCntr;
    uint16_t  fclCycleCountMax;
    uint16_t  fclCycleCount;            // FCL Latency variable

    uint16_t  focClrCntr;
    uint16_t  focCycleCountMax;
    uint16_t  focCycleCount;            // FOC execution time variable

    uint16_t speedLoopPrescaler;        // Speed loop pre scalar
    uint16_t speedLoopCount;            // Speed loop counter
    uint16_t alignCntr;
    uint16_t alignCnt;
    uint16_t posPtrMax;
    uint16_t posPtr;

    uint16_t currentThreshHi;
    uint16_t currentThreshLo;

    uint16_t drvEnableGateGPIO;
    uint16_t drvFaultTripGPIO;
    uint16_t drvClearFaultGPIO;

    uint16_t tripCountDMC;              // motor fault times counter
    uint16_t tripFlagDMC;               // motor fault status
    uint16_t tripFlagPrev;              // motor fault prev status

    ENC_Status_e    lsw;
    PWMUpdateMode_e pwmUpdateMode;
    MotorNum_e      motorNum;
    MotorRunStop_e  runMotor;
    CtrlState_e     ctrlState;

    bool clearTripFlagDMC;
    bool lsw2EntryFlag;
    bool offsetDoneFlag;
    bool sfraEnableFlag;
} MOTOR_Vars_t;

extern MOTOR_Vars_t motorVars[2];

// ****************************************************************************
//
// functions
//

extern uint32_t FCL_getSwVersion(void);

extern void FCL_initPWM(MOTOR_Vars_t *pMotor,
                        const uint32_t pwmBaseU,
                        const uint32_t pwmBaseV,
                        const uint32_t pwmBaseW);

extern void FCL_initQEP(MOTOR_Vars_t *pMotor, const uint32_t qepBaseA);

extern void FCL_initADC_2In(MOTOR_Vars_t *pMotor, uint32_t adcBaseW,
                      const uint32_t adcResultBaseV, ADC_PPBNumber adcV_PPBNum,
                      const uint32_t adcResultBaseW, ADC_PPBNumber adcW_PPBNum);

extern void FCL_initADC_3In(MOTOR_Vars_t *pMotor, uint32_t adcBaseW,
                      const uint32_t adcResultBaseU, ADC_PPBNumber adcU_PPBNum,
                      const uint32_t adcResultBaseV, ADC_PPBNumber adcV_PPBNum,
                      const uint32_t adcResultBaseW, ADC_PPBNumber adcW_PPBNum);

//
// static inline functions will be put in header files
//
static inline void FCL_readCount(MOTOR_Vars_t *pMotor)
{
    pMotor->fclCycleCount = EPWM_getTimeBaseCounterValue(pMotor->pwmBaseU);

    return;
}

//
// This function is called to reset the FCL variables and is useful when user
// wants to stop the motor and restart the motor
//
static inline void FCL_resetController(MOTOR_Vars_t *pMotor)
{
    pMotor->cmplx_Id.ref = 0.0;
    pMotor->cmplx_Id.carryOver = 0;
    pMotor->cmplx_Id.out = 0;
    pMotor->cmplx_Id.carryOver = 0;
    pMotor->cmplx_Id.err = 0;
    pMotor->cmplx_Id.xErr = 0;

    pMotor->cmplx_Iq.ref = 0.0;
    pMotor->cmplx_Iq.carryOver = 0;
    pMotor->cmplx_Iq.out = 0;
    pMotor->cmplx_Iq.carryOver = 0;
    pMotor->cmplx_Iq.err = 0;
    pMotor->cmplx_Iq.xErr = 0;

    pMotor->speedWePrev = 0;

    return;
}

//
// CURRENT PI CONTROLLER ROUTINES
//
static inline void FCL_runPI(FCL_cmplxCtrl_t *pPI)
{
    pPI->out += (pPI->err * pPI->Kerr) + pPI->carryOver;
    pPI->out = __fsat(pPI->out, pPI->Umax, pPI->Umin);

    return;
}

//
// Complex control in Fast Current Loop
//
static inline void FCL_runCCSyn(FCL_cmplxCtrl_t *pId, FCL_cmplxCtrl_t *pIq)
{
    float32_t errTemp;

    errTemp = pId->err * pId->cosWTs - pIq->err * pId->sinWTs;
    pId->out += pId->kDirect * errTemp + pId->carryOver;
    pId->out = __fsat(pId->out, pId->Umax, pId->Umin);

    errTemp = pIq->err * pIq->cosWTs + pId->err * pIq->sinWTs;
    pIq->out += pIq->kDirect * errTemp + pIq->carryOver;
    pIq->out = __fsat(pIq->out, pIq->Umax, pIq->Umin);

    return;
}

//-----------------------------------------------------------------------------
// PI CONTROL ROUTINES
//-----------------------------------------------------------------------------
//
// FCL PI controller
//
static inline void FCL_runPICtrl(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    SVGEN     svgen2;

    park1Sine   = __sinpuf32(pMotor->pangle);
    park1Cosine = __cospuf32(pMotor->pangle);

    //
    // CLARKE transformation
    //
#if(DRIVER_MODULE == BITFIELD_MODE)
    while(pMotor->pADCIntFlag->bit.ADCINT1 == 0);
#else
    while(ADC_getInterruptStatus(pMotor->adcBaseW, pMotor->adcIntNumber) == 0);
#endif

//    clarke1Alpha = (float32_t)((int16_t)HWREGH(pMotor->curA_PPBRESULT) *
//            pMotor->FCL_params.adcPPBScale);
//    clarke1Beta  = ((clarke1Alpha +
//                   (2.0F * ((float32_t)((int16_t)HWREGH(pMotor->curB_PPBRESULT))
//                           * pMotor->FCL_params.adcPPBScale))) * ONEbySQRT3);

    clarke1Alpha = (float32_t)((int16_t)HWREGH(pMotor->curA_PPBRESULT) *
            pMotor->FCL_params.adcAlphaScale);
    clarke1Beta  = (clarke1Alpha +
                   (((float32_t)((int16_t)HWREGH(pMotor->curB_PPBRESULT))
                           * pMotor->FCL_params.adcBetaScale))) * ONEbySQRT3;

    // PARK Transformation
    //
    pMotor->cmplx_Iq.err = pMotor->cmplx_Iq.ref -
            ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));

    pMotor->cmplx_Id.err = pMotor->cmplx_Id.ref -
            ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    //
    // PI controllers for Id and Iq
    //
    FCL_runPI(&pMotor->cmplx_Iq);     // Iq loop - PI controller - CPU
    FCL_runPI(&pMotor->cmplx_Id);     // Id loop - PI controller - CPU

    //
    // Inverse Park Transformation
    //
    svgen2.Ualpha = ((pMotor->cmplx_Id.out * park1Cosine) -
            (pMotor->cmplx_Iq.out * park1Sine)) * pMotor->FCL_params.carrierMid;

    svgen2.Ubeta  = ((pMotor->cmplx_Iq.out * park1Cosine) +
            (pMotor->cmplx_Id.out * park1Sine)) * pMotor->FCL_params.cmidsqrt3;

    //
    // PWM pulse width time calculation
    //
    svgen2.Tb = (svgen2.Ubeta - svgen2.Ualpha) / 2;
    svgen2.Tc = svgen2.Tb - svgen2.Ubeta;

    svgen2.tmp2  = __fmax(__fmax(svgen2.Ualpha, svgen2.Tc), svgen2.Tb);
    svgen2.tmp2 += __fmin(__fmin(svgen2.Ualpha, svgen2.Tc), svgen2.Tb);
    svgen2.tmp1  = pMotor->FCL_params.carrierMid - (svgen2.tmp2 / 2);

    //
    // PWM updates
    //
    *(pMotor->pwmCompA) = (uint32_t)(svgen2.Tc + svgen2.tmp1);
    *(pMotor->pwmCompB) = (uint32_t)(svgen2.Ualpha + svgen2.tmp1);
    *(pMotor->pwmCompC) = (uint32_t)(svgen2.Tb + svgen2.tmp1);

    FCL_readCount(pMotor);

    return;
}

//
// Wrap up function to be called by the user app after end of current loop
//
static inline void FCL_runPICtrlWrap(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase = pMotor->FCL_params.Vdcbus * (1.15f / 2.0f);
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //   this wrap function and .CarryOver carries it to the next iteration
    //   Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //   change much between iterations - equation tweaked to fit here
    //

    //
    // Update PI ID parameters
    //
    float32_t wccXinvZb = invZbase * pMotor->FCL_params.wccD;

    pMotor->cmplx_Id.Kp = pMotor->FCL_params.Ld * wccXinvZb;
    pMotor->cmplx_Id.Ki = pMotor->FCL_params.Rd * wccXinvZb *
            pMotor->FCL_params.tSamp;

    float32_t Ki_rev = pMotor->cmplx_Id.Ki / 2;
    pMotor->cmplx_Id.Kerr = Ki_rev + pMotor->cmplx_Id.Kp;
    pMotor->cmplx_Id.KerrOld = Ki_rev - pMotor->cmplx_Id.Kp;

    pMotor->cmplx_Id.carryOver =
            pMotor->cmplx_Id.err * pMotor->cmplx_Id.KerrOld;

    // Update PI IQ parameters
    wccXinvZb = invZbase * pMotor->FCL_params.wccQ;

    pMotor->cmplx_Iq.Kp = pMotor->FCL_params.Lq * wccXinvZb;
    pMotor->cmplx_Iq.Ki = pMotor->FCL_params.Rq * wccXinvZb *
            pMotor->FCL_params.tSamp;

    Ki_rev = pMotor->cmplx_Iq.Ki / 2;

    pMotor->cmplx_Iq.Kerr = Ki_rev + pMotor->cmplx_Iq.Kp;
    pMotor->cmplx_Iq.KerrOld = Ki_rev - pMotor->cmplx_Iq.Kp;

    pMotor->cmplx_Iq.carryOver =
           (pMotor->cmplx_Iq.err * pMotor->cmplx_Iq.KerrOld) +
           (pMotor->FCL_params.BemfK * (pMotor->speedWe - pMotor->speedWePrev));

    pMotor->speedWePrev = pMotor->speedWe;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->cmplx_Id.fbk = pMotor->cmplx_Id.ref - pMotor->cmplx_Id.err;
    pMotor->cmplx_Iq.fbk = pMotor->cmplx_Iq.ref - pMotor->cmplx_Iq.err;

    return;
}

//-----------------------------------------------------------------------------
// COMPLEX CONTROL ROUTINES
//-----------------------------------------------------------------------------
//
// FCL complex controller
//
static inline void FCL_runComplexCtrl(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    SVGEN     svgen2;

    park1Sine   = __sinpuf32(pMotor->pangle);
    park1Cosine = __cospuf32(pMotor->pangle);

    //
    // CLARKE transformation
    //
#if(DRIVER_MODULE == BITFIELD_MODE)
    while(pMotor->pADCIntFlag->bit.ADCINT1 == 0);
#else
    while(ADC_getInterruptStatus(pMotor->adcBaseW, pMotor->adcIntNumber) == 0);
#endif

//    clarke1Alpha = (float32_t)((int16_t)HWREGH(pMotor->curA_PPBRESULT) *
//            pMotor->FCL_params.adcPPBScale);
//    clarke1Beta  = ((clarke1Alpha +
//                   (2.0F * ((float32_t)((int16_t)HWREGH(pMotor->curB_PPBRESULT))
//                           * pMotor->FCL_params.adcPPBScale))) *ONEbySQRT3);

    clarke1Alpha = (float32_t)((int16_t)HWREGH(pMotor->curA_PPBRESULT) *
            pMotor->FCL_params.adcAlphaScale);
    clarke1Beta  = (clarke1Alpha +
                   (((float32_t)((int16_t)HWREGH(pMotor->curB_PPBRESULT))
                           * pMotor->FCL_params.adcBetaScale))) * ONEbySQRT3;

    //
    // PARK Transformation
    //
    pMotor->cmplx_Iq.err = pMotor->cmplx_Iq.ref -
            ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));

    pMotor->cmplx_Id.err = pMotor->cmplx_Id.ref -
            ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    //
    //  Complex digital controllers for Id and Iq
    //
    FCL_runCCSyn(&pMotor->cmplx_Id, &pMotor->cmplx_Iq);

    //
    // Inverse Park Transformation
    //
    svgen2.Ualpha = ((pMotor->cmplx_Id.out * park1Cosine) -
            (pMotor->cmplx_Iq.out * park1Sine)) * pMotor->FCL_params.carrierMid;

    svgen2.Ubeta  = ((pMotor->cmplx_Iq.out * park1Cosine) +
            (pMotor->cmplx_Id.out * park1Sine)) * pMotor->FCL_params.cmidsqrt3;

    //
    // PWM pulse width time calculation
    //
    svgen2.Tb = (svgen2.Ubeta - svgen2.Ualpha) / 2.0;
    svgen2.Tc = svgen2.Tb - svgen2.Ubeta;

    svgen2.tmp2  = __fmax(__fmax(svgen2.Ualpha, svgen2.Tc), svgen2.Tb);
    svgen2.tmp2 += __fmin(__fmin(svgen2.Ualpha, svgen2.Tc), svgen2.Tb);
    svgen2.tmp1  = pMotor->FCL_params.carrierMid - (svgen2.tmp2 / 2.0);

    //
    // PWM updates
    //
    *(pMotor->pwmCompA) = (uint32_t)(svgen2.Tc + svgen2.tmp1);
    *(pMotor->pwmCompB) = (uint32_t)(svgen2.Ualpha + svgen2.tmp1);
    *(pMotor->pwmCompC) = (uint32_t)(svgen2.Tb + svgen2.tmp1);

    FCL_readCount(pMotor);

    return;
}

//
// Wrap up function to be called by the user application at the completion of
// Fast Current Loop (FCL) in Complex Control Mode
//
static inline void FCL_runComplexCtrlWrap(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase = pMotor->FCL_params.Vdcbus * 1.15/2;
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    float32_t WTs = pMotor->speedWe *
            pMotor->FCL_params.Wbase * pMotor->FCL_params.tSamp;

    pMotor->cmplx_Id.cosWTs  = pMotor->cmplx_Iq.cosWTs = __cos(WTs);
    pMotor->cmplx_Id.sinWTs  = pMotor->cmplx_Iq.sinWTs = __sin(WTs);

    pMotor->cmplx_Id.expVal  = expf( -(pMotor->FCL_params.Rd /
            pMotor->FCL_params.Ld) * pMotor->FCL_params.tSamp);
    pMotor->cmplx_Iq.expVal  = expf( -(pMotor->FCL_params.Rq /
            pMotor->FCL_params.Lq) * pMotor->FCL_params.tSamp);

    pMotor->cmplx_Id.kDirect = pMotor->FCL_params.Rd * invZbase *
              __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccD *
                                         pMotor->FCL_params.tSamp)) /
                                         (1.0F - pMotor->cmplx_Id.expVal);
    pMotor->cmplx_Iq.kDirect = pMotor->FCL_params.Rq * invZbase *
              __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccQ *
                                         pMotor->FCL_params.tSamp)) /
                                         (1.0F - pMotor->cmplx_Iq.expVal);

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //    this wrap function and 'carryOver' carries it to the next iteration
    // Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //    change much between iterations - equation tweaked to fit here
    //
    pMotor->cmplx_Id.carryOver = ( -pMotor->cmplx_Id.kDirect *
            pMotor->cmplx_Id.err * pMotor->cmplx_Id.expVal);
    pMotor->cmplx_Iq.carryOver = ( -pMotor->cmplx_Iq.kDirect *
            pMotor->cmplx_Iq.err * pMotor->cmplx_Iq.expVal) +
           (pMotor->FCL_params.BemfK * (pMotor->speedWe - pMotor->speedWePrev));

    pMotor->speedWePrev = pMotor->speedWe;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->cmplx_Id.fbk = pMotor->cmplx_Id.ref - pMotor->cmplx_Id.err;
    pMotor->cmplx_Iq.fbk = pMotor->cmplx_Iq.ref - pMotor->cmplx_Iq.err;

    return;
}

//------------------------------------------------------------------------------
// QEP INTERFACE ROUTINES
//------------------------------------------------------------------------------

#if(DRIVER_MODULE == BITFIELD_MODE)
//
// QEP position Estimation Module
//
static inline void FCL_runQEPPosEst(MOTOR_Vars_t *pMotor)
{
    if(pMotor->lsw == ENC_CALIBRATION_DONE)
    {
        //
        // QEP UTO and ADCSOC are carefully pre aligned
        // QEP POSLAT = QEP POSCNT at QEP UTO event that is in sync with ADCSOC
        // Therefore, read mech angle from QEP POSLAT
        //
        pMotor->posMechTheta = pMotor->posMechScaler *
                pMotor->pQEPRegs->QPOSLAT;

        pMotor->posElecTheta = pMotor->polePairs  * pMotor->posMechTheta;
        pMotor->posElecTheta -= ((int32)(pMotor->posElecTheta));

        pMotor->pangle = pMotor->posElecTheta;
    }
    else if(pMotor->lsw == ENC_WAIT_FOR_INDEX)
    {
        //
        // If QEP index pulse is found :-
        // POSILAT captures POSCNT at the first INDEX pulse.
        // Load POSINIT with POSILAT, so that at every future INDEX event,
        // POSCNT is loaded (reset) with POSINIT
        //
        if (pMotor->pQEPRegs->QFLG.all & QEP_FLAG_IEL_EVENT)
        {
            //QPOSILAT updates on every Index edge(IEL)
            pMotor->pQEPRegs->QPOSINIT = pMotor->pQEPRegs->QPOSILAT;

            //make QPOSCNT=QPOSINIT on Index edge
            pMotor->pQEPRegs->QEPCTL.all |= IEI_RISING;   //

            pMotor->lsw = ENC_CALIBRATION_DONE;

        }   // Keep the latched pos. at the first index

        pMotor->pangle = pMotor->rg.Out;
    }
    else
    {
        //
        // Alignment Routine - Reset POSCNT to ZERO
        //
        // during alignment, reset the current shaft position to zero
        pMotor->pQEPRegs->QPOSCNT = 0;

        // write to the entire register - it is more efficient
        // Reset pos cnt for QEP
        // pMotor->QepRegs->QCLR.bit.IEL = 1;
        pMotor->pQEPRegs->QCLR.all = QEP_FLAG_IEL_EVENT;

        // reset poscnt init on index
        //pMotor->QepRegs->QEPCTL.all &= ~IEI_RISING;
        pMotor->pQEPRegs->QEPCTL.bit.IEI = 0;

        pMotor->pangle  = 0.0;
    }

    return;
}

//
// QEP peripheral - clearing flags after previous read
//
static inline void FCL_runQEPPosEstWrap(MOTOR_Vars_t *pMotor)
{
    if(pMotor->lsw != ENC_ALIGNMENT)
    {
        //
        // Check an index occurrence
        //
        if (pMotor->pQEPRegs->QFLG.all & QEP_FLAG_IEL_EVENT)
        {
            //
            // Clear interrupt flag
            // pMotor->QepRegs->QCLR.bit.IEL = 1;
            //
            pMotor->pQEPRegs->QCLR.all = QEP_FLAG_IEL_EVENT;
        }

        //
        // Check unit Time out-event for speed calculation:
        // Unit Timer is sync'ed with motor PWM during INIT function
        //
        if(pMotor->pQEPRegs->QFLG.all & QEP_FLAG_UTO_EVENT)
        {
            pMotor->pQEPRegs->QCLR.all = QEP_FLAG_UTO_EVENT;

            //
            // Low Speed Calculation
            //
            if(pMotor->pQEPRegs->QEPSTS.all & 0x000C)   //QEPSTS.(COEF || CDEF)
            {
                //
                // Capture Counter overflowed, hence do no compute speed
                //
                pMotor->pQEPRegs->QEPSTS.all = 0x000C;
            }
        }
    }

    return;
}
#else

#define EQEP_POSCNT_INIT_NOTHING             0U //!< No action
#define EQEP_POSCNT_INIT_RISING_EDGE    0x0200U //!< poscnt=posinit @ QEPI rise
#define EQEP_POSCNT_INIT_FALLING_EDGE   0x0300U //!< poscnt=posinit @ QEPI fall

static inline void EQEP_resetPoscnt(uint32_t base, uint16_t initMode)
{
    //
    // Check the arguments.
    //
    ASSERT(EQEP_isBaseValid(base));

    //
    // Set the init mode in the QEP Control register.
    //
    HWREGH(base + EQEP_O_QEPCTL) = (HWREGH(base + EQEP_O_QEPCTL) &
                                    ~(EQEP_QEPCTL_IEI_M)) | initMode;
}

//
// QEP position Estimation Module
//
static inline void FCL_runQEPPosEst(MOTOR_Vars_t *pMotor)
{
    uint32_t qepBase = (uint32_t)pMotor->pQEPRegs;

    if(pMotor->lsw == ENC_CALIBRATION_DONE)
    {
        //
        // QEP UTO and ADCSOC are carefully pre aligned
        // QEP POSLAT = QEP POSCNT at QEP UTO event that is in sync with ADCSOC
        // Therefore, read mech angle from QEP POSLAT
        //
        pMotor->posMechTheta = pMotor->posMechScaler *
                EQEP_getPositionLatch(qepBase);
        pMotor->posElecTheta = pMotor->polePairs  * pMotor->posMechTheta;
        pMotor->posElecTheta -= ((int32)(pMotor->posElecTheta));

        pMotor->pangle = pMotor->posElecTheta;
    }
    else if(pMotor->lsw == ENC_WAIT_FOR_INDEX)
    {
        //
        // If QEP index pulse is found :-
        // POSILAT captures POSCNT at the first INDEX pulse.
        // Load POSINIT with POSILAT, so that at every future INDEX event,
        // POSCNT is loaded (reset) with POSINIT
        //
        if (EQEP_getInterruptStatus(qepBase) & EQEP_INT_INDEX_EVNT_LATCH)
        {
            //QPOSINIT = QPOSILAT (QPOSILAT updates on Index edge(IEL))
            EQEP_setInitialPosition(qepBase,
                                    EQEP_getIndexPositionLatch(qepBase));

            //make QPOSCNT=QPOSINIT on Index edge
            EQEP_resetPoscnt(qepBase, EQEP_POSCNT_INIT_RISING_EDGE);

            pMotor->lsw = ENC_CALIBRATION_DONE;
        }   // Keep the latched pos. at the first index

        pMotor->pangle = pMotor->rg.Out;
    }
    else
    {
        //
        // Alignment Routine - Reset POSCNT to ZERO
        // during alignment, reset the current shaft position to zero
        //
        EQEP_setPosition(qepBase, 0);
        EQEP_clearInterruptStatus(qepBase, EQEP_INT_INDEX_EVNT_LATCH);
        EQEP_resetPoscnt(qepBase, EQEP_POSCNT_INIT_NOTHING);

        pMotor->pangle = 0;
    }

    return;
}

//
// QEP peripheral - clearing flags after previous read
//
static inline void FCL_runQEPPosEstWrap(MOTOR_Vars_t *pMotor)
{
    uint32_t qepBase = (uint32_t)pMotor->pQEPRegs;

    if(pMotor->lsw != ENC_ALIGNMENT)
    {
        // Check an index occurrence
        if (EQEP_getInterruptStatus(qepBase) & EQEP_INT_INDEX_EVNT_LATCH)
        {
            // Clear index event INT flag
            EQEP_clearInterruptStatus(qepBase, EQEP_INT_INDEX_EVNT_LATCH);
        }

        // Check unit Time out-event for speed calculation
        // Unit Timer is sync'ed with motor PWM during INIT function
        if (EQEP_getInterruptStatus(qepBase) & EQEP_INT_UNIT_TIME_OUT)
        {
            // Clear UTO event INT flag
            EQEP_clearInterruptStatus(qepBase, EQEP_INT_UNIT_TIME_OUT);

            //
            // Low Speed Calculation
            //
            if(EQEP_getStatus(qepBase) &
                    (EQEP_STS_CAP_OVRFLW_ERROR | EQEP_STS_CAP_DIR_ERROR))
            {
                //
                // Capture Counter overflowed, hence do no compute speed
                //
                EQEP_clearStatus(qepBase,
                     (EQEP_STS_CAP_OVRFLW_ERROR | EQEP_STS_CAP_DIR_ERROR));
            }
        }
    }

    return;
}
#endif // BF

#endif /* _FAST_CURRENT_LOOP_H_ */
