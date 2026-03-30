//#############################################################################
//
// FILE:    dual_axis_servo_drive_user.h
//
// TITLE:   motor parameters definition
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

#ifndef DUAL_AXIS_SERVO_DRIVE_USER_H
#define DUAL_AXIS_SERVO_DRIVE_USER_H

//
// Include project specific include files.
//

//
// PWM, SAMPLING FREQUENCY and Current Loop Band width definitions for motor 1
// motor 2, can be set separately
//
#define DM_PWM_FREQUENCY        10   // in KHz


//
// Analog scaling with ADC
//
#define TWO_CURRENT_SENSORS         2U
#define THREE_CURRENT_SENSORS       3U
#define COUNT_CURRENT_SENSORS       TWO_CURRENT_SENSORS

#if (COUNT_CURRENT_SENSORS == TWO_CURRENT_SENSORS)
#define IS_TWO_SHUNT_DRIVE
#endif

// J3 pin24 exposes ADCINC3/CMPIN6N (CMPSS6) and J3 pin25 exposes ADCINB3/CMPIN3N (CMPSS3).
// Both channels have comparator-capable inputs, so CMPSS overcurrent protection is enabled.
#define COUNT_CURRENT_PROTECTION_CMPSS  2U

// Current-sensor analog front end: 90mV/A with an approximately 2.5V offset.
// With a 3.0V ADC reference this gives only about +5.56A of positive headroom,
// so the software current limits must stay below that unless the signal is
// level-shifted or attenuated before it reaches the MCU.
#define M1_ADC_REFERENCE_VOLTAGE            3.0f
#define M1_ADC_FULL_SCALE_COUNTS            4096.0f
#define M1_CURRENT_SENSE_SENSITIVITY        0.09f
#define M1_CURRENT_SENSE_ZERO_VOLTAGE       2.5f

#define M1_CMPSS_ZERO_COUNT                 3413U
#define M1_CURRENT_COUNTS_PER_AMP           ((M1_CURRENT_SENSE_SENSITIVITY * \
                                              M1_ADC_FULL_SCALE_COUNTS) / \
                                             M1_ADC_REFERENCE_VOLTAGE)
#define M1_CURRENT_SENSE_MAX_POS_CURRENT    ((M1_ADC_REFERENCE_VOLTAGE - \
                                              M1_CURRENT_SENSE_ZERO_VOLTAGE) / \
                                             M1_CURRENT_SENSE_SENSITIVITY)
#define M1_CURRENT_SENSE_MAX_NEG_CURRENT    (M1_CURRENT_SENSE_ZERO_VOLTAGE / \
                                             M1_CURRENT_SENSE_SENSITIVITY)

#define ADC_PU_SCALE_FACTOR         0.000244140625     // 1/2^12, 12bits ADC
#define ADC_PU_PPB_SCALE_FACTOR     0.000488281250     // 1/2^11, 12bits ADC

//
// ADC and PWM Related defines for M1
//
#ifndef IS_TWO_SHUNT_DRIVE // Only in 3-shunt configuration
// Phase U
#define M1_IU_ADC_BASE         ADCC_BASE           //C2, 
#define M1_IU_ADCRESULT_BASE   ADCCRESULT_BASE     //C2, NC: Set up based board
#define M1_IU_ADC_CH_NUM       ADC_CH_ADCIN2       //C2, NC: Set up based board
#define M1_IU_ADC_SOC_NUM      ADC_SOC_NUMBER0     //C2, NC: Set up based board
#define M1_IU_ADC_PPB_NUM      ADC_PPB_NUMBER1     //C2, NC: Set up based board

#define M1_U_CMPSS_BASE        CMPSS6_BASE         // NC: Set up based board

#endif // !IS_TWO_SHUNT_DRIVE

// Phase V
#define M1_IV_ADC_BASE         ADCC_BASE           // J3 pin24, ADCINC3
#define M1_IV_ADCRESULT_BASE   ADCCRESULT_BASE
#define M1_IV_ADC_CH_NUM       ADC_CH_ADCIN3
#define M1_IV_ADC_SOC_NUM      ADC_SOC_NUMBER0
#define M1_IV_ADC_PPB_NUM      ADC_PPB_NUMBER1
#define M1_IV_CMPSS_BASE       CMPSS6_BASE         // J3 pin24, CMPIN6N

// Phase W
#define M1_IW_ADC_BASE         ADCB_BASE           // J3 pin25, ADCINB3
#define M1_IW_ADCRESULT_BASE   ADCBRESULT_BASE
#define M1_IW_ADC_CH_NUM       ADC_CH_ADCIN3
#define M1_IW_ADC_SOC_NUM      ADC_SOC_NUMBER0
#define M1_IW_ADC_PPB_NUM      ADC_PPB_NUMBER1
#define M1_IW_CMPSS_BASE       CMPSS3_BASE         // J3 pin25, CMPIN3N

// DC Bus voltage
#define M1_VDC_ADC_BASE        ADCD_BASE           // J7 pin63, ADCIND15
#define M1_VDC_ADCRESULT_BASE  ADCDRESULT_BASE
#define M1_VDC_ADC_CH_NUM      ADC_CH_ADCIN15
#define M1_VDC_ADC_SOC_NUM     ADC_SOC_NUMBER0
#define M1_VDC_ADC_PPB_NUM     ADC_PPB_NUMBER1

// ADC trigger for current and voltage sensing
#define M1_ADC_TRIGGER_SOC     ADC_TRIGGER_EPWM1_SOCA  // NC: Set up based board

// PWM related defines

#define M1_U_PWM_BASE          EPWM1_BASE          // NC: Set up based board
#define M1_V_PWM_BASE          EPWM2_BASE          // NC: Set up based board
#define M1_W_PWM_BASE          EPWM3_BASE          // NC: Set up based board

#define M1_INT_PWM             INT_EPWM1           // NC: Set up based board

#define M1_QEP_BASE            EQEP1_BASE          // NC: Set up based board

#define M1_SPI_BASE            SPIA_BASE           // NC: Set up based board

#ifndef IS_TWO_SHUNT_DRIVE // Only in 3-shunt configuration
#define M1_IFB_U      ADC_readResult(M1_IU_ADCRESULT_BASE, M1_IU_ADC_SOC_NUM)
#define M1_IFB_U_PPB  ADC_readPPBResult(M1_IU_ADCRESULT_BASE, M1_IU_ADC_PPB_NUM)
#endif // !IS_TWO_SHUNT_DRIVE

#define M1_IFB_V      ADC_readResult(M1_IV_ADCRESULT_BASE, M1_IV_ADC_SOC_NUM)
#define M1_IFB_V_PPB  ADC_readPPBResult(M1_IV_ADCRESULT_BASE, M1_IV_ADC_PPB_NUM)

#define M1_IFB_W      ADC_readResult(M1_IW_ADCRESULT_BASE, M1_IW_ADC_SOC_NUM)
#define M1_IFB_W_PPB  ADC_readPPBResult(M1_IW_ADCRESULT_BASE, M1_IW_ADC_PPB_NUM)

#define M1_VDC      ADC_readResult(M1_VDC_ADCRESULT_BASE, M1_VDC_ADC_SOC_NUM)
#define M1_VDC_PPB  ADC_readPPBResult(M1_VDC_ADCRESULT_BASE, M1_VDC_ADC_PPB_NUM)

//
// Motor_1 Parameters
//

// PWM, SAMPLING FREQUENCY and Current Loop Band width definitions
//
#define M1_PWM_FREQUENCY           10   // in KHz

#if(SAMPLING_METHOD == SINGLE_SAMPLING)
#define M1_ISR_FREQUENCY           (M1_PWM_FREQUENCY)

#elif(SAMPLING_METHOD == DOUBLE_SAMPLING)
#define M1_ISR_FREQUENCY           (2 * M1_PWM_FREQUENCY)

#endif

//
// EnDat position-update definitions.
// The dedicated EnDat producer runs independently at 4x the motor PWM rate.
//
#define ENDAT_PRODUCER_RATE_RATIO       4U


//
// Keep PWM Period same between single sampling and double sampling
//
#define M1_INV_PWM_TICKS        (((SYSTEM_FREQUENCY/2.0)/M1_PWM_FREQUENCY)*1000)
#define M1_INV_PWM_DB            (200.0)
#define M1_QEP_UNIT_TIMER_TICKS  (SYSTEM_FREQUENCY/(2*M1_PWM_FREQUENCY) * 1000)

#define ENDAT_POSITION_UPDATE_FREQ      (ENDAT_PRODUCER_RATE_RATIO * M1_PWM_FREQUENCY)
#define ENDAT_PRODUCER_PWM_BASE         EPWM9_BASE
#define ENDAT_PRODUCER_INT              INT_EPWM9
#define ENDAT_PRODUCER_PWM_TICKS        (M1_INV_PWM_TICKS / ENDAT_PRODUCER_RATE_RATIO)
#define ENDAT_PRODUCER_PHASE_TICKS      (ENDAT_PRODUCER_PWM_TICKS / 4U)

#define M1_INV_PWM_TBPRD         (M1_INV_PWM_TICKS / 2)
#define M1_INV_PWM_HALF_TBPRD    (M1_INV_PWM_TBPRD / 2)
#define M1_SAMPLING_FREQ         (M1_ISR_FREQUENCY * 1000)
#define M1_CUR_LOOP_BANDWIDTH    (2.0f * PI * M1_SAMPLING_FREQ / 100)

#define M1_TPWM_CARRIER          (1000.0 / (M1_PWM_FREQUENCY))    //in uSec

//
// FCL Computation time predetermined from library
// tests on F2837xD
//
#define M1_FCL_COMPUTATION_TIME  (1.00)  //in uS

//
// set the motor parameters to the one available
//
#define M1_ENCODER_LINES         262144 // Encoder lines for AMK
// #define M1_ENCODER_LINES         1000 // Encoder lines for Tekic

//
// Define the electrical motor parameters
//

// AMK Motor parameters (BE CAREFUL WITH UNITS)
#define M1_RS      0.0675          // Stator resistance (ohm)
#define M1_RR      NULL            // Rotor resistance (ohm)
#define M1_LS      (M1_LD + M1_LQ) / 2.0  // Stator inductance (H)
#define M1_LD      0.00024         // Stator d-axis inductance (H)
#define M1_LQ      0.00012         // Stator q-axis inductance (H)
#define M1_LR      NULL            // Rotor inductance (H)
#define M1_LM      NULL            // Magnetizing inductance (H)
#define M1_KE      18.8            // V/kU/min, Back EMF constant
// #define M1_KB      (M1_KE * 1000.0) / (60.0 * M1_POLES/2.0)  // BEMF Constant (V/Hz)
#define M1_KB      0.2256          // BEMF Constant (V/Hz)
#define M1_POLES   10              // Number of poles

// // Tekic motor parameters
// #define M1_RS      0.381334811     // Stator resistance (ohm)
// #define M1_RR      NULL            // Rotor resistance (ohm)
// #define M1_LS      0.000169791776  // Stator inductance (H)
// #define M1_LD      M1_LS           // Stator d-axis inductance (H)
// #define M1_LQ      M1_LS           // Stator q-axis inductance (H)
// #define M1_LR      NULL            // Rotor inductance (H)
// #define M1_LM      NULL            // Magnetizing inductance (H)
// #define M1_KB      0.8             // BEMF Constant (V/Hz)
// #define M1_POLES   10               // Number of poles


//
// NOTE:-
// Base voltage and base current information from TIDA-00909 doc is
// based off of an ADC that works at 3.3V reference.
// For TMS320F28379x, the reference is 3.0V. Therefore,
// the base voltage and base current values will vary.
//
// Original base current = 16.5A (for a spread of 3.3V - 1.65V = 1.65V)
// Revised base current  = 16.5*(3.0-1.65)/(3.3-1.65)
//                       = 13.5A
//
// Original base voltage = 81.5V (for a spread of 0 to 3.3V)
// Revised base voltage  = 81.5 * 3.0/3.3
//                       = 74.1V
//                       = 74.1/sqrt(3)=
//

//
// Define the base quantites
//
#define M1_BASE_VOLTAGE     346.4 // Base peak phase voltage (volt), Vdc/sqrt(3)
#define M1_BASE_CURRENT     5.0  // Conservative positive current range limit (amp)
#define M1_BASE_TORQUE      NULL  // Base torque (N.m)
#define M1_BASE_FLUX        NULL  // Base flux linkage (volt.sec/rad)
#define M1_BASE_FREQ        1000  // Base electrical frequency (Hz)
#define M1_MAXIMUM_CURRENT  5.0   // Keep below the +5.56A ADC headroom

#define M1_MAXIMUM_VOLTAGE  600.0   // DC bus maximum voltage (V)
#define M1_MINIMUM_VOLTAGE  5.0     // DC bus minimum voltage (V)

#define M1_VDCBUS_MAX       600.0   // maximum dc bus voltage for motor
#define M1_VDCBUS_MIN       24.0    // minimum dc bus voltage for motor

//
// Current sensors scaling
// 1.0pu current ==> 9.95A -> 2048 counts ==> 8A -> 1647
//
#define M1_CURRENT_SCALE(A)            ((uint16_t)((A) * M1_CURRENT_COUNTS_PER_AMP))

//
// Analog scaling with ADC
//
#define M1_ADC_PU_SCALE_FACTOR          0.000244140625     // 1/2^12
#define M1_ADC_PU_PPB_SCALE_FACTOR      0.000488281250     // 1/2^11

//
// Current Scale
//
#define M1_MAXIMUM_SCALE_CURRENT        (M1_ADC_REFERENCE_VOLTAGE / \
                                         M1_CURRENT_SENSE_SENSITIVITY)
#define M1_CURRENT_SF                   (1.0f / M1_CURRENT_COUNTS_PER_AMP)
#define M1_CURRENT_INV_SF               (M1_CURRENT_COUNTS_PER_AMP)

// FCL adcScale: maps signed PPB counts to per-unit current (1.0 pu = M1_BASE_CURRENT).
// -1 / (M1_BASE_CURRENT * M1_CURRENT_COUNTS_PER_AMP) = -1/(5.0 * 122.88) ≈ -0.001628
// The original -M1_ADC_PU_PPB_SCALE_FACTOR (-1/2048) was designed for a mid-rail
// sensor (1.5 V zero) where Ibase produced 2048 counts. With a 2.5 V zero this
// sensor only produces 614 counts at Ibase = 5 A, so the scale must be adjusted.
#define M1_FCL_ADC_SCALE    (1.0f / (M1_BASE_CURRENT * M1_CURRENT_COUNTS_PER_AMP))

//
// Voltage Scale
//
// Hardware calibration points: 140 mV at 34 V bus, 2.92 V at 600 V bus (max).
// Primary calibration from the high-voltage point (600 V / 2.92 V) is used
// because it covers the normal operating range and is the safety-critical value:
//   V_bus_full_scale = 600 V × (3.0 V / 2.92 V) = 616.4 V
// Note: at 34 V the divider reads ~15 % low (140 mV vs ~165 mV expected) due
// to resistor tolerances; this error is negligible above ~100 V.
#define M1_MAXIMUM_SCALE_VOLATGE        616.4f
#define M1_VOLTAGE_SF                   (M1_MAXIMUM_SCALE_VOLATGE / 4096.0f)
#define M1_VOLTAGE_INV_SF               (4096.0f / M1_MAXIMUM_SCALE_VOLATGE)

#endif  // end of DUAL_AXIS_SERVO_DRIVE_USER_H definition
