//#############################################################################
// FILE:   fcl_cpu_cla.h
// TITLE:  Header file to be shared between example and library for CPU data.
//
// Group: C2000
// Target Family:  F2837x/F2838x/F28004x
//
//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

#ifndef FCL_CPU_CLA_H
#define FCL_CPU_CLA_H

typedef float _iq;
#include <math.h>
#include <stdlib.h>

#if defined(F2837x_DEVICE) || defined(F28004x_DEVICE)
#include "F28x_Project.h"
#else
#include "f28x_project.h"
#endif

#include "driverlib.h"

#include "fcl_pi.h"
#include "qep_defs.h"

#include "clarke.h"
#include "park.h"
#include "speed_fr.h"
#include "svgen.h"
#include "RAMP_GEN_CLA.h"

/* ****************************************************************************
 * The following variables are brought into library but not used as they take
 * few more cycles - (svgen1 seemed to take similar time as local def -
 * probably because of its size)
 * These variables can be used by user for their experimentation purposes
 * ***************************************************************************
 */

extern CLARKE       clarke1;        // LEM Current
extern CLARKE       clarke2;        // SDFM Current
extern float32_t    currentSenV;    // V phase SDFM Current
extern float32_t    currentSenW;    // W phase SDFM Current

extern PARK         park1;

//
// Instance a Space Vector PWM modulator. This modulator generates a, b and c
// phases based on the d and q stationery reference frame inputs
//
extern SVGEN    svgen1;
//*****************************************************************************
extern ENC_Status_e lsw;

extern float32_t    pangle;
extern float32_t    speedWe;

extern QEP          qep1;

extern FCL_PIController_t pi_id;
extern FCL_PIController_t pi_iq;

//
//  Instance a ramp generator to simulate an Angle
//
extern RAMP_GEN_CLA rg1;

extern SPEED_MEAS_QEP speed1;


typedef struct _FCL_Parameters_ {
    float32_t   carrierMid;      // Mid point value of carrier count
    float32_t   adcScale;        // ADC conversion scale to pu
    float32_t   sdfmScale;       // ADC conversion scale to pu
    float32_t   cmidsqrt3;       // internal variable

    float32_t   tSamp;           // sampling time
    float32_t   Rd;              // Motor resistance in D axis
    float32_t   Rq;              // Motor resistance in Q axis
    float32_t   Ld;              // Motor inductance in D axis
    float32_t   Lq;              // Motor inductance in Q axis
    float32_t   Vbase;           // Base voltage for the controller
    float32_t   Ibase;           // Base current for the controller
    float32_t   wccD;            // D axis current controller bandwidth
    float32_t   wccQ;            // Q axis current controller bandwidth
    float32_t   Vdcbus;          // DC bus voltage
    float32_t   BemfK;           // Motor Bemf constant
    float32_t   Wbase;           // Controller base frequency (Motor) in rad/sec
} FCL_Parameters_t;

extern FCL_Parameters_t FCL_params;

#define FCL_PARS_DEFAULTS {                                                    \
    0, /* carrierMid */                                                        \
    0, /* adcScale */                                                          \
    0, /* sdfmScale */                                                         \
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
//interface functions
//
extern void FCL_runPICtrl(void);
extern void FCL_runSDFMPICtrl(void);
extern void FCL_runComplexCtrl(void);
extern void FCL_runSDFMComplexCtrl(void);

extern void FCL_runPICtrlWrap(void);
extern void FCL_runComplexCtrlWrap(void);

extern void FCL_runAbsEncPICtrl(void);
extern void FCL_runSDFMAbsEncPICtrl(void);
extern void FCL_runAbsEncComplexCtrl(void);
extern void FCL_runSDFMAbsEncComplexCtrl(void);

extern void FCL_runAbsEncPICtrlWrap(void);
extern void FCL_runAbsEncComplexCtrlWrap(void);

extern void FCL_runQEPWrap(void);
extern void FCL_initPWM(uint32_t basePhaseU, uint32_t basePhaseV,
                        uint32_t basePhaseW);
extern void FCL_initADC(uint32_t resultBaseA, ADC_PPBNumber baseA_PPB,
                        uint32_t resultBaseB, ADC_PPBNumber baseB_PPB,
                        uint32_t adcBasePhaseW);
extern void FCL_initQEP(uint32_t baseA);
extern void FCL_resetController(void);
extern uint32_t FCL_getSwVersion(void);

//
// tasks 1-4 are owned by the FCL library
//
extern __interrupt void Cla1Task1(void);
extern __interrupt void Cla1Task2(void);
extern __interrupt void Cla1Task3(void);
extern __interrupt void Cla1Task4(void);

#endif // end of FCL_CPU_CLA_H definition
