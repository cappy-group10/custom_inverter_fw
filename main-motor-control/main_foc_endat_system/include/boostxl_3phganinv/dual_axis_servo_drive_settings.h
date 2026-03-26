//#############################################################################
//
// FILE:    dual_axis_servo_drive_settings.h
//
// TITLE:   User settings
//
// Group:   C2000
//
// Target Family: F2837x
//
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

#ifndef DUAL_AXIS_SERVO_DRIVE_SETTINGS_H
#define DUAL_AXIS_SERVO_DRIVE_SETTINGS_H

//
// Include project specific include files.
//

typedef float _iq;
#include <math.h>
#include <stdlib.h>

#include "device.h"

#include "ipark.h"              // Include header for the IPARK object
#include "pi.h"                 // Include header for the PI  object
#include "fcl_pi.h"             // Include header for the FCL_PI object
#include "svgen.h"              // Include header for the SVGENDQ object
#include "rampgen.h"            // Include header for the RAMPGEN object
#include "rmp_cntl.h"           // Include header for the RMPCNTL object
#include "volt_calc.h"          // Include header for the PHASEVOLTAGE object
#include "speed_fr.h"           // Include header for the SPEED_MEAS_QEP object
#include "resolver.h"
#include "pid_grando.h"
#include "pid_reg3.h"

#include <math.h>

#include "dlog_4ch_f.h"


//
// List of control GND configurations - COLD or HOT
//
#define  COLD  1           // control GND is COLD
#define  HOT   2           // control GND is HOT

//
// Following is the list of the Build Level choices.
//
#define  FCL_LEVEL1  1           // Verify SVGEN module and PWM generation
#define  FCL_LEVEL2  2           // Verify ADC, park/clarke, calibrate the
                                 // offset and speed measurement
#define  FCL_LEVEL3  3           // Verify closed current(torque) loop + its PI
#define  FCL_LEVEL4  4           // Verify speed loop and speed PID
#define  FCL_LEVEL5  5           // Verify position loop
#define  FCL_LEVEL6  6           // SFRA integration to verify bandwidth

//
// This line sets the SAMPLING FREQUENCY to one of the available choices
//
#define  SINGLE_SAMPLING    1
#define  DOUBLE_SAMPLING    2

//
// Following is the list of Current sense options
//
#define  SHUNT_CURRENT_SENSE   1
#define  LEM_CURRENT_SENSE     2
#define  SD_CURRENT_SENSE      3

//
// Following is the list of Position Encoder options
// Select Position Feedback Option
//
#define  QEP_POS_ENCODER       1
#define  RESOLVER_POS_ENCODER  2
#define  BISS_POS_ENCODER      3
#define  ENDAT_POS_ENCODER     4
#define  SINCOS_POS_ENCODER    5

//
// Here below, pick current loop controller option
//
#define  CMPLX_CNTLR     1
#define  PI_CNTLR        2


//
// Following is the list of motor
// Select motor to implement SFRA, DLOG and DAC
//
#define  MOTOR_1        1
#define  MOTOR_2        2
#define  MOTOR_NA       3

//
// User can select choices from available control configurations
//
#define  CGND                COLD
#define  BUILDLEVEL          FCL_LEVEL2
#define  SAMPLING_METHOD     SINGLE_SAMPLING   // DOUBLE_SAMPLING   // SINGLE_SAMPLING
#define  FCL_CNTLR           PI_CNTLR          // CMPLX_CNTLR       //
#define  CURRENT_SENSE       LEM_CURRENT_SENSE
#define  POSITION_ENCODER    ENDAT_POS_ENCODER

#define  SFRA_MOTOR          MOTOR_1

#define POSITION_ENCODER_IS_QEP      (POSITION_ENCODER == QEP_POS_ENCODER)
#define POSITION_ENCODER_IS_ENDAT    (POSITION_ENCODER == ENDAT_POS_ENCODER)
#define POSITION_ENCODER_NEEDS_INDEX (POSITION_ENCODER_IS_QEP)


// Uncomment to disable all fault protection for bench EnDat testing (NO INVERTER)
#define ENDAT_HACK

#define DISABLE_MOTOR_FAULTS
// #define DISABLE_BUS_VOLTAGE_CHECK
// #define DISABLE_OVERCURRENT_CHECK

// #define DISABLE_ENDAT
// #define DACOUT_EN


//
// Generate error if
//
#if(CURRENT_SENSE != LEM_CURRENT_SENSE)
#error  Critical: Only LEM_CURRENT_SENSE is supported in this example
#endif

#if(POSITION_ENCODER != ENDAT_POS_ENCODER)
#error  Critical: This configuration supports only ENDAT_POS_ENCODER
#endif

#ifndef BUILDLEVEL
#error  Critical: BUILDLEVEL must be defined !!
#endif  // BUILDLEVEL

#ifndef PI
#define PI 3.14159265358979
#endif


#endif  // end of DUAL_AXIS_SERVO_DRIVE_SETTINGS_H definition
