//----------------------------------------------------------------------------------
//  FILE:           qep_defs.h
//
//  Description:    Contains QEP macros
//
//  Version:        1.0
//
//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

#ifndef QEP_DEFS_H
#define QEP_DEFS_H

#include <stdint.h>
#include "inc/hw_types.h"

#define QEP_FLAG_IEL_EVENT  0x0400
#define QEP_FLAG_UTO_EVENT  0x0800

/*-----------------------------------------------------------------------------
Define the structure of the QEP (Quadrature Encoder) Driver Object
-----------------------------------------------------------------------------*/
typedef struct {
    float32_t ElecTheta;       // Output: Motor Electrical angle
    float32_t MechTheta;       // Output: Motor Mechanical Angle
    uint16_t  DirectionQep;    // Output: Motor rotation direction
    uint16_t  QepPeriod;       // Output: Capture period of QEP signal in number
                               //         of EQEP capture timer (QCTMR) period
    uint32_t  QepCountIndex;   // Variable: Encoder counter index
    int32_t   RawTheta;        // Variable: Raw angle from EQEP Postiion counter
    float32_t MechScaler;      // Parameter: 0.9999/total count
    uint16_t  LineEncoder;     // Parameter: Number of line encoder
    uint16_t  PolePairs;       // Parameter: Number of pole pairs
    int32_t   CalibratedAngle; // Parameter: Raw angular offset
                               //            between encoder index and phase A
    uint16_t  IndexSyncFlag;   // Output: Index sync status
} QEP;

/*-----------------------------------------------------------------------------
Default initializer for the QEP Object.
-----------------------------------------------------------------------------*/
#define QEP_DEFAULTS { \
    0.0, /* ElecTheta */ \
    0.0, /* MechTheta */ \
    0, /* DirectionQep */ \
    0, /* QepPeriod */ \
    0, /* QepCountIndex */ \
    0, /* RawTheta */ \
    0.0, /* MechScaler */ \
    1000, /* LineEncoder */ \
    4, /* PolePairs */ \
    0, /* CalibratedAngle */ \
    0 /* IndexSyncFlag */ \
    }


// State machine typedef for ENC status
typedef enum
{
    ENC_ALIGNMENT        = 0,
    ENC_WAIT_FOR_INDEX   = 1,
    ENC_CALIBRATION_DONE = 2
} ENC_Status_e;


#endif // QEP_DEFS_H




