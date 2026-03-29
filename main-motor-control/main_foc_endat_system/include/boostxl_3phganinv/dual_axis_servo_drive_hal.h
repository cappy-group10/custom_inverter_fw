//#############################################################################
//
// FILE:    dual_axis_servo_drive_hal.h
//
// TITLE:   header file of device handle configuration
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

#ifndef DUAL_AXIS_SERVO_DRIVE_HAL_H
#define DUAL_AXIS_SERVO_DRIVE_HAL_H

//
// Include project specific include files.
//
#include <math.h>

#include "device.h"
#include "driverlib.h"

#include "dual_axis_servo_drive_user.h"

//
// Define the system frequency (MHz)
//
#define SYSTEM_FREQUENCY    (DEVICE_SYSCLK_FREQ / 1000000U)

//
// Timer definitions based on System Clock
//
#define     MICROSEC         SYSTEM_FREQUENCY
#define     MICROSEC_50       50 * MICROSEC     // 50 uS
#define     MICROSEC_100     100 * MICROSEC     // 0.1 mS
#define     MICROSEC_150     150 * MICROSEC     // 0.15 mS
#define     MILLISEC        1000 * MICROSEC     // 1 mS

#define     MILSEC_0_5       0.5f * MILLISEC     // 0.5 mS
#define     MILSEC_1         1.0f * MILLISEC     // 1.0 mS
#define     MILSEC_2         2.0f * MILLISEC     // 2.0 mS
#define     MILSEC_5         5.0f * MILLISEC     // 5.0 mS
#define     MILSEC_7_5       7.5f * MILLISEC     // 7.5 mS
#define     MILSEC_10         10f * MILLISEC     // 10 mS
#define     MILSEC_20         20f * MILLISEC     // 20 mS
#define     MILSEC_50         50f * MILLISEC     // 50 mS
#define     MILSEC_100       100f * MILLISEC     // 100 mS
#define     MILSEC_500       500f * MILLISEC     // 500 mS
#define     MILSEC_1000     1000f * MILLISEC     // 1000 mS



#define  LPD_RED_LED1           34          // NC: Set up based board
#define  LPD_BLUE_LED2          31          // NC: Set up based board

#define  LPD_LED1_WAIT_TIME     800
#define  LPD_LED2_WAIT_TIME     400

//
// define EN_GATE and SPI_CS pin for DRV device for Motor 1
//
#define ENABLE_GATE        1           // Active low: 0, Active high: 1
#define DISABLE_GATE       0

#define M1_EN_GATE_GPIO         124         // NC: Set up based board
#define M1_SPI_SCS_GPIO         61          // NC: Set up based board
#define M1_nFAULT_GPIO          24          // NC: Set up based board
#define M1_XBAR_INPUT_GPIO      24          // NC: Set up based board
#define M1_XBAR_INPUT_NUM       XBAR_INPUT1
#define M1_CLR_FAULT_GPIO       61

//
// define deadband delay cout for rising edge
//
#define EPWM_DB_DELAY_RISE  200      // 50*10ns=0.5us, EPWMCLK=100MHz

//
// define deadband delay cout for falling edge
//
#define EPWM_DB_DELAY_FALL  200      // 50*10ns=0.5us, EPWMCLK=100MHz

//! \brief Enumeration for the Motor numbers
//!
typedef enum
{
  MTR_1 = 0,
  MTR_2 = 1
} MOTOR_Num_e;


//! \brief    Defines the hardware abstraction layer (HAL) data
//! \details  The HAL object contains all handles to peripherals. When
//!           accessing a peripheral on a processor, use a HAL function along
//!           with the HAL handle for that processor to access its peripherals.
//!
typedef struct _HAL_Obj_
{
  uint32_t      claHandle;          //!< the CLA handle

  uint32_t      timerHandle[3];     //<! the timer handles

  uint32_t      sciHandle[2];       //!< the SCI handle

  uint32_t      dacHandle[3];       //!< the DAC handle

  uint32_t      adcHandle[4];       //!< the ADC handles
  uint32_t      adcResult[4];       //!< the ADC Result handles
} HAL_Obj;

//! \brief    Defines the HAL handle
//! \details  The HAL handle is a pointer to a HAL object.  In all HAL functions
//!           the HAL handle is passed so that the function knows what
//!           peripherals are to be accessed.
//!
typedef struct _HAL_Obj_ *HAL_Handle;

//! \brief    Defines the hardware abstraction layer (HAL) data for motor
//! \details  The HAL object contains all handles to peripherals for motor
//!           control.
//!
typedef struct _HAL_MTR_Obj_
{
  uint32_t  pwmHandle[3];        //<! the PWM handles for Motor Control

  uint32_t  cmpssHandle[3];      //!< the CMPSS handle

  uint32_t  spiHandle;          //!< the SPI handle

  uint32_t  qepHandle;          //!< the QEP handle

  bool      flagEnablePWM;      //<! the pwm enable flag
} HAL_MTR_Obj;

//! \brief      Defines the HAL_MTR handle
//! \details    The HAL_MTR handle is a pointer to a HAL_MTR object.  In all
//!             HAL_MTR functions, the HAL_MTR handle is passed so that the
//!             function knows what peripherals are to be accessed.
//!
typedef struct _HAL_MTR_Obj_ *HAL_MTR_Handle;

extern HAL_Handle    halHandle;
extern HAL_Obj       hal;

extern HAL_MTR_Handle halMtrHandle[2];
extern HAL_MTR_Obj    halMtr[2];

//
// the function prototypes
//

//! \brief     Acknowledges an interrupt so that another INT can happen again.
//! \param[in] handle     The hardware abstraction layer (HAL) handle
static inline void HAL_ackInt_M1(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    EPWM_clearEventTriggerInterruptFlag(obj->pwmHandle[0]);

    //
    // clear ADCINT1 INT and ack PIE INT
    //
    ADC_clearInterruptStatus(M1_IW_ADC_BASE, ADC_INT_NUMBER1);

    //
    // ACK PIE for CLA INT GROUP
    // FCL is not clearing the ACK bit for CLA group
    // because the example might have other CLA Tasks
    // ACK the PWM, ADC and CLA interrupts
    //
    Interrupt_clearACKGroup( INTERRUPT_ACK_GROUP3 |
                             INTERRUPT_ACK_GROUP11 );
    return;
}

//! \brief     Clear TZFLAG of EPWM modules.
//! \param[in] handle     The hardware abstraction layer (HAL) handle
static inline void HAL_clearTZFlag(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    //
    // clear OST & DCAEVT1 flags
    //
    EPWM_clearTripZoneFlag(obj->pwmHandle[0],
                           (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

    EPWM_clearTripZoneFlag(obj->pwmHandle[1],
                           (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

    EPWM_clearTripZoneFlag(obj->pwmHandle[2],
                           (EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_DCAEVT1));

    //
    // clear HLATCH - (not in TRIP gen path)
    //
    CMPSS_clearFilterLatchHigh(obj->cmpssHandle[0]);
    CMPSS_clearFilterLatchHigh(obj->cmpssHandle[1]);
    CMPSS_clearFilterLatchHigh(obj->cmpssHandle[2]);

    //
    // clear LLATCH - (not in TRIP gen path)
    //
    CMPSS_clearFilterLatchLow(obj->cmpssHandle[0]);
    CMPSS_clearFilterLatchLow(obj->cmpssHandle[1]);
    CMPSS_clearFilterLatchLow(obj->cmpssHandle[2]);

    return;
}

//! \brief     Enables the interrupts
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_enableInterrupts(HAL_MTR_Handle handle);

//! \brief      Initializes the hardware abstraction layer (HAL) object
//! \details    Initializes all handles to the microcontroller peripherals.
//!             Returns a handle to the HAL object.
//! \param[in]  pMemory   A pointer to the memory for the HAL object
//! \param[in]  numBytes  The number of bytes allocated for the HAL object
//! \return     The hardware abstraction layer (HAL) object handle
extern HAL_Handle HAL_init(void *pMemory, const size_t numBytes);


//! \brief      Initializes the hardware abstraction layer object for motors
//! \details    Initializes all handles to the microcontroller peripherals.
//!             Returns a handle to the HAL_MTR object.
//! \param[in]  pMemory   A pointer to the memory for the HAL object
//! \param[in]  numBytes  The number of bytes allocated for the HAL object
//! \return     The hardware abstraction layer (HAL_MTR) object handle
extern HAL_MTR_Handle HAL_MTR_init(void *pMemory, const size_t numBytes);

//! \brief     Sets up the CPU timers
//! \param[in] base         The cpu timer base
//! \param[in] periodCount  The cpu timer period count
extern void HAL_setupCpuTimer(uint32_t base, uint32_t periodCount);


//! \brief     Sets up the interrupts
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupInterrupts(HAL_MTR_Handle handle);


//! \brief     Sets up the PWM module as Up count mode
//! \param[in] base     The PWM module base
//! \param[in] period   The PWM period count
extern void HAL_setupPWM_1chUpCnt(uint32_t base, uint16_t period);


//! \brief     Sets up the PWM module as Up count mode
//! \param[in] base     The PWM module base
//! \param[in] period   The PWM period count
extern void HAL_setupPWM_2ch_UpCnt(uint32_t base, uint16_t period);

//! \brief     Sets up the PWM module as Up-Down count mode enabled deadband
//! \param[in] base     The PWM module base
//! \param[in] period   The PWM period count
//! \param[in] db       The PWM deadband value
extern void HAL_setupPWM_1chUpDwnCnt(uint32_t base,
                                     uint16_t period, int16_t db);


//! \brief     Sets up the GPIO (General Purpose I/O) pins
//!
extern void HAL_setupGPIOs(HAL_Handle handle);


//! \brief     Sets up the PWMs (Pulse Width Modulators) for motor
//! \param[in] handle  The hardware abstraction layer (HAL) handle for motor
//! \param[in] systemFreq_MHz  The system frequency, MHz
//! \param[in] pwmPeriod_usec  The PWM period, usec
extern void HAL_setupMotorPWMs(HAL_MTR_Handle handle);


//! \brief     Sets up the ADC
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupADCs(HAL_Handle handle);


//! \brief     Sets up the CLA
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupCLA(HAL_Handle handle);

//! \brief     Sets up the CMPSS
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupCMPSS(HAL_MTR_Handle handle);

//! \brief     Sets up the CMPSS
//! \param[in] handle  The hardware abstraction layer (HAL) handle
//! \param[in] curHi  DAC value for CMPSS High threshold for motor
//! \param[in] curLo  DAC value for CMPSS Low threshold
extern void HAL_setupCMPSS_DACValue(HAL_MTR_Handle handle,
                                    const uint16_t curHi, const uint16_t curLo);


//! \brief     Sets up the DAC
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupDACs(HAL_Handle handle);


//! \brief     Sets up the GATE object
//! \param[in] handle       The hardware abstraction layer (HAL) handle
extern void HAL_setupGate(HAL_MTR_Handle handle,
                          const MOTOR_Num_e motorNum);

//! \brief     Sets up the QEP peripheral
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupQEP(HAL_MTR_Handle handle);


//! \brief     Sets up the GPIO (General Purpose I/O) pins for test
//! \param[in] handle  The hardware abstraction layer (HAL) handle
extern void HAL_setupTestGPIOs(HAL_Handle handle);

//! \brief     Sets up fault protection for motor
//! \param[in] handle  The hardware abstraction layer (HAL) handle for motor
//! \param[in] currentLimit  over current limitation value
extern void  HAL_setupMotorFaultProtection(HAL_MTR_Handle handle,
                                           const float32_t currentLimit);


//! \brief      Sets the hardware abstraction layer parameters
//! \details    Sets up the microcontroller peripherals.
//! \param[in]  handle    The hardware abstraction layer (HAL) handle
extern void HAL_setParams(HAL_Handle handle);


//! \brief      Sets the hardware abstraction layer parameters for motor
//! \param[in] handle  The hardware abstraction layer (HAL) handle for motor
extern void HAL_setMotorParams(HAL_MTR_Handle handle);


#endif  // end of DUAL_AXIS_SERVO_DRIVE_HAL_H definition
