//#############################################################################
// FILE:   cpu_cla_shared.h
// TITLE:  header file for shared data and types between CPU and CLA.
//
// Group:           C2000
// Target Family:   F2837x/F2838x/F28004x
//
//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

#ifndef CPU_CLA_SHARED_H
#define CPU_CLA_SHARED_H

//
// includes
//

#include "f28x_bmsk.h"

#include "qep_defs.h"
#include "fcl_pi.h"
#include "RAMP_GEN_CLA.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// defines
//

#if 0

#define SETGPIO18_HIGH  asm(" PUSH DP");                                       \
                        asm(" MOVW      DP,#0x1fc") ;                          \
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

//
// globals
//

//
// Task 1 (C) Variables
//
extern QEP qep1;
extern ENC_Status_e lsw;

//
// Task 2 (C) Variables
//
extern FCL_PIController_t pi_iq;

//
// Task 3 (C) Variables
//
extern cmplxPars_t Q_cla;
extern cmplxPars_t D_cpu;

extern float32_t pangle;
extern float32_t speedWe;

extern RAMP_GEN_CLA rg1;

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

#endif // end of CPU_CLA_SHARED_H definition
