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
// FILE:    dual_axis_servo_drive.h
//
// TITLE:   Include header files used in the project
//
// Target Family: F2837x/F28004x/F28P55x
//
//-----------------------------------------------------------------------------


#ifndef DUAL_AXIS_SERVO_DRIVE_H
#define DUAL_AXIS_SERVO_DRIVE_H

//
//! \file   solutions/common/sensored_foc/include/dual_axis_servo_drive.h
//! \brief  header file to be included in all labs
//!
//


//
//! \defgroup LABS LABS
//! @{
//

//
// includes
//
#include <math.h>

#include "device.h"

#include "dual_axis_servo_drive_user.h"

#include "clarke.h"
#include "park.h"
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

#if defined(F28002x_DEVICE)
#include "DCLF32.h"
#include "DCL_NLPID.h"
#include "fcl_foc_cpu_dm.h"

#include "dlog_2ch_f.h"
#elif defined(F28004x_DEVICE) || defined(F2837x_DEVICE) || \
      defined(F28P55x_DEVICE) || defined(F28P65x_DEVICE)
#include "fcl_enum.h"
#include "fcl_cla_dm.h"
#include "fcl_cpu_cla_dm.h"
#include "cpu_cla_shared_dm.h"

#include "dlog_4ch_f.h"
#endif  // F28004x_DEVICE || F2837x_DEVICE || F28P55x_DEVICE


#define DBUFF_SIZE_NUM  200

// Variables for Datalog module
#ifdef DLOG_ENABLE
#if defined (F28004x_DEVICE) || defined (F28P55x_DEVICE) || defined(F28P65x_DEVICE)
extern float32_t DBUFF_4CH1[DBUFF_SIZE_NUM];
extern float32_t DBUFF_4CH2[DBUFF_SIZE_NUM];
extern float32_t DBUFF_4CH3[DBUFF_SIZE_NUM];
extern float32_t DBUFF_4CH4[DBUFF_SIZE_NUM];

extern float32_t dlogCh1;
extern float32_t dlogCh2;
extern float32_t dlogCh3;
extern float32_t dlogCh4;

// Create an instance of DATALOG Module
extern DLOG_4CH_F dlog_4ch1;
#endif  // F28004x_DEVICE

#if defined(DAC128S_ENABLE)
extern DAC128S_Handle   dac128sHandle;
extern DAC128S_Obj      dac128s;
#endif  // DAC128S_ENABLE

#if defined (F28002x_DEVICE)
extern float32_t DBUFF_4CH1[DBUFF_SIZE_NUM];
extern float32_t DBUFF_4CH2[DBUFF_SIZE_NUM];

extern float32_t dlogCh1;
extern float32_t dlogCh2;

// Create an instance of DATALOG Module
extern DLOG_2CH_F dlog_2ch1;
#endif  // F28002x_DEVICE

#endif  // DLOG_ENABLE

extern float32_t posArray[8];
extern float32_t posPtrMax;

extern volatile uint16_t FCL_cycleCount[2];

//
// the function prototypes
//

//! \brief
//! \param[in]  out
//! \return
extern float32_t refPosGen(float32_t out, MOTOR_Vars_t *pMotor);

//! \brief
//! \param[in]  in
//! \param[in]  out
//! \param[in]  rampDelta
//! \return
extern float32_t ramper(float32_t in, float32_t out, float32_t rampDelta);


//
// Current sensors scaling
//
static inline uint16_t scaleCurrentValue(float32_t current, float32_t currentSF)
{
    return((uint16_t)(current * currentSF));
}

//
// Voltage sensors scaling
//
static inline uint16_t scaleVoltageValue(float32_t voltage, float32_t voltageSF)
{
    return((uint16_t)(voltage * voltageSF));
}

// ****************************************************************************
// Position Reading Functions - EnDat
// ****************************************************************************
static inline bool updateMotorPositionFeedback(MOTOR_Num_e motorNum);

static inline ENC_Status_e getPostAlignmentEncoderState(void)
{
    // The active build uses an absolute EnDat encoder, so there is no
    // incremental-index search state after startup.
    return ENC_CALIBRATION_DONE;
}

// ****************************************************************************
// Get FCL timing details - time stamp taken in library after PWM update
// ****************************************************************************
#if(BUILDLEVEL > FCL_LEVEL2)
#pragma FUNC_ALWAYS_INLINE(getFCLTime)

static inline void getFCLTime(MOTOR_Num_e motorNum)
{
    // SETGPIO18_HIGH; // only for debug

    if(EPWM_getTimeBaseCounterValue(halMtrHandle[motorNum]->pwmHandle[0]) <
            FCL_cycleCount[motorNum])
    {
        FCL_cycleCount[motorNum] =
                EPWM_getTimeBasePeriod(halMtrHandle[motorNum]->pwmHandle[0]) -
                FCL_cycleCount[motorNum];
    }

    if(motorVars[motorNum].fclCycleCountMax < FCL_cycleCount[motorNum])
    {
        motorVars[motorNum].fclCycleCountMax =
                FCL_cycleCount[motorNum];
    }

    if(motorVars[motorNum].fclClrCntr)
    {
        motorVars[motorNum].fclCycleCountMax = 0;
        motorVars[motorNum].fclClrCntr = 0;
    }

    //for 100MHz PWM clock
    motorVars[motorNum].fclLatencyInMicroSec =
            (motorVars[motorNum].fclCycleCountMax) * 0.01;

    // SETGPIO18_LOW;  // only for debug

    return;
}
#endif

//! \brief      Get the dc bus voltage
//! \return     The dc bus voltage
static inline float32_t getVdc(MOTOR_Vars_t *ptrMotor)
{
    float32_t vdc;

    vdc = HWREGH(ptrMotor->volDC_PPBRESULT) * ptrMotor->voltageScale;

    if(vdc < 1.0)
    {
        vdc = 1.0;
    }

    return(vdc);
}

//! \brief      Initializes the parameters of motor
//! \details    Initializes all the parameters for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
extern void initMotorParameters(MOTOR_Vars_t *pMotor, HAL_MTR_Handle mtrHandle);

//! \brief      Initializes the control variables of motor
//! \details    Initializes all the control variables for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
extern void initControlVars(MOTOR_Vars_t *pMotor);

//! \brief      Reset the control variables of motor
//! \details    Reset the control variables for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
extern void resetControlVars(MOTOR_Vars_t *pMotor);

//! \brief      Run offser calibration
//! \details    implements offset calculation using filters
//! \param[in]  pMotor   A pointer to the motorVars object
extern void runOffsetsCalculation(MOTOR_Vars_t *pMotor);

//! \brief      Run motor control
//! \details    Set current limitation, check fault
//! \param[in]  pMotor   A pointer to the motorVars object
extern void
runMotorControl(MOTOR_Vars_t *pMotor, HAL_MTR_Handle mtrHandle);

//! \brief      Run motor sync control
extern void runSyncControl(void);

//
// SFRA utility functions
//
#if(BUILDLEVEL == FCL_LEVEL6)
extern void injectSFRA(void);
extern void collectSFRA(MOTOR_Vars_t *pMotor);
#endif

//
// Close the Doxygen group.
//! @} //defgroup
//

#endif // end of DUAL_AXIS_SERVO_DRIVE_H definition
