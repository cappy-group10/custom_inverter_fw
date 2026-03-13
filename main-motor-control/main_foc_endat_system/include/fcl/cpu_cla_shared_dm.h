//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

//------------------------------------------------------------------------------
//  Software:       Motor Control SDK
//
// FILE:   cpu_cla_shared_dm.h
//
// TITLE:  header file for shared data and types between CPU and CLA.
//
// Target Family: F2837x/F28004x/F28P55x/F28P65x
//
//-----------------------------------------------------------------------------



#ifndef CPU_CLA_SHARED_DM_H
#define CPU_CLA_SHARED_DM_H

//
// includes
//
#if defined(F2837x_DEVICE) || defined(F28004x_DEVICE)
#include "F28x_Project.h"
#else
#include "f28x_project.h"
#endif

#include "f28x_bmsk.h"
#include "qep_defs.h"

#include "fcl_pi.h"
#include "RAMP_GEN_CLA.h"

#include "dual_axis_servo_drive_user.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// defines
//

#if 0
#define SETGPIO18_HIGH  asm(" PUSH DP");                                       \
                        asm(" MOVW      DP,#0x1fc");                           \
                        asm(" OR        @0x1,#0x0004 ");                       \
                        asm(" POP DP");

#define SETGPIO18_LOW   asm(" PUSH DP");                                       \
                        asm(" MOVW      DP,#0x1fc") ;                          \
                        asm(" AND       @0x1,#0xfffb ");                       \
                        asm(" POP DP");

#else
#define SETGPIO18_HIGH
#define SETGPIO18_LOW

#endif

//
// typedefs
//

//
// Define the structure of the QEP (Quadrature Encoder) Driver Object
// this is same as QEP stucture
//

typedef struct motPars {
    float32_t cosWTs;
    float32_t sinWTs;
    float32_t expVal;
    float32_t kDirect;
    float32_t idErr;
    float32_t iqErr;
    float32_t carryOver;
} cmplxPars_t;


#define CMPLXPARS_DEFAULTS {                                                   \
    0, /* cosWTs */                                                            \
    0, /* sinWTs */                                                            \
    0, /* expVal */                                                            \
    0, /* kDirect */                                                           \
    0, /* idErr */                                                             \
    0, /* iqErr */                                                             \
    0  /* carryOver */                                                         \
}

//
// globals
//

//
// Variables for CLA tasks
//
#define FCL_DEFAULTS  {                                                        \
    ENC_ALIGNMENT,                      /* lsw */                              \
                                                                               \
    CMPLXPARS_DEFAULTS,                 /* Q_cla */                            \
    0,                                  /* speedWePrev */                      \
    0,                                  /* pangle */                           \
    RAMPGEN_CLA_DEFAULTS,               /* rg */                               \
    FCL_PI_CONTROLLER_DEFAULTS,         /* pi_iq */                            \
    QEP_DEFAULTS,                       /* qep */                              \
    &EQep1Regs,                         /* *ptrQEP */                          \
    0,                                  /* taskFlag */                         \
    0, 0, 0, 0                          /* taskCount[4] */                     \
 }

//
// default parameters for motor_2
//

//
//!  \brief typedefs for motorVars
//!   Variables for CLA tasks
//
typedef struct _FCL_Vars_t_
{
    ENC_Status_e lsw;

    cmplxPars_t Q_cla;
    float32_t speedWePrev;
    float32_t pangle;                   // rotor angle
    RAMP_GEN_CLA rg;                    // a ramp generator to simulate an Angle
    FCL_PIController_t pi_iq;
    QEP qep;
    volatile struct EQEP_REGS *ptrQEP;  // Aligned to lower 16-bits
    uint32_t taskCount[4];
} FCL_Vars_t;

extern FCL_Vars_t fclVars[2];


typedef struct _SVGEN2_t_
{
    float32_t  Ualpha;      // Input: reference alpha-axis phase voltage
    float32_t  Ubeta;       // Input: reference beta-axis phase voltage
    float32_t  Ta;          // Output: reference phase-a switching function
    float32_t  Tb;          // Output: reference phase-b switching function
    float32_t  Tc;          // Output: reference phase-c switching function
    float32_t  tmp1;        // Variable: temp variable
    float32_t  tmp2;        // Variable: temp variable
}SVGEN2_t;

//
// MACROS
//

//
// Speed optimized PI MACRO
//
#define  FCL_PI_MACRO(v)                                                       \
{                                                                              \
    v.out += (v.err * v.Kerr) + v.carryOver;                                   \
    v.out = (v.out > v.Umax) ? v.Umax : v.out;                                 \
    v.out = (v.out < v.Umin) ? v.Umin : v.out;                                 \
}

#define  CLAMP_MACRO(v)                                                        \
{                                                                              \
    v.out  = (v.out > v.Umax) ? v.Umax : v.out;                                \
    v.out  = (v.out < v.Umin) ? v.Umin : v.out;                                \
}

#ifdef __cplusplus
}
#endif // extern "C"

#endif // end of CPU_CLA_SHARED_DM_H definition
