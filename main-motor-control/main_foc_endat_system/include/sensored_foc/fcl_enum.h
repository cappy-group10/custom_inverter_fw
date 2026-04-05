//#############################################################################
//
// FILE:    fcl_enum.h
//
// TITLE:   define enumerations for FCL
//
// Group:   C2000
//
// Target Family: F2837x/F28004x
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

#ifndef FCL_ENUM_H
#define FCL_ENUM_H

//
//! \brief Enumeration for PWM update mode
//
typedef enum
{
    PWW_CMP_CTR_ZERO  = 0,
    PWW_CMP_CTR_PRD   = 1,
    PWW_CMP_CTR_BOTH  = 2,
    PWW_CMP_IMMEDIATE = 3
} PWMUpdateMode_e;

//
//! \brief Enumeration for State Machine typedef for motor QEP calibration
//
typedef enum
{
    QEP_CALIB_LOOPFLUSH = 0,
    QEP_CALIB_EQP1 = 1,
    QEP_CALIB_QEP2 = 2,
    QEP_CALIB_DONE = 3
} QEPCalibSM_e;

//
//! \brief Enumeration for Motor run/ stop command
//
typedef enum
{
    MOTOR_STOP = 0,
    MOTOR_RUN = 1
} MotorRunStop_e;

//
//! \brief Enumeration for synchronization control
//
typedef enum
{
    CTRL_SYN_DISABLE = 0,
    CTRL_SYN_ENABLE  = 1
} CtrlSync_e;

//
//! \brief Enumeration for ethercat control
//
typedef enum
{
    ECAT_CTRL_DISABLE = 0,
    ECAT_CTRL_ENABLE  = 1
} ECATCtrl_e;
//
//! \brief Enumeration for Motor control state
//
typedef enum
{
    CTRL_STOP  = 0,
    CTRL_RUN   = 1,
    CTRL_BRAKE = 2,
    CTRL_RESET = 3,
    CTRL_FAULT = 4,
    CTRL_CALIBRATE = 5
} CtrlState_e;

//
//! \brief Enumeration for controller loop
//
typedef enum
{
    CTRL_MODE_STOP     = 0,
    CTRL_MODE_SPEED    = 1,
    CTRL_MODE_POSITION = 2,
    CTRL_MODE_RESET    = 3,
    CTRL_MODE_FAULT    = 4
} CtrlMode_e;

//
//! \brief Enumeration for Load motor selection/ reset
//
typedef enum
{
    LOAD_NONE = 0,
    LOAD_MOTOR1 = 1,
    LOAD_MOTOR2 = 2
} LoadMotor_e;

//
//! \brief Enumeration for FCL controller --> PI/ FCL
//
typedef enum
{
    CNTLR_CPI = 0,
    CNTLR_CMPLX = 1
} CurrentCntlr_e;

//
//! \brief Enumeration for SFRA test axis
//
typedef enum
{
    SFRA_TEST_D_AXIS = 0,
    SFRA_TEST_Q_AXIS = 1,
    SFRA_TEST_SPEEDLOOP = 2
} SFRATest_e;

//
//! \brief Enumeration for PWM update mode
//
typedef enum
{
    PWM_UPDATE_IMMEDIATE = 0,
    PWM_UPDATE_SHADOW = 1
} PWMUpdateType_e;

#endif // end of FCL_ENUM_H definition
