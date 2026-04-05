//#############################################################################
//
// FILE:    sensored_foc_drive.h
//
// TITLE:   Include header files used in the project
//
// Group:   C2000
//
// Target Family: F2838x/F2837x/F28004x/F28002x
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
//
//! \file   solutions/common/sensored_foc/include/sensored_foc_drive.h
//! \brief  header file to be included in all labs
//!
//

#ifndef SENSORED_FOC_DRIVE_H
#define SENSORED_FOC_DRIVE_H

//
//! \defgroup LABS LABS
//! @{
//

//
// includes
//
#include <math.h>
#include "device.h"

// FOC header files
#include "clarke.h"
#include "park.h"
#include "ipark.h"              // Include header for the IPARK object
#include "pi.h"                 // Include header for the PI  object
#include "svgen.h"              // Include header for the SVGENDQ object
#include "rampgen.h"            // Include header for the RAMPGEN object
#include "rmp_cntl.h"           // Include header for the RMPCNTL object
#include "speed_fr.h"           // Include header for the SPEED_MEAS_QEP object
#include "pid_grando.h"
#include "dlog_4ch_f.h"

#include "DCLF32.h"

#ifdef F28002x_DEVICE
#include "DCL_NLPID.h"
#include "fcl_foc_cpu.h"
#endif  // F28002x_DEVICE

#ifdef F28004x_DEVICE
#ifdef FCL_CLA_VERSION
#include "fcl_dcore_cpu.h"
#else
#include "fcl_foc_cpu.h"
#endif
#endif  //F28004x_DEVICE

#include "sensored_foc_hal.h"

//
// Global variables used in this system
//
// motor drive variables
extern MOTOR_Vars_t motorVars;

// hardware abstraction layer variables
extern HAL_Handle halHandle;
extern HAL_Obj    hal;

// interrupt variables
extern volatile uint16_t tempPIEIER;

// Variables for Field Oriented Control
extern float32_t VdTesting;         // Vd reference (pu)
extern float32_t VqTesting;         // Vq reference (pu)

// Variables for position reference generation and control
extern float32_t posArray[8];
extern float32_t posPtrMax;

// Variables for Datalog module
#ifdef DLOG_ENABLE
#define DBUFF_SIZE_NUM  200

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
#endif  // DLOG_ENABLE

// Variables for SFRA module
#if((BUILDLEVEL == FCL_LEVEL6) || (BUILDLEVEL == FCL_LEVEL9))
extern SFRA_F32    sfra1;
extern SFRATest_e  sfraTestLoop;  //speedLoop;
extern uint32_t    sfraCollectStart;
extern float32_t   sfraNoiseD;
extern float32_t   sfraNoiseQ;
extern float32_t   sfraNoiseW;
#endif




//
// Instrumentation code for timing verifications
// display variable A (in pu) on DAC
//
#define  DAC_MACRO_PU(A)  ((1.0 + A)*2048)

//
// software prioritized interrupts
//
static inline void setInterruptPriority(void)
{
    //
    // Set interrupt priority:
    //
    tempPIEIER = HWREGH(PIECTRL_BASE + PIE_O_IER3);
    HWREGH(PIECTRL_BASE + PIE_O_IER3) &= 0xFFFE;

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3 |
                            INTERRUPT_ACK_GROUP11 );   // Enable PIE interrupts

//    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3 |
//                            INTERRUPT_ACK_GROUP7 |
//                            INTERRUPT_ACK_GROUP11 );   // Enable PIE interrupts
    __asm("  NOP");
    EINT;
}

static inline void restoreInterruptRegisters(void)
{
    //
    // Restore registers saved
    //
    __asm("  NOP");
    DINT;
    HWREGH(PIECTRL_BASE + PIE_O_IER3) = tempPIEIER;
}

//
// Get FCL timing details - time stamp taken in library after PWM update
//
static inline void getFOCCount(MOTOR_Vars_t *pMotor)
{
    pMotor->focCycleCount = EPWM_getTimeBaseCounterValue(pMotor->pwmBaseU);

    return;
}

//
// Get FCL timing details - time stamp taken in library after PWM update
//
static inline void getFOCTime(MOTOR_Vars_t *pMotor)
{
    if(EPWM_getTimeBaseCounterValue(pMotor->pwmBaseU) < pMotor->focCycleCount)
    {
        pMotor->focCycleCount = EPWM_getTimeBasePeriod(pMotor->pwmBaseU) -
                pMotor->focCycleCount;
    }

    if(pMotor->focCycleCountMax < pMotor->focCycleCount)
    {
        pMotor->focCycleCountMax = pMotor->focCycleCount;
    }

    if(pMotor->focClrCntr)
    {
        pMotor->focCycleCountMax = 0;
        pMotor->focClrCntr = 0;
    }

    //for 100MHz PWM clock
    pMotor->focExecutionTime_us = pMotor->focCycleCountMax * 0.01;

    return;
}

//
// Get FCL timing details - time stamp taken in library after PWM update
//
#if(BUILDLEVEL > FCL_LEVEL2)
static inline void getFCLTime(MOTOR_Vars_t *pMotor)
{
    if(EPWM_getTimeBaseCounterValue(pMotor->pwmBaseU) < pMotor->fclCycleCount)
    {
        pMotor->fclCycleCount = EPWM_getTimeBasePeriod(pMotor->pwmBaseU) -
                pMotor->fclCycleCount;
    }

    if(pMotor->fclCycleCountMax < pMotor->fclCycleCount)
    {
        pMotor->fclCycleCountMax = pMotor->fclCycleCount;
    }

    if(pMotor->fclClrCntr)
    {
        pMotor->fclCycleCountMax = 0;
        pMotor->fclClrCntr = 0;
    }

    //for 100MHz PWM clock
    pMotor->fclUpdateLatency_us = pMotor->fclCycleCountMax * 0.01;

    return;
}
#endif // (BUILDLEVEL > FCL_LEVEL2)

//
// slew programmable ramper
//
#if(BUILDLEVEL > FCL_LEVEL2)
static inline float32_t ramper(float32_t in, float32_t out, float32_t rampDelta)
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
#endif // (BUILDLEVEL > FCL_LEVEL2)

//
// Reference Position Generator for position loop
//
#if(BUILDLEVEL == FCL_LEVEL5)
extern float32_t posArray[8];
extern float32_t posPtrMax;

static inline float32_t refPosGen(float32_t out, MOTOR_Vars_t *pMotor)
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
#endif // (BUILDLEVEL == FCL_LEVEL5)

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


//
// Current sensors scaling
//
static inline uint16_t scaleCurrentValue(float32_t current)
{
    return((uint16_t)(current * (4096.0 / M_ADC_SCALE_CURRENT)));
}

//
// Voltage sensors scaling
//
static inline uint16_t scaleVoltageValue(float32_t voltage)
{
    return((uint16_t)(voltage * (4096.0 / M_ADC_SCALE_VOLATGE)));
}

//! \brief      interrupt subroutine for motor control
//! \details    interrupt subroutine for motor control
extern __interrupt void motorControlISR(void);

//! \brief      Initializes the parameters of motor
//! \details    Initializes all the parameters for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
extern void initMotorParameters(MOTOR_Vars_t *pMotor, HAL_Handle handle);

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
extern void runMotorControl(MOTOR_Vars_t *pMotor, HAL_Handle halHandle);

//
// Close the Doxygen group.
//! @} //defgroup
//

#endif // end of SENSORED_FOC_DRIVE_H definition
