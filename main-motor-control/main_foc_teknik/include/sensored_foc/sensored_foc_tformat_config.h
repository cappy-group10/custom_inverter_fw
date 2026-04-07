//#############################################################################
// File Name        :   fcl_tformat_f28004x_config.h
//#############################################################################
//
// Contains all the initialization, data declarations and setup for tformat
// encoder interface. This file serves are a template for using PM_tformat
// Library to interface and incorporates all the encoder and library specific
// initializations and other important aspects of usage.
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

#ifndef FCL_TFORMAT_F28004X_CONFIG_H
#define FCL_TFORMAT_F28004X_CONFIG_H

#include "PM_tformat_Include.h"
#include "driverlib.h"
#include "device.h"

//
// Clock Frequency = SYSCLK/(4*TFORMAT_FREQ_DIVIDER) ==> (2.5MHz)
//
#define TFORMAT_FREQ_DIVIDER    20

extern void tformat_setupGPIO(void);
extern void tformat_configXBAR(void);
extern uint16_t tformat_exCommands(void);
extern uint16_t tformat_checkCRC (uint16_t expectCRC5, uint16_t receiveCRC5);
extern void tformat_configEPWM4(void);

extern interrupt void spiRxFIFOISR(void);
extern void tformat_init(void);
extern void tformat_error();

extern inline void readTformatEncPosition(void);

//
// Encoder connection status
//
#define  ENC_OPEN   1       // Encoder is connected correctly
#define  ENC_CLOSE  0       // Encoder is not connected

//
// Set T-Format resolution
// 17-bit resolution per turn
//
#define  TFORMAT_ENCODER_STEPS_PER_TURN  (float32_t)((int32_t)1<<17)

//
// GPIO assignment for serial encoder interface
//
#define  ENCODER_SPI_BASE    SPIB_BASE

#define  ENC_CLK_PWM_PIN     7
#define  ENC_CLK_PWM_CFG     GPIO_7_EPWM4B
#define  ENC_SPI_SIMO_PIN    24
#define  ENC_SPI_SIMO_CFG    GPIO_24_SPISIMOB
#define  ENC_SPI_SOMI_PIN    25
#define  ENC_SPI_SOMI_CFG    GPIO_25_SPISOMIB
#define  ENC_SPI_CLK_PIN     26
#define  ENC_SPI_CLK_CFG     GPIO_26_SPICLKB
#define  ENC_SPI_STE_PIN     27
#define  ENC_SPI_STE_CFG     GPIO_27_SPISTEB
#define  ENC_TXEN_PIN        34
#define  ENC_TXEN_CFG        GPIO_34_OUTPUTXBAR1
#define  ENC_PWREN_PIN       10             // Set switch on ControlCard

#endif // end of FCL_TFORMAT_F28004X_CONFIG_H definition
