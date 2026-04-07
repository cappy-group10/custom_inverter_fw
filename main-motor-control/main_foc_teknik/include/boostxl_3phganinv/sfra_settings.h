//#############################################################################
//
// FILE:    sfra_settings.h
//
// TITLE:   SFRA settings definition
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

#ifndef SFRA_SETTINGS_H
#define SFRA_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "dual_axis_servo_drive_settings.h"
#include "dual_axis_servo_drive_user.h"
#include "dual_axis_servo_drive_hal.h"

#include "sfra_f32.h"
#include "sfra_gui_scicomms_driverlib.h"

//
// API prototypes
//
extern void configureSFRA(uint16_t plotOption, float32_t sfraISRFreq);

//
//defines
//

//
// Project Options
//

//
// 1 means control runs on C28x, otherwise it will run on CLA
//
#define C28x_CORE  1
#define CLA_CORE   2
#define CONTROL_RUNNING_ON 1

//
//SFRA Options
//
#define SFRA_FREQ_START     20

#define SFRA_SWEEP_SPEED    1           // the speed of the sweep

//
//SFRA step Multiply = 10^(1/No of steps per decade(40))
//
#define SFRA_FREQ_STEP_MULTIPLY         ((float32_t)(1.263355))
#define SFRA_AMPLITUDE                  ((float32_t)(0.005))
#define SFRA_FREQ_LENGTH                22

#define SFRA_GUI_SCI_BASE               SCIA_BASE
#define SFRA_GUI_VBUS_CLK               50000000
#define SFRA_GUI_SCI_BAUDRATE           57600

#define SFRA_GUI_SCIRX_GPIO             43
#define SFRA_GUI_SCITX_GPIO             42

#define SFRA_GUI_SCIRX_GPIO_PIN_CONFIG  GPIO_43_SCIRXDA
#define SFRA_GUI_SCITX_GPIO_PIN_CONFIG  GPIO_42_SCITXDA

//
// if the following #define is set to 1 SFRA GUI indicates status on an LED
// otherwise LED code is ignored
//
#define SFRA_GUI_LED_INDICATOR          1
#define SFRA_GUI_LED_GPIO               31
#define SFRA_GUI_LED_GPIO_PIN_CONFIG    GPIO_31_GPIO31

/* USER CODE END (section: User_Section) */

#ifdef __cplusplus
}
#endif                                  /* extern "C" */

#endif // end of SFRA_SETTINGS_H definition
