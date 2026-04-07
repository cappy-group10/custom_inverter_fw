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
// FILE:    dual_axis_servo_drive_user.c
//
// TITLE:   Initialize the parameter variables for motor
//
//
// Group:   C2000
//
// Target Family: F2837x/F28004x/F28P55x/F28P65x
//
//-----------------------------------------------------------------------------

//
// includes
//
#include "dual_axis_servo_drive_settings.h"
#include "dual_axis_servo_drive_user.h"
#include "dual_axis_servo_drive_hal.h"
#include "dual_axis_servo_drive.h"

#include "sfra_settings.h"

// Variables for current feedback offset calculation
float32_t K1 = 0.998;         // Offset filter coefficient K1: 0.05/(T+0.05);
float32_t K2 = 0.001999;      // Offset filter coefficient K2: T/(T+0.05);

uint16_t offsetCalCounter = 0;

//
//initMotorParameters() function enter
//
void initMotorParameters(MOTOR_Vars_t *pMotor, HAL_MTR_Handle mtrHandle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)mtrHandle;

    if(pMotor == &motorVars[0])
    {
        pMotor->ptrFCL = &fclVars[0];

        pMotor->Ts = 0.001/M1_ISR_FREQUENCY;

        pMotor->voltageLimit = (float32_t)(M1_MAXIMUM_VOLTAGE);             // V
        pMotor->currentLimit = (float32_t)(M1_MAXIMUM_CURRENT);             // A

        #if(SAMPLING_METHOD == SINGLE_SAMPLING)
        pMotor->maxModIndex = (M1_TPWM_CARRIER -
                (2 * M1_FCL_COMPUTATION_TIME)) / M1_TPWM_CARRIER;
        pMotor->FCL_params.carrierMid = pMotor->maxModIndex *
                M1_INV_PWM_HALF_TBPRD * 0x10000L;
        #elif(SAMPLING_METHOD == DOUBLE_SAMPLING)
        pMotor->maxModIndex = (M1_TPWM_CARRIER -
                (4 * M1_FCL_COMPUTATION_TIME)) / M1_TPWM_CARRIER;
        pMotor->FCL_params.carrierMid = M1_INV_PWM_HALF_TBPRD * 0x10000L;
        #endif

        pMotor->FCL_params.adcScale = -M1_ADC_PU_PPB_SCALE_FACTOR;

        pMotor->FCL_params.cmidsqrt3 =
                pMotor->FCL_params.carrierMid * sqrtf(3.0);

        pMotor->FCL_params.tSamp = (1.0F / M1_SAMPLING_FREQ);
        pMotor->FCL_params.Rd    = M1_RS;
        pMotor->FCL_params.Rq    = M1_RS;
        pMotor->FCL_params.Ld    = M1_LD;
        pMotor->FCL_params.Lq    = M1_LQ;
        pMotor->FCL_params.BemfK = M1_KB;
        pMotor->FCL_params.Ibase = M1_BASE_CURRENT;
        pMotor->FCL_params.Wbase = 2.0 * PI * M1_BASE_FREQ;
        pMotor->FCL_params.wccD  = M1_CUR_LOOP_BANDWIDTH;
        pMotor->FCL_params.wccQ  = M1_CUR_LOOP_BANDWIDTH;

        // set the number of slots in the encoder
        pMotor->ptrFCL->qep.LineEncoder = M1_ENCODER_LINES;
        pMotor->ptrFCL->qep.MechScaler = 0.25 / pMotor->ptrFCL->qep.LineEncoder;

        // set the number of pole pairs of the motor
        pMotor->ptrFCL->qep.PolePairs = M1_POLES / 2;
        pMotor->ptrFCL->qep.CalibratedAngle = 0;

        // Initialize the Speed module for speed calculation from QEP/RESOLVER
        pMotor->speed.K1 = 1 / (M1_BASE_FREQ * pMotor->Ts);

        // Low-pass cut-off frequency
        pMotor->speed.K2 = 1 / (1 + (2 * PI * pMotor->Ts * 5));
        pMotor->speed.K3 = 1 - pMotor->speed.K2;
        pMotor->speed.BaseRpm = 120 * (M1_BASE_FREQ / M1_POLES);

        // set up current and voltage scaling coefficient
        pMotor->currentScale = M1_CURRENT_SF;
        pMotor->voltageScale = M1_VOLTAGE_SF;
        pMotor->adcScale = M1_ADC_PU_SCALE_FACTOR;
        pMotor->currentInvSF = M1_CURRENT_INV_SF;
        pMotor->voltageInvSF = M1_VOLTAGE_INV_SF;

        pMotor->Vdcbus = M1_VDCBUS_MIN;
        pMotor->VdcbusMax = M1_VDCBUS_MAX;
        pMotor->VdcbusMin = M1_VDCBUS_MIN;

        pMotor->drvEnableGateGPIO = M1_EN_GATE_GPIO;
        pMotor->drvFaultTripGPIO = M1_nFAULT_GPIO;
        pMotor->drvClearFaultGPIO = M1_CLR_FAULT_GPIO;

        pMotor->currentThreshHi = 2048 +
                       scaleCurrentValue(M1_MAXIMUM_CURRENT, M1_CURRENT_INV_SF);

        pMotor->currentThreshLo = 2048 -
                       scaleCurrentValue(M1_MAXIMUM_CURRENT, M1_CURRENT_INV_SF);

        //
        // Initialize FCL library
        //

        //
        // This function initializes the ADC PPB result bases, as well as the
        // ADC module used to sample phase W. Ensure that the final argument
        // passed corresponds to the ADC base used to sample phase W on the
        // HW board
        //
        FCL_initADC_3I(pMotor, M1_IW_ADC_BASE,
                       M1_IV_ADCRESULT_BASE, M1_IV_ADC_PPB_NUM,
                       M1_IW_ADCRESULT_BASE, M1_IW_ADC_PPB_NUM,
                       M1_IU_ADCRESULT_BASE, M1_IU_ADC_PPB_NUM);

        pMotor->volDC_PPBRESULT = M1_VDC_ADCRESULT_BASE +
                ADC_PPBxRESULT_OFFSET_BASE + M1_VDC_ADC_PPB_NUM * 2;

    }
    else if(pMotor == &motorVars[1])
    {
        pMotor->ptrFCL = &fclVars[1];

        pMotor->Ts = 0.001 / M2_ISR_FREQUENCY;

        pMotor->voltageLimit = (float32_t)(M2_MAXIMUM_VOLTAGE);             // V
        pMotor->currentLimit = (float32_t)(M2_MAXIMUM_CURRENT);             // A

        #if(SAMPLING_METHOD == SINGLE_SAMPLING)
        pMotor->maxModIndex = (M2_TPWM_CARRIER -
                (2 * M2_FCL_COMPUTATION_TIME)) / M2_TPWM_CARRIER;
        pMotor->FCL_params.carrierMid = pMotor->maxModIndex *
                M2_INV_PWM_HALF_TBPRD * 0x10000L;
        #elif(SAMPLING_METHOD == DOUBLE_SAMPLING)
        pMotor->maxModIndex = (M2_TPWM_CARRIER -
                (4 * M2_FCL_COMPUTATION_TIME)) / M2_TPWM_CARRIER;
        pMotor->FCL_params.carrierMid = M2_INV_PWM_HALF_TBPRD * 0x10000L;
        #endif

        pMotor->FCL_params.adcScale = -M2_ADC_PU_PPB_SCALE_FACTOR;

        pMotor->FCL_params.cmidsqrt3 =
                pMotor->FCL_params.carrierMid * sqrtf(3.0);

        pMotor->FCL_params.tSamp = (1.0F / M2_SAMPLING_FREQ);
        pMotor->FCL_params.Rd    = M2_RS;
        pMotor->FCL_params.Rq    = M2_RS;
        pMotor->FCL_params.Ld    = M2_LD;
        pMotor->FCL_params.Lq    = M2_LQ;
        pMotor->FCL_params.BemfK = M2_KB;
        pMotor->FCL_params.Ibase = M2_BASE_CURRENT;
        pMotor->FCL_params.Wbase = 2.0 * PI * M2_BASE_FREQ;
        pMotor->FCL_params.wccD  = M2_CUR_LOOP_BANDWIDTH;
        pMotor->FCL_params.wccQ  = M2_CUR_LOOP_BANDWIDTH;

        // set the number of slots in the encoder
        pMotor->ptrFCL->qep.LineEncoder = M2_ENCODER_LINES;
        pMotor->ptrFCL->qep.MechScaler = 0.25 / pMotor->ptrFCL->qep.LineEncoder;

        // set the number of pole pairs of the motor
        pMotor->ptrFCL->qep.PolePairs = M2_POLES / 2;
        pMotor->ptrFCL->qep.CalibratedAngle = 0;

        // Initialize the Speed module for speed calculation from QEP/RESOLVER
        pMotor->speed.K1 = 1 / (M2_BASE_FREQ * pMotor->Ts);

        // Low-pass cut-off frequency
        pMotor->speed.K2 = 1 / (1 + (2 * PI * pMotor->Ts * 5));
        pMotor->speed.K3 = 1 - pMotor->speed.K2;
        pMotor->speed.BaseRpm = 120 * (M2_BASE_FREQ / M2_POLES);

        // set up current and voltage scaling coefficient
        pMotor->currentScale = M2_CURRENT_SF;
        pMotor->voltageScale = M2_VOLTAGE_SF;
        pMotor->adcScale = M2_ADC_PU_SCALE_FACTOR;
        pMotor->currentInvSF = M2_CURRENT_INV_SF;
        pMotor->voltageInvSF = M2_VOLTAGE_INV_SF;

        pMotor->Vdcbus = M2_VDCBUS_MIN;
        pMotor->VdcbusMax = M2_VDCBUS_MAX;
        pMotor->VdcbusMin = M2_VDCBUS_MIN;

        pMotor->drvEnableGateGPIO = M2_EN_GATE_GPIO;
        pMotor->drvFaultTripGPIO = M2_nFAULT_GPIO;
        pMotor->drvClearFaultGPIO = M2_CLR_FAULT_GPIO;

        pMotor->currentThreshHi = 2048 +
                       scaleCurrentValue(M2_MAXIMUM_CURRENT, M2_CURRENT_INV_SF);

        pMotor->currentThreshLo = 2048 -
                       scaleCurrentValue(M2_MAXIMUM_CURRENT, M2_CURRENT_INV_SF);

        //
        // Initialize FCL library
        //

        //
        // This function initializes the ADC PPB result bases, as well as the
        // ADC module used to sample phase W. Ensure that the final argument
        // passed corresponds to the ADC base used to sample phase W on the
        // HW board
        //
        FCL_initADC_3I(pMotor, M2_IW_ADC_BASE,
                       M2_IV_ADCRESULT_BASE, M2_IV_ADC_PPB_NUM,
                       M2_IW_ADCRESULT_BASE, M2_IW_ADC_PPB_NUM,
                       M2_IU_ADCRESULT_BASE, M2_IU_ADC_PPB_NUM );

        pMotor->volDC_PPBRESULT = M2_VDC_ADCRESULT_BASE +
                ADC_PPBxRESULT_OFFSET_BASE + M2_VDC_ADC_PPB_NUM *2;
    }

    //
    // ensure that the correct PWM base addresses are being passed to the
    // FCL library here. pwmHandle[0:2] should represent inverter phases
    // U/V/W in the hardware
    //
    FCL_initPWM(pMotor,
                obj->pwmHandle[0], obj->pwmHandle[1], obj->pwmHandle[2]);

    // ensure the correct QEP base is being passed
    FCL_initQEP(pMotor, obj->qepHandle);

    pMotor->ptrFCL->taskCount[0] = 0;
    pMotor->ptrFCL->taskCount[1] = 0;
    pMotor->ptrFCL->taskCount[2] = 0;
    pMotor->ptrFCL->taskCount[3] = 0;

    return;
}

//
// initControlVars() function enter
//
void initControlVars(MOTOR_Vars_t *pMotor)
{

    // Maximum delay rate of ramp control
    pMotor->rc.RampDelayMax = 10;

    //
    // PI Controllers Configuration
    //
    // Initialize the PI module for position
    pMotor->pi_pos.Kp = 0.5;             //10.0;
    pMotor->pi_pos.Ki = 0.001;           //T*speedLoopPrescaler/0.3;
    pMotor->pi_pos.Umax = 1.0;
    pMotor->pi_pos.Umin = -1.0;

    // Initialize the PID module for speed
    pMotor->pid_spd.param.Kp   = 0.5;
    pMotor->pid_spd.param.Ki   = 0.001;
    pMotor->pid_spd.param.Kd   = 0.0;
    pMotor->pid_spd.param.Kr   = 1.0;
    pMotor->pid_spd.param.Umax = 0.95;
    pMotor->pid_spd.param.Umin = -0.95;

    // Init PI module for ID loop
    pMotor->pi_id.Kp = 1.0;         // LS * CUR_LOOP_BW;
    pMotor->pi_id.Ki = pMotor->Ts / 0.04;      // (RS * T) * CUR_LOOP_BW;
    pMotor->pi_id.Kerr = (pMotor->pi_id.Ki * 0.5) + pMotor->pi_id.Kp;
    pMotor->pi_id.KerrOld = (pMotor->pi_id.Ki * 0.5) - pMotor->pi_id.Kp;
    pMotor->pi_id.Umax = 0.5 * pMotor->maxModIndex;
    pMotor->pi_id.Umin = -0.5 * pMotor->maxModIndex;
    pMotor->pi_id.ref = 0;
    pMotor->pi_id.err = 0;
    pMotor->pi_id.out = 0;

    // Initialize the Speed module for speed calculation from QEP/RESOLVER
    pMotor->speed.K1 = 1 / (pMotor->baseFreq * pMotor->Ts);

    // Low-pass cut-off frequency
    pMotor->speed.K2 = 1 / (1 + (2 * PI * pMotor->Ts * 5));
    pMotor->speed.K3 = 1 - pMotor->speed.K2;
    pMotor->speed.BaseRpm = 120 * (pMotor->baseFreq / pMotor->poles);

    // Init PI module for IQ loop
    pMotor->ptrFCL->pi_iq.Kp = pMotor->pi_id.Kp;
    pMotor->ptrFCL->pi_iq.Ki = pMotor->pi_id.Ki;
    pMotor->ptrFCL->pi_iq.Kerr =
            (pMotor->ptrFCL->pi_iq.Ki * 0.5) + pMotor->ptrFCL->pi_iq.Kp;

    pMotor->ptrFCL->pi_iq.KerrOld =
            (pMotor->ptrFCL->pi_iq.Ki * 0.5) - pMotor->ptrFCL->pi_iq.Kp;

    pMotor->ptrFCL->pi_iq.Umax = 0.8 * pMotor->maxModIndex;
    pMotor->ptrFCL->pi_iq.Umin = -0.8 * pMotor->maxModIndex;
    pMotor->ptrFCL->pi_iq.ref = 0;
    pMotor->ptrFCL->pi_iq.err = 0;
    pMotor->ptrFCL->pi_iq.out = 0;

    // Initialize the RAMPGEN module
    pMotor->ptrFCL->rg.StepAngleMax = pMotor->baseFreq * pMotor->Ts;
    pMotor->ptrFCL->rg.Angle = 0;
    pMotor->ptrFCL->rg.Out = 0;
    pMotor->ptrFCL->rg.Gain = 1.0;
    pMotor->ptrFCL->rg.Offset = 1.0;

    // set mock REFERENCES for Speed and current loops
    pMotor->speedRef  = 0.1;
    pMotor->IdRef     = 0;

#if(BUILDLEVEL == FCL_LEVEL5)
    pMotor->IqRef = 0.05;
#else
    pMotor->IqRef = 0.03;
#endif

    return;
}

//
// initControlVars() function enter
// This function is called to reset the FCL variables and is useful when user
// wants to stop the motor and restart the motor
void resetControlVars(MOTOR_Vars_t *pMotor)
{
    pMotor->runMotor = MOTOR_STOP;

    pMotor->pi_id.carryOver = 0.0;
    pMotor->pi_id.out = 0.0;

    pMotor->D_cpu.carryOver = 0.0;
    pMotor->D_cpu.idErr = 0.0;
    pMotor->D_cpu.iqErr = 0.0;

    pMotor->ptrFCL->lsw = ENC_ALIGNMENT;

    pMotor->ptrFCL->pi_iq.carryOver = 0.0;
    pMotor->ptrFCL->pi_iq.out = 0.0;

    pMotor->ptrFCL->Q_cla.carryOver = 0.0;
    pMotor->ptrFCL->Q_cla.idErr = 0.0;
    pMotor->ptrFCL->Q_cla.iqErr = 0.0;

    pMotor->ptrFCL->speedWePrev = 0.0;

    return;
}

//
// initControlVars() function enter
// This function is called to reset the FCL variables and is useful when user
// wants to stop the motor and restart the motor
//
void runOffsetsCalculation(MOTOR_Vars_t *pMotor)
{
    // Feedbacks OFFSET Calibration
    pMotor->offset_currentAs = 0;
    pMotor->offset_currentBs = 0;
    pMotor->offset_currentCs = 0;

    if(pMotor == &motorVars[0])
    {
        for(offsetCalCounter = 0; offsetCalCounter < 20000; offsetCalCounter++)
        {
            EPWM_clearEventTriggerInterruptFlag(halMtr[0].pwmHandle[0]);

            while(EPWM_getEventTriggerInterruptStatus(halMtr[0].pwmHandle[0]) == false);

            if(offsetCalCounter > 1000)
            {
                // Offsets in phase current sensing
                pMotor->offset_currentAs  = (K1 * pMotor->offset_currentAs) +
                        (K2 * (M1_IFB_U) * pMotor->adcScale);

                pMotor->offset_currentBs  = (K1 * pMotor->offset_currentBs) +
                        (K2 * (M1_IFB_V) * pMotor->adcScale);

                pMotor->offset_currentCs  = (K1 * pMotor->offset_currentCs) +
                        (K2 * (M1_IFB_W) * pMotor->adcScale);
            }
        }

        //
        // Read and update DC BUS voltage for FCL to use
        //
        pMotor->FCL_params.Vdcbus = getVdc(pMotor);

        //
        // Init OFFSET regs with identified values
        //

        // setting Iu offset
        ADC_setPPBReferenceOffset(M1_IU_ADC_BASE, M1_IU_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentAs * 4096.0));
        // setting Iv offset
        ADC_setPPBReferenceOffset(M1_IV_ADC_BASE, M1_IV_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentBs * 4096.0));

        // setting Iw offset
        ADC_setPPBReferenceOffset(M1_IW_ADC_BASE, M1_IW_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentCs * 4096.0));

        // setting Vdc offset
        ADC_setPPBReferenceOffset(M1_VDC_ADC_BASE, M1_VDC_ADC_PPB_NUM, 0);
    }
    else if(pMotor == &motorVars[1])
    {
        for(offsetCalCounter = 0; offsetCalCounter < 20000; offsetCalCounter++)
        {
            EPWM_clearEventTriggerInterruptFlag(halMtr[1].pwmHandle[0]);

            while(EPWM_getEventTriggerInterruptStatus(halMtr[1].pwmHandle[0]) == false);

            if(offsetCalCounter > 1000)
            {
                // Offsets in phase current sensing
                pMotor->offset_currentAs  = (K1 * pMotor->offset_currentAs) +
                        (K2 * (M2_IFB_U) * pMotor->adcScale);

                pMotor->offset_currentBs  = (K1 * pMotor->offset_currentBs) +
                        (K2 * (M2_IFB_V) * pMotor->adcScale);

                pMotor->offset_currentCs  = (K1 * pMotor->offset_currentCs) +
                        (K2 * (M2_IFB_W) * pMotor->adcScale);
            }
        }

        //
        // Read and update DC BUS voltage for FCL to use
        //
        pMotor->FCL_params.Vdcbus = getVdc(pMotor);

        //
        // Init OFFSET regs with identified values
        //

        // setting Iu offset
        ADC_setPPBReferenceOffset(M2_IU_ADC_BASE, M2_IU_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentAs * 4096.0));
        // setting Iv offset
        ADC_setPPBReferenceOffset(M2_IV_ADC_BASE, M2_IV_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentBs * 4096.0));

        // setting Iw offset
        ADC_setPPBReferenceOffset(M2_IW_ADC_BASE, M2_IW_ADC_PPB_NUM,
                                 (uint16_t)(pMotor->offset_currentCs * 4096.0));

        // setting Vdc offset
        ADC_setPPBReferenceOffset(M2_VDC_ADC_BASE, M2_VDC_ADC_PPB_NUM, 0);
    }

    pMotor->offsetDoneFlag = 1;

    return;
}

//
// End of Code
//
