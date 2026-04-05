//#############################################################################
//
// FILE:    cl_pi.h
//
// TITLE:   Header file to be shared between example and library for CPU data.
//
// Group:   C2000
//
// Target Family: F2837x/F2838x/F28004x
//
//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

#ifndef FCL_PI_H
#define FCL_PI_H

#include "inc/hw_types.h"

typedef struct {
    float32_t  ref;             // Input: reference set-point
    float32_t  fbk;             // Input: feedback
    float32_t  err;             // Output : error
    float32_t  out;             // Output: controller output
    float32_t  carryOver;       // Output : carry over for next iteration
    float32_t  Kp;              // Parameter: proportional loop gain
    float32_t  Ki;              // Parameter: integral gain
    float32_t  Kerr;            // Parameter: gain for latest error
    float32_t  KerrOld;         // Parameter: gain for prev error
    float32_t  Umax;            // Parameter: upper saturation limit
    float32_t  Umin;            // Parameter: lower saturation limit
} FCL_PIController_t;

#define FCL_PI_CONTROLLER_DEFAULTS {                                           \
                           0.0,    /* ref */                                   \
                           0.0,    /* fbk */                                   \
                           0.0,    /* err */                                   \
                           0.0,    /* out */                                   \
                           0.0,    /* carryOver */                             \
                           1.0,    /* Kp */                                    \
                           0.1,    /* Ki */                                    \
                           0.0,    /* Kerr */                                  \
                           0.0,    /* KerrOld */                               \
                           1.0,    /* Umax */                                  \
                           -1.0    /* Umin */                                  \
                          }
#endif // FCL_PI_H
