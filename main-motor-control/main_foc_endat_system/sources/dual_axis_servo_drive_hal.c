//#############################################################################
//
// FILE:    dual_axis_servo_drive_hal.c
//
// TITLE:   define initialize the handle functions of device
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

//
// includes
//
#include "dual_axis_servo_drive_settings.h"
#include "dual_axis_servo_drive_hal.h"

#include "dual_axis_servo_drive.h"

#include "stdbool.h"
#include "stdint.h"


//
// SD Trip Level - scope for additional work
//
uint16_t hlt = 0x7FFF;
uint16_t llt = 0x0;

//
// These are defined by the linker file
//
extern uint32_t Cla1funcsRunStart;
extern uint32_t Cla1funcsLoadStart;
extern uint32_t Cla1funcsLoadSize;

extern uint32_t Cla1ConstRunStart;
extern uint32_t Cla1ConstLoadStart;
extern uint32_t Cla1ConstLoadSize;

//
// interrupt routines for CPU
//
extern __interrupt void motor1ControlISR(void);
extern __interrupt void motor2ControlISR(void);

//
// tasks 1-4 are owned by the FCL for motor 1
//
extern __interrupt void Cla1Task1(void);
extern __interrupt void Cla1Task2(void);
extern __interrupt void Cla1Task3(void);
extern __interrupt void Cla1Task4(void);

//
// tasks 5-8 are owned by the FCL for motor 2
//
extern __interrupt void Cla1Task5(void);
extern __interrupt void Cla1Task6(void);
extern __interrupt void Cla1Task7(void);
extern __interrupt void Cla1Task8(void);

//
// Enables interrupts
//
void HAL_enableInterrupts(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    //
    // clear pending INT event
    //
    EPWM_clearEventTriggerInterruptFlag(obj->pwmHandle[0]);

    if(handle == &halMtr[MTR_1])
    {
        //
        // Enable PWM1INT in PIE group 3
        //
        Interrupt_enable(M1_INT_PWM);
    }
    else if(handle == &halMtr[MTR_2])
    {
        //
        // Enable PWM4INT in PIE group 3
        //
        Interrupt_enable(M2_INT_PWM);        // Enable PWM1INT in PIE group 3
    }

    //
    // Enable group 3 interrupts - EPWM1 is here
    //
    Interrupt_enableInCPU(INTERRUPT_CPU_INT3);

    return;
}

//
// HAL_init()
//
HAL_Handle HAL_init(void *pMemory, const size_t numBytes)
{
    HAL_Handle handle;
    HAL_Obj *obj;

    if(numBytes < sizeof(HAL_Obj))
    {
        return((HAL_Handle)NULL);
    }

    //
    // assign the handle
    //
    handle = (HAL_Handle)pMemory;

    //
    // assign the object
    //
    obj = (HAL_Obj *)handle;

    //
    // disable watchdog
    //
    SysCtl_disableWatchdog();

    //
    // initialize the DAC handles
    //
    obj->dacHandle[0] = DACA_BASE;
    obj->dacHandle[1] = DACB_BASE;
    obj->dacHandle[2] = DACC_BASE;

    //
    // initialize CLA handle
    //
    obj->claHandle = CLA1_BASE;

    //
    // initialize SCI handle
    //
    obj->sciHandle[0] = SCIA_BASE;
    obj->sciHandle[1] = SCIB_BASE;

    //
    // initialize timer handles
    //
    obj->timerHandle[0] = CPUTIMER0_BASE;
    obj->timerHandle[1] = CPUTIMER1_BASE;
    obj->timerHandle[2] = CPUTIMER2_BASE;

    //
    // initialize ADC handle
    //
    obj->adcHandle[0] = ADCA_BASE;
    obj->adcHandle[1] = ADCB_BASE;
    obj->adcHandle[2] = ADCC_BASE;
    obj->adcHandle[3] = ADCD_BASE;

    //
    // initialize the ADC results
    //
    obj->adcResult[0] = ADCARESULT_BASE;
    obj->adcResult[1] = ADCBRESULT_BASE;
    obj->adcResult[2] = ADCCRESULT_BASE;
    obj->adcResult[3] = ADCDRESULT_BASE;

    return(handle);
} // end of HAL_init() function

//
// Initializes the hardware abstraction layer (HAL) object for motors
//
HAL_MTR_Handle HAL_MTR_init(void *pMemory, const size_t numBytes)
{
    HAL_MTR_Handle handle;
    HAL_MTR_Obj *obj;

    if(numBytes < sizeof(HAL_MTR_Obj))
    {
        return((HAL_MTR_Handle)NULL);
    }

    //
    // assign the handle
    //
    handle = (HAL_MTR_Handle)pMemory;

    //
    // assign the object
    //
    obj = (HAL_MTR_Obj *)handle;

    if(handle == &halMtr[MTR_1])
    {
        //
        // initialize SPI handle
        //
        obj->spiHandle = M1_SPI_BASE;

        //
        // initialize PWM handles for motor_1
        //
        obj->pwmHandle[0] = M1_U_PWM_BASE;
        obj->pwmHandle[1] = M1_V_PWM_BASE;
        obj->pwmHandle[2] = M1_W_PWM_BASE;

        //
        // initialize CMPSS handle
        //
        obj->cmpssHandle[0] = M1_U_CMPSS_BASE;
        obj->cmpssHandle[1] = M1_V_CMPSS_BASE;
        obj->cmpssHandle[2] = M1_W_CMPSS_BASE;

        //
        // initialize QEP driver
        //
        obj->qepHandle = M1_QEP_BASE;
    }
    else if(handle == &halMtr[MTR_2])
    {
        //
        // initialize SPI handle
        //
        obj->spiHandle = M2_SPI_BASE;

        //
        // initialize PWM handles for motor_2
        //
        obj->pwmHandle[0] = M2_U_PWM_BASE;
        obj->pwmHandle[1] = M2_V_PWM_BASE;
        obj->pwmHandle[2] = M2_W_PWM_BASE;

        //
        // initialize CMPSS handle for motor_2
        //
        obj->cmpssHandle[0] = M2_U_CMPSS_BASE;
        obj->cmpssHandle[1] = M2_V_CMPSS_BASE;
        obj->cmpssHandle[2] = M2_W_CMPSS_BASE;

        //
        // initialize QEP driver
        //
        obj->qepHandle = M2_QEP_BASE;
    }

     return(handle);
} // end of HAL_MTR_init() function

//
// sets the HAL parameters for motor
//
void HAL_setMotorParams(HAL_MTR_Handle handle)
{
    //
    // setup the PWMs
    //
    HAL_setupMotorPWMs(handle);

    //
    // setup the CMPSS
    //
    HAL_setupCMPSS(handle);

    //
    // setup the eqep
    //
    HAL_setupQEP(handle);

    return;
}

//
// sets the HAL parameters
//
void HAL_setParams(HAL_Handle handle)
{

    HAL_Obj *obj = (HAL_Obj *)handle;

    //
    // Make sure the LSPCLK divider is set to divide by 4
    //
    SysCtl_setLowSpeedClock(SYSCTL_LSPCLK_PRESCALE_4); // 50MHz for SFRA

    //
    // Disable pin locks and enable internal pullups.
    //
    Device_initGPIO();

    //
    // Sets up the CLA
    //
    HAL_setupCLA(handle);

    //
    // Initialize the PIE control registers to their default state.
    // The default state is all PIE interrupts disabled and flags
    // are cleared.
    //
    Interrupt_initModule();

    //
    // Clear all interrupts and initialize PIE vector table:
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    // This will populate the entire table, even if the interrupt
    // is not used in this example.  This is useful for debug purposes.
    //
    Interrupt_initVectorTable();

    //
    // Timing sync for background loops
    //
    HAL_setupCpuTimer(obj->timerHandle[0], MICROSEC_50);    // A tasks
    HAL_setupCpuTimer(obj->timerHandle[1], MICROSEC_100);   // B tasks
    HAL_setupCpuTimer(obj->timerHandle[2], MICROSEC_150);   // C tasks

    //
    // Sets up the GPIO (General Purpose I/O) pins
    //
    HAL_setupGPIOs(handle);

#ifdef DACOUT_EN
    //
    // Sets up the DAC
    //
    HAL_setupDACs(handle);
#endif  //DACOUT_EN

    //
    // Sets up the ADC
    //
    HAL_setupADCs(handle);



    return;
}


//
// Configure ADC
//
void HAL_setupADCs(HAL_Handle handle)
{
    HAL_Obj *obj = (HAL_Obj *)handle;

    uint16_t cnt;

    //
    // setup ADC modules A, B, C, D
    //
    for(cnt = 0; cnt < 4; cnt++)
    {
        //
        // Set 12-bit single ended conversion mode
        //
        ADC_setMode(obj->adcHandle[cnt],
                    ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);

        // Set main clock scaling factor (100MHz max clock for the ADC module)
        // Set the ADC clock to 50MHz
        ADC_setPrescaler(obj->adcHandle[cnt], ADC_CLK_DIV_4_0);

        // set the ADC interrupt pulse generation to end of conversion
        ADC_setInterruptPulseMode(obj->adcHandle[cnt], ADC_PULSE_END_OF_CONV);

        // enable the ADC
        ADC_enableConverter(obj->adcHandle[cnt]);

        // set priority of SOCs
        ADC_setSOCPriority(obj->adcHandle[cnt], ADC_PRI_ALL_HIPRI);
    }

    // delay to allow ADCs to power up
    DEVICE_DELAY_US(1500U);

    //-------------------------------------------------------------------------
    // For motor 1
    //-------------------------------------------------------------------------
    // Shunt Motor Currents (M1-Iu) @ C2
    // SOC0 will convert pin C2, sample window in SYSCLK cycles
    // trigger on ePWM1 SOCA/C
    ADC_setupSOC(M1_IU_ADC_BASE, M1_IU_ADC_SOC_NUM,
                 M1_ADC_TRIGGER_SOC, M1_IU_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC0
    ADC_setupPPB(M1_IU_ADC_BASE, M1_IU_ADC_PPB_NUM, M1_IU_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M1_IU_ADC_BASE, M1_IU_ADC_PPB_NUM, 0);

    // Shunt Motor Currents (M1-Iv) @ B2
    // SOC0 will convert pin B2, sample window in SYSCLK cycles
    // trigger on ePWM1 SOCA/C
    ADC_setupSOC(M1_IV_ADC_BASE, M1_IV_ADC_SOC_NUM,
                 M1_ADC_TRIGGER_SOC, M1_IV_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC0
    ADC_setupPPB(M1_IV_ADC_BASE, M1_IV_ADC_PPB_NUM, M1_IV_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M1_IV_ADC_BASE, M1_IV_ADC_PPB_NUM, 0);

    // Shunt Motor Currents (M1-Iw) @ A2
    // SOC0 will convert pin A2, sample window in SYSCLK cycles
    // trigger on ePWM1 SOCA/C
    ADC_setupSOC(M1_IW_ADC_BASE, M1_IW_ADC_SOC_NUM,
                 M1_ADC_TRIGGER_SOC, M1_IW_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC0
    ADC_setupPPB(M1_IW_ADC_BASE, M1_IW_ADC_PPB_NUM, M1_IW_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M1_IW_ADC_BASE, M1_IW_ADC_PPB_NUM, 0);

    // Phase Voltage (M1-Vfb-dc) @ D14
    // SOC1 will convert pin D14, sample window in SYSCLK cycles
    // trigger on ePWM1 SOCA/C
    ADC_setupSOC(M1_VDC_ADC_BASE, M1_VDC_ADC_SOC_NUM,
                 M1_ADC_TRIGGER_SOC, M1_VDC_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC0
    ADC_setupPPB(M1_VDC_ADC_BASE, M1_VDC_ADC_PPB_NUM, M1_VDC_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M1_VDC_ADC_BASE, M1_VDC_ADC_PPB_NUM, 0);

    //-------------------------------------------------------------------------
    // For motor 2
    //-------------------------------------------------------------------------
    // Shunt Motor Currents (M2-Iu) @ C4
    // SOC2 will convert pin C2, sample window in SYSCLK cycles
    // trigger on ePWM4 SOCA/C
    ADC_setupSOC(M2_IU_ADC_BASE, M2_IU_ADC_SOC_NUM,
                 M2_ADC_TRIGGER_SOC, M2_IU_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC2
    ADC_setupPPB(M2_IU_ADC_BASE, M2_IU_ADC_PPB_NUM, M2_IU_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M2_IU_ADC_BASE, M2_IU_ADC_PPB_NUM, 0);

    // Shunt Motor Currents (M2-Iv) @ B4
    // SOC2 will convert pin B2, sample window in SYSCLK cycles
    // trigger on ePWM4 SOCA/C
    ADC_setupSOC(M2_IV_ADC_BASE, M2_IV_ADC_SOC_NUM,
                 M2_ADC_TRIGGER_SOC, M2_IV_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC2
    ADC_setupPPB(M2_IV_ADC_BASE, M2_IV_ADC_PPB_NUM, M2_IV_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M2_IV_ADC_BASE, M2_IV_ADC_PPB_NUM, 0);

    // Shunt Motor Currents (M2-Iw) @ A4
    // SOC2 will convert pin A2, sample window in SYSCLK cycles
    // trigger on ePWM4 SOCA/C
    ADC_setupSOC(M2_IW_ADC_BASE, M2_IW_ADC_SOC_NUM,
                 M2_ADC_TRIGGER_SOC, M2_IW_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC2
    ADC_setupPPB(M2_IW_ADC_BASE, M2_IW_ADC_PPB_NUM, M2_IW_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M2_IW_ADC_BASE, M2_IW_ADC_PPB_NUM, 0);

    // Phase Voltage (M2-Vfb-dc) @ D15
    // SOC3 will convert pin D15, sample window in SYSCLK cycles
    // trigger on ePWM4 SOCA/C
    ADC_setupSOC(M2_VDC_ADC_BASE, M2_VDC_ADC_SOC_NUM,
                 M2_ADC_TRIGGER_SOC, M2_VDC_ADC_CH_NUM, 14);

    // Configure PPB to eliminate subtraction related calculation
    // PPB is associated with SOC3
    ADC_setupPPB(M2_VDC_ADC_BASE, M2_VDC_ADC_PPB_NUM, M2_VDC_ADC_SOC_NUM);

    // Write zero to this for now till offset ISR is run
    ADC_setPPBCalibrationOffset(M2_VDC_ADC_BASE, M2_VDC_ADC_PPB_NUM, 0);

    return;
}

//
// setup CLA
//
void HAL_setupCLA(HAL_Handle handle)
{

    HAL_Obj *obj = (HAL_Obj *)handle;

    EALLOW;

#ifdef _FLASH
    //
    // Copy CLA code from its load address (FLASH) to CLA program RAM
    //
    // Note: during debug the load and run addresses can be
    // the same as Code Composer Studio can load the CLA program
    // RAM directly.
    //
    // The ClafuncsLoadStart, ClafuncsLoadEnd, and ClafuncsRunStart
    // symbols are created by the linker.
    //
    memcpy((uint32_t *)&Cla1funcsRunStart, (uint32_t *)&Cla1funcsLoadStart,
            (uint32_t)&Cla1funcsLoadSize);

    memcpy((uint32_t *)&Cla1ConstRunStart, (uint32_t *)&Cla1ConstLoadStart,
            (uint32_t)&Cla1ConstLoadSize);
#endif //_FLASH

    // make sure QEP access is given to CLA as Secondary master
    SysCtl_selectSecMaster(SYSCTL_SEC_MASTER_CLA, SYSCTL_SEC_MASTER_CLA);

    // Initialize and wait for CLA1ToCPUMsgRAM
    MemCfg_initSections(MEMCFG_SECT_MSGCLA1TOCPU);
    while(MemCfg_getInitStatus(MEMCFG_SECT_MSGCLA1TOCPU) != 1);

    // Initialize and wait for CPUToCLA1MsgRAM

    MemCfg_initSections(MEMCFG_SECT_MSGCPUTOCLA1);
    while(MemCfg_getInitStatus(MEMCFG_SECT_MSGCPUTOCLA1) != 1);

    // Select LS5RAM to be the programming space for the CLA
    // First configure the CLA to be the master for LS5 and then
    // set the space to be a program block

    MemCfg_setLSRAMMasterSel(MEMCFG_SECT_LS4, MEMCFG_LSRAMMASTER_CPU_CLA1);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS4, MEMCFG_CLA_MEM_PROGRAM);

    MemCfg_setLSRAMMasterSel(MEMCFG_SECT_LS5, MEMCFG_LSRAMMASTER_CPU_CLA1);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS5, MEMCFG_CLA_MEM_PROGRAM);

    // Next configure LS2RAM and LS3RAM as data spaces for the CLA
    // First configure the CLA to be the master and then
    // set the spaces to be code blocks
    MemCfg_setLSRAMMasterSel(MEMCFG_SECT_LS2, MEMCFG_LSRAMMASTER_CPU_CLA1);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS2, MEMCFG_CLA_MEM_DATA);

    MemCfg_setLSRAMMasterSel(MEMCFG_SECT_LS3, MEMCFG_LSRAMMASTER_CPU_CLA1);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS3, MEMCFG_CLA_MEM_DATA);

    // Compute all CLA task vectors
    // On Type-1 CLAs the MVECT registers accept full 16-bit task addresses as
    // opposed to offsets used on older Type-0 CLAs
#pragma diag_suppress = 770
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_1, (uint16_t)(&Cla1Task1));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_2, (uint16_t)(&Cla1Task2));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_3, (uint16_t)(&Cla1Task3));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_4, (uint16_t)(&Cla1Task4));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_5, (uint16_t)(&Cla1Task5));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_6, (uint16_t)(&Cla1Task6));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_7, (uint16_t)(&Cla1Task7));
    CLA_mapTaskVector(obj->claHandle, CLA_MVECT_8, (uint16_t)(&Cla1Task8));
#pragma diag_suppress = 770

    // Enable the IACK instruction to start a task on CLA in software
    // for all  8 CLA tasks. Also, globally enable all 8 tasks (or a
    // subset of tasks) by writing to their respective bits in the
    // MIER register
    CLA_enableIACK(obj->claHandle);
    CLA_enableTasks(obj->claHandle, CLA_TASKFLAG_ALL);

    // Enable EPWM1 INT trigger for CLA TASK1
    CLA_setTriggerSource(CLA_TASK_1, CLA_TRIGGER_EPWM1INT);

    // Enable EPWM4 INT trigger for CLA TASK5
    CLA_setTriggerSource(CLA_TASK_5, CLA_TRIGGER_EPWM4INT);

    return;
}

//
// setup CMPSS
//
void HAL_setupCMPSS(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    uint16_t cnt;

    for(cnt = 0; cnt < 3; cnt++)
    {
        // Set up COMPCTL register
        // NEG signal from DAC for COMP-H
        CMPSS_configHighComparator(obj->cmpssHandle[cnt], CMPSS_INSRC_DAC);

        // NEG signal from DAC for COMP-L, COMP-L output is inverted
        CMPSS_configLowComparator(obj->cmpssHandle[cnt],
                                  (CMPSS_INSRC_DAC | CMPSS_INV_INVERTED)) ;

        // Dig filter output ==> CTRIPH, Dig filter output ==> CTRIPOUTH
        CMPSS_configOutputsHigh(obj->cmpssHandle[cnt],
                                (CMPSS_TRIP_FILTER | CMPSS_TRIPOUT_FILTER));

        // Dig filter output ==> CTRIPL, Dig filter output ==> CTRIPOUTL
        CMPSS_configOutputsLow(obj->cmpssHandle[cnt],
                               (CMPSS_TRIP_FILTER | CMPSS_TRIPOUT_FILTER));

        // Set up COMPHYSCTL register
        // COMP hysteresis set to 2x typical value
        CMPSS_setHysteresis(obj->cmpssHandle[cnt], 2);

        // set up COMPDACCTL register
        // VDDA is REF for CMPSS DACs, DAC updated on sysclock, Ramp bypassed
        CMPSS_configDAC(obj->cmpssHandle[cnt],
                (CMPSS_DACREF_VDDA | CMPSS_DACVAL_SYSCLK | CMPSS_DACSRC_SHDW));

        // Load DACs - High and Low
        // Set DAC-H to allowed MAX +ve current
        CMPSS_setDACValueHigh(obj->cmpssHandle[cnt], 1024);

        // Set DAC-L to allowed MAX -ve current
        CMPSS_setDACValueLow(obj->cmpssHandle[cnt], 1024);

        // digital filter settings - HIGH side
        // set time between samples, max : 1023, # of samples in window,
        // max : 31, recommended : thresh > sampWin/2
        // Init samples to filter input value
        CMPSS_configFilterHigh(obj->cmpssHandle[cnt], 20, 30, 18);
        CMPSS_initFilterHigh(obj->cmpssHandle[cnt]);

        // digital filter settings - LOW side
        // set time between samples, max : 1023, # of samples in window,
        // max : 31, recommended : thresh > sampWin/2
        // Init samples to filter input value
        CMPSS_configFilterLow(obj->cmpssHandle[cnt], 20, 30, 18);
        CMPSS_initFilterLow(obj->cmpssHandle[cnt]);

        // Clear the status register for latched comparator events
        CMPSS_clearFilterLatchHigh(obj->cmpssHandle[cnt]);
        CMPSS_clearFilterLatchLow(obj->cmpssHandle[cnt]);

        // Enable CMPSS
        CMPSS_enableModule(obj->cmpssHandle[cnt]);
    }

    DEVICE_DELAY_US(500);

    return;
}

//
// Setup OCP limits and digital filter parameters of CMPSS
//
void HAL_setupCMPSS_DACValue(HAL_MTR_Handle handle,
                             uint16_t curHi, uint16_t curLo)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    uint16_t cnt;

    for(cnt = 0; cnt < 3; cnt++)
    {
        // comparator references
        // Set DAC-H to allowed MAX +ve current
        CMPSS_setDACValueHigh(obj->cmpssHandle[cnt], curHi);

        // Set DAC-L to allowed MAX -ve current
        CMPSS_setDACValueLow(obj->cmpssHandle[cnt], curLo);
    }

    return;
}

//
// Setup interrupts
//
void HAL_setupInterrupts(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;


    // Enable EPWM1 INT to generate MotorControlISR
    #if(SAMPLING_METHOD == SINGLE_SAMPLING)
    // Select INT @ ctr = 0
    EPWM_setInterruptSource(obj->pwmHandle[0],
                            EPWM_INT_TBCTR_ZERO);
    #elif(SAMPLING_METHOD == DOUBLE_SAMPLING)
    // Select INT @ ctr = 0 or ctr = prd
    EPWM_setInterruptSource(obj->pwmHandle[0],
                            EPWM_INT_TBCTR_ZERO_OR_PERIOD);
    #endif

    // Enable Interrupt Generation from the PWM module
    EPWM_enableInterrupt(obj->pwmHandle[0]);

    // This needs to be 1 for the INTFRC to work
    EPWM_setInterruptEventCount(obj->pwmHandle[0], 1);

    // Clear ePWM Interrupt flag
    EPWM_clearEventTriggerInterruptFlag(obj->pwmHandle[0]);

    if(handle == &halMtr[MTR_1])
    {
        Interrupt_register(M1_INT_PWM, &motor1ControlISR);

        // Enable AdcA-ADCINT1- to help verify EoC before result data read
        ADC_setInterruptSource(M1_IW_ADC_BASE,
                               ADC_INT_NUMBER1, M1_IW_ADC_SOC_NUM);
        ADC_enableContinuousMode(M1_IW_ADC_BASE, ADC_INT_NUMBER1);
        ADC_enableInterrupt(M1_IW_ADC_BASE, ADC_INT_NUMBER1);
    }
    else if(handle == &halMtr[MTR_2])
    {
        Interrupt_register(M2_INT_PWM, &motor2ControlISR);

        // Enable AdcA-ADCINT1- to help verify EoC before result data read
        ADC_setInterruptSource(M2_IW_ADC_BASE,
                               ADC_INT_NUMBER2, M2_IW_ADC_SOC_NUM);
        ADC_enableContinuousMode(M2_IW_ADC_BASE, ADC_INT_NUMBER2);
        ADC_enableInterrupt(M2_IW_ADC_BASE, ADC_INT_NUMBER2);
    }

    return;
}

//
// setup CPU Timer
//
void HAL_setupCpuTimer(uint32_t base, uint32_t periodCount)
{
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);  // divide by 1 (SYSCLKOUT)
    CPUTimer_setPeriod(base, periodCount);
    CPUTimer_stopTimer(base);                // Stop timer / reload / restart
    CPUTimer_setEmulationMode(base,
                              CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_reloadTimerCounter(base);       // Reload counter with period value
    CPUTimer_resumeTimer(base);

    return;
}

#ifdef DACOUT_EN
//
// setup DAC
//
void HAL_setupDACs(HAL_Handle handle)
{

    HAL_Obj *obj = (HAL_Obj *)handle;

    //
    // DAC-A  ---> Resolver carrier excitation
    // DAC-B  ---> General purpose display
    // DAC-C  ---> General purpose display
    //

    uint16_t cnt;

    for(cnt = 0; cnt < 3; cnt++)
    {
        // Set DAC voltage reference to VRefHi
        DAC_setReferenceVoltage(obj->dacHandle[cnt], DAC_REF_ADC_VREFHI);

        // Set DAC shadow value register
        DAC_setShadowValue(obj->dacHandle[cnt], 1024);

        //Enable DAC output
        DAC_enableOutput(obj->dacHandle[cnt]);
    }

    DAC_enableOutput(obj->dacHandle[0]);   // disable DACA
    DAC_enableOutput(obj->dacHandle[1]);   // disable DACB

    //
    // Resolver carrier excitation signal additional initialization
    //

    // enable value change only on sync signal
    DAC_setLoadMode(obj->dacHandle[0], DAC_LOAD_PWMSYNC);

    // sync sel 5 means sync from pwm 6
    DAC_setPWMSyncSignal(obj->dacHandle[0], 5);

    return;
}
#endif // DACOUT_EN

//
// Sets up the GPIO (General Purpose I/O) pins
//
void HAL_setupGPIOs(HAL_Handle handle)
{
    // GPIO0->EPWM1A->UH_M1
    GPIO_setMasterCore(0, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPadConfig(0, GPIO_PIN_TYPE_STD);

    // GPIO1->EPWM1B->UL_M1
    GPIO_setMasterCore(1, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPadConfig(1, GPIO_PIN_TYPE_STD);

    // GPIO2->EPWM2A->VH_M1
    GPIO_setMasterCore(2, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPadConfig(2, GPIO_PIN_TYPE_STD);

    // GPIO3->EPWM2B->VL_M1
    GPIO_setMasterCore(3, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPadConfig(3, GPIO_PIN_TYPE_STD);

    // GPIO4->EPWM3A->WH_M1
    GPIO_setMasterCore(4, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPadConfig(4, GPIO_PIN_TYPE_STD);

    // GPIO5->EPWM3B->WL_M1
    GPIO_setMasterCore(5, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_5_EPWM3B);
    GPIO_setPadConfig(5, GPIO_PIN_TYPE_STD);

    // GPIO6->EPWM4A->UH_M2
    GPIO_setMasterCore(6, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_6_EPWM4A);
    GPIO_setPadConfig(6, GPIO_PIN_TYPE_STD);

    // GPIO7->EPWM4B->UL_M2
    GPIO_setMasterCore(7, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_7_EPWM4B);
    GPIO_setPadConfig(7, GPIO_PIN_TYPE_STD);

    // GPIO8->EPWM5A->VH_M2
    GPIO_setMasterCore(8, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_8_EPWM5A);
    GPIO_setPadConfig(8, GPIO_PIN_TYPE_STD);

    // GPIO9->EPWM5B->VL_M2
    GPIO_setMasterCore(9, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_9_EPWM5B);
    GPIO_setPadConfig(9, GPIO_PIN_TYPE_STD);

    // GPIO10->EPWM6A->WH_M2
    GPIO_setMasterCore(10, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_10_EPWM6A);
    GPIO_setPadConfig(10, GPIO_PIN_TYPE_STD);

    // GPIO11->EPWM6B->WL_M2
    GPIO_setMasterCore(11, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_11_EPWM6B);
    GPIO_setPadConfig(11, GPIO_PIN_TYPE_STD);

    // GPIO12 - CANTXB
    GPIO_setMasterCore(12, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_12_CANTXB);
    GPIO_setDirectionMode(12, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(12, GPIO_PIN_TYPE_STD);

    // GPIO14 - OT_M2
    GPIO_setMasterCore(14, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_14_GPIO14);
    GPIO_setDirectionMode(14, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(14, GPIO_PIN_TYPE_INVERT);

    // GPIO15 - Reserve for debug
    GPIO_setMasterCore(15, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_15_GPIO15);
    GPIO_setDirectionMode(15, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(15, GPIO_PIN_TYPE_STD);

    // GPIO16 - Reserve for debug
    GPIO_setMasterCore(16, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_16_GPIO16);
    GPIO_setDirectionMode(16, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(16, GPIO_PIN_TYPE_STD);

    // GPIO17 - CANRXB
    GPIO_setMasterCore(17, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_17_CANRXB);
    GPIO_setDirectionMode(17, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(17, GPIO_PIN_TYPE_STD);

    // GPIO18 reserve for debug
    GPIO_setMasterCore(18, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_18_GPIO18);
    GPIO_setPadConfig(18, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(18, GPIO_DIR_MODE_OUT);

    // GPIO19 - Input->nFault_M1
    GPIO_setMasterCore(19, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_19_GPIO19);
    GPIO_setDirectionMode(19, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(19, GPIO_PIN_TYPE_STD);

    // Setup GPIO for QEP operation
    // GPIO20->QEP1A_M1
    GPIO_setMasterCore(20, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_20_EQEP1A);
    GPIO_setDirectionMode(20, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(20, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(20, GPIO_QUAL_3SAMPLE);

    // GPIO21->QEP1B_M1
    GPIO_setMasterCore(21, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_21_EQEP1B);
    GPIO_setDirectionMode(21, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(21, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(21, GPIO_QUAL_3SAMPLE);

    // GPIO24 - OT_M1
    GPIO_setMasterCore(24, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_24_GPIO24);
    GPIO_setDirectionMode(24, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(24, GPIO_PIN_TYPE_INVERT);

    // GPIO25 - Reserve for debug
    GPIO_setMasterCore(25, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_25_GPIO25);
    GPIO_setDirectionMode(25, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(25, GPIO_PIN_TYPE_STD);

    // GPIO26 - EN_GATE_M2
    GPIO_setMasterCore(26, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_26_GPIO26);
    GPIO_writePin(26, 1);
    GPIO_setDirectionMode(26, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(26, GPIO_PIN_TYPE_PULLUP);

    // GPIO27 - WAKE_M2
    GPIO_setMasterCore(27, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_27_GPIO27);
    GPIO_setDirectionMode(27, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(27, GPIO_PIN_TYPE_STD);

    // GPIO31->LED
    GPIO_setMasterCore(31, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_31_GPIO31);
    GPIO_writePin(31, 1);
    GPIO_setDirectionMode(31, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(31, GPIO_PIN_TYPE_STD);

    // GPIO34->LED
    GPIO_setMasterCore(34, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_34_GPIO34);
    GPIO_writePin(34, 1);
    GPIO_setDirectionMode(34, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(34, GPIO_PIN_TYPE_STD);

    // GPIO40->SDAB
    GPIO_setMasterCore(40, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_40_SDAB);
    GPIO_setDirectionMode(40, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(40, GPIO_PIN_TYPE_STD);

    // GPIO41->SCLB
    GPIO_setMasterCore(41, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_41_SCLB);
    GPIO_setDirectionMode(41, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(41, GPIO_PIN_TYPE_STD);

    // GPIO42->SCITXDA
    GPIO_setMasterCore(42, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_42_SCITXDA);
    GPIO_setDirectionMode(42, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(42, GPIO_PIN_TYPE_STD);

    // GPIO43->SCIRXDA
    GPIO_setMasterCore(43, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_43_SCIRXDA);
    GPIO_setDirectionMode(43, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(43, GPIO_PIN_TYPE_STD);

    // GPIO54->EQEP2A_M2
    GPIO_setMasterCore(54, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_54_EQEP2A);
    GPIO_setDirectionMode(54, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(54, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(54, GPIO_QUAL_3SAMPLE);

    // GPIO55->EQEP2B_M2
    GPIO_setMasterCore(55, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_55_EQEP2B);
    GPIO_setDirectionMode(55, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(55, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(55, GPIO_QUAL_3SAMPLE);

    // GPIO56->SCITXDC
    GPIO_setMasterCore(56, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_56_SCITXDC);
    GPIO_setDirectionMode(56, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(56, GPIO_PIN_TYPE_STD);

    // GPIO57->EQEP2I_M2
    GPIO_setMasterCore(57, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_57_EQEP2I);
    GPIO_setDirectionMode(57, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(57, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(57, GPIO_QUAL_3SAMPLE);

    // GPIO58->SPISIMOA_M1
    GPIO_setMasterCore(58, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_58_SPISIMOA);
    GPIO_setDirectionMode(58, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(58, GPIO_PIN_TYPE_STD);

    // GPIO59->SPISOMIA_M1
    GPIO_setMasterCore(59, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_59_SPISOMIA);
    GPIO_setDirectionMode(59, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(59, GPIO_PIN_TYPE_STD);

    // GPIO60->SPICLKA_M1
    GPIO_setMasterCore(60, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_60_SPICLKA);
    GPIO_setDirectionMode(60, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(60, GPIO_PIN_TYPE_STD);

    // GPIO61->SPISTEA_M1
    GPIO_setMasterCore(61, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_61_SPISTEA);
    GPIO_setDirectionMode(61, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(61, GPIO_PIN_TYPE_STD);

    // GPIO63->SPISIMOB_M2
    GPIO_setMasterCore(63, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_63_SPISIMOB);
    GPIO_setDirectionMode(63, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(63, GPIO_PIN_TYPE_STD);

    // GPIO64->SPISOMIB_M2
    GPIO_setMasterCore(64, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_64_SPISOMIB);
    GPIO_setDirectionMode(64, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(64, GPIO_PIN_TYPE_STD);

    // GPIO65->SPICLKB_M2
    GPIO_setMasterCore(65, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_65_SPICLKB);
    GPIO_setDirectionMode(65, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(65, GPIO_PIN_TYPE_STD);

    // GPIO66->SPISTEB
    GPIO_setMasterCore(66, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_66_SPISTEB);
    GPIO_setDirectionMode(66, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(66, GPIO_PIN_TYPE_STD);

    // GPIO94->Vref
    GPIO_setMasterCore(94, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_94_GPIO94);
    GPIO_setDirectionMode(94, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(94, GPIO_PIN_TYPE_STD);

    // GPIO99->QEP1I
    GPIO_setMasterCore(99, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_99_EQEP1I);
    GPIO_setDirectionMode(99, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(99, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(99, GPIO_QUAL_3SAMPLE);

    // GPIO111->Vref
    GPIO_setMasterCore(111, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_111_GPIO111);
    GPIO_setDirectionMode(111, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(111, GPIO_PIN_TYPE_STD);

    // GPIO124->EN_GATE_M1
    GPIO_setMasterCore(124, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_124_GPIO124);
    GPIO_writePin(124, 1);
    GPIO_setDirectionMode(124, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(124, GPIO_PIN_TYPE_PULLUP);

    // GPIO125->WAKE_M1
    GPIO_setMasterCore(125, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_125_GPIO125);
    GPIO_setDirectionMode(125, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(125, GPIO_PIN_TYPE_STD);

    // GPIO139->nFault_M2
    GPIO_setMasterCore(139, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_139_GPIO139);
    GPIO_setDirectionMode(139, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(139, GPIO_PIN_TYPE_STD);

    // GPIO153->GPIO
    GPIO_setMasterCore(153, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_153_GPIO153);
    GPIO_setDirectionMode(153, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(153, GPIO_PIN_TYPE_STD);

    // GPIO154->GPIO
    GPIO_setMasterCore(154, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_154_GPIO154);
    GPIO_setDirectionMode(154, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(154, GPIO_PIN_TYPE_STD);

    // GPIO155->GPIO
    GPIO_setMasterCore(155, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_155_GPIO155);
    GPIO_setDirectionMode(155, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(155, GPIO_PIN_TYPE_STD);

    // GPIO156->GPIO
    GPIO_setMasterCore(139, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_139_GPIO139);
    GPIO_setDirectionMode(139, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(139, GPIO_PIN_TYPE_STD);

    // GPIO157->EPWM7A-DAC1
    GPIO_setMasterCore(157, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_157_EPWM7A);
    GPIO_setDirectionMode(157, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(157, GPIO_PIN_TYPE_STD);

    // GPIO158->EPWM7B-DAC2
    GPIO_setMasterCore(158, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_158_EPWM7B);
    GPIO_setDirectionMode(158, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(158, GPIO_PIN_TYPE_STD);

    // GPIO159->EPWM8A-DAC3
    GPIO_setMasterCore(159, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_159_EPWM8A);
    GPIO_setDirectionMode(159, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(159, GPIO_PIN_TYPE_STD);

    // GPIO160->EPWM8B-DAC4
    GPIO_setMasterCore(160, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_160_EPWM8B);
    GPIO_setDirectionMode(160, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(160, GPIO_PIN_TYPE_STD);

    return;
}



// Sets up the PWMs (Pulse Width Modulators) for motor
void HAL_setupMotorPWMs(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    uint16_t  cnt;
    uint16_t  halfPeriod = 0;

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // *****************************************
    // Inverter PWM configuration - PWM 1, 2, 3
    // *****************************************
    for(cnt = 0; cnt < 3; cnt++)
    {
        // Time Base SubModule Registers
        // set Immediate load
        EPWM_setPeriodLoadMode(obj->pwmHandle[cnt], EPWM_PERIOD_DIRECT_LOAD);
        EPWM_setTimeBasePeriod(obj->pwmHandle[cnt], 0);
        EPWM_setPhaseShift(obj->pwmHandle[cnt], 0);
        EPWM_setTimeBaseCounter(obj->pwmHandle[cnt], 0);
        EPWM_setTimeBaseCounterMode(obj->pwmHandle[cnt],
                                    EPWM_COUNTER_MODE_UP_DOWN);

        EPWM_setClockPrescaler(obj->pwmHandle[cnt], EPWM_CLOCK_DIVIDER_1,
                               EPWM_HSCLOCK_DIVIDER_1);

        // Counter Compare Submodule Registers
        // set duty 0% initially
        EPWM_setCounterCompareValue(obj->pwmHandle[cnt],
                                    EPWM_COUNTER_COMPARE_A, 0);
        EPWM_setCounterCompareShadowLoadMode(obj->pwmHandle[cnt],
                                             EPWM_COUNTER_COMPARE_A,
                                             EPWM_COMP_LOAD_ON_CNTR_ZERO);

        // Action Qualifier SubModule Registers
        EPWM_setActionQualifierActionComplete(obj->pwmHandle[cnt],
                                              EPWM_AQ_OUTPUT_A,
                (EPWM_ActionQualifierEventAction)(EPWM_AQ_OUTPUT_LOW_UP_CMPA |
                                                EPWM_AQ_OUTPUT_HIGH_DOWN_CMPA));

        // Active high complementary PWMs - Set up the deadband
        EPWM_setRisingEdgeDeadBandDelayInput(obj->pwmHandle[cnt],
                                             EPWM_DB_INPUT_EPWMA);
        EPWM_setFallingEdgeDeadBandDelayInput(obj->pwmHandle[cnt],
                                              EPWM_DB_INPUT_EPWMA);

        EPWM_setDeadBandDelayMode(obj->pwmHandle[cnt], EPWM_DB_RED, true);
        EPWM_setDeadBandDelayMode(obj->pwmHandle[cnt], EPWM_DB_FED, true);
        EPWM_setDeadBandDelayPolarity(obj->pwmHandle[cnt], EPWM_DB_RED,
                                      EPWM_DB_POLARITY_ACTIVE_HIGH);
        EPWM_setDeadBandDelayPolarity(obj->pwmHandle[cnt],
                                      EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);

        EPWM_setRisingEdgeDelayCount(obj->pwmHandle[cnt], EPWM_DB_DELAY_RISE);
        EPWM_setFallingEdgeDelayCount(obj->pwmHandle[cnt], EPWM_DB_DELAY_FALL);

        EPWM_enablePhaseShiftLoad(obj->pwmHandle[cnt]);

        EPWM_setCountModeAfterSync(obj->pwmHandle[cnt],
                                   EPWM_COUNT_MODE_UP_AFTER_SYNC);
        // configure sync
        EPWM_setSyncOutPulseMode(obj->pwmHandle[cnt],
                                 EPWM_SYNC_OUT_PULSE_ON_EPWMxSYNCIN);
    }

    //EPWM1->EWPM4
    SysCtl_setSyncInputConfig(SYSCTL_SYNC_IN_EPWM4,
                              SYSCTL_SYNC_IN_SRC_EPWM1SYNCOUT);

    if(handle == &halMtr[MTR_1])
    {
        halfPeriod = M1_INV_PWM_TICKS / 2;     // 100MHz EPWMCLK

        EPWM_disablePhaseShiftLoad(obj->pwmHandle[0]);

        // sync "down-stream"
        EPWM_setSyncOutPulseMode(obj->pwmHandle[0],
                                      EPWM_SYNC_OUT_PULSE_ON_COUNTER_ZERO);

        EPWM_setPhaseShift(obj->pwmHandle[0], 0);
        EPWM_setPhaseShift(obj->pwmHandle[1], 2);
        EPWM_setPhaseShift(obj->pwmHandle[2], 4);

        EPWM_setTimeBasePeriod(obj->pwmHandle[0], halfPeriod);
        EPWM_setTimeBasePeriod(obj->pwmHandle[1], halfPeriod);
        EPWM_setTimeBasePeriod(obj->pwmHandle[2], halfPeriod);
    }
    else if(handle == &halMtr[MTR_2])
    {
        halfPeriod = M2_INV_PWM_TICKS/2;     // 100MHz EPWMCLK

        EPWM_setPhaseShift(obj->pwmHandle[0], ((halfPeriod>>1) + 0));
        EPWM_setPhaseShift(obj->pwmHandle[1], ((halfPeriod>>1) + 2));
        EPWM_setPhaseShift(obj->pwmHandle[2], ((halfPeriod>>1) + 4));

        EPWM_setTimeBasePeriod(obj->pwmHandle[0], halfPeriod);
        EPWM_setTimeBasePeriod(obj->pwmHandle[1], halfPeriod);
        EPWM_setTimeBasePeriod(obj->pwmHandle[2], halfPeriod);
    }

    // Setting up link from EPWM to ADC
    // EPWM1/EPWM4 - Inverter currents at sampling frequency
    //               (@ PRD or @ (PRD&ZRO) )
#if(SAMPLING_METHOD == SINGLE_SAMPLING)
    // Select SOC from counter at ctr = prd
    EPWM_setADCTriggerSource(obj->pwmHandle[0],
                             EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
#elif(SAMPLING_METHOD == DOUBLE_SAMPLING)
    // Select SOC from counter at ctr = 0 or ctr = prd
    EPWM_setADCTriggerSource(obj->pwmHandle[0], EPWM_SOC_A,
                             EPWM_SOC_TBCTR_ZERO_OR_PERIOD);
#endif

    // Generate pulse on 1st event
    EPWM_setADCTriggerEventPrescale(obj->pwmHandle[0], EPWM_SOC_A, 1);

    // Enable SOC on A group
    EPWM_enableADCTrigger(obj->pwmHandle[0], EPWM_SOC_A);

    return;
}

//
// Configure Motor Fault Protection Against Over Current
//
void HAL_setupMotorFaultProtection(HAL_MTR_Handle handle,
                                   const float32_t currentLimit)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    uint16_t  cnt;

    EPWM_DigitalCompareTripInput tripInSet = EPWM_DC_TRIP_TRIPIN4;

    // High and Low Compare event trips
    uint16_t curHi = 0;
    uint16_t curLo = 0;

    if(handle == &halMtr[MTR_1])
    {
        tripInSet = EPWM_DC_TRIP_TRIPIN4;

        curHi = 2048 + M1_CURRENT_SCALE(currentLimit);
        curLo = 2048 - M1_CURRENT_SCALE(currentLimit);

        //Select GPIO24 as INPUTXBAR1
        XBAR_setInputPin(M1_XBAR_INPUT_NUM, M1_XBAR_INPUT_GPIO);

        // Configure TRIP 4 to OR the High and Low trips from both
        // comparator 1 & 3, clear everything first
        EALLOW;
        HWREG(XBAR_EPWM_CFG_REG_BASE + XBAR_O_TRIP4MUX0TO15CFG) = 0;
        HWREG(XBAR_EPWM_CFG_REG_BASE + XBAR_O_TRIP4MUX16TO31CFG) = 0;
        EDIS;

        // Enable Muxes for ored input of CMPSS1H and 1L, mux for Mux0x
        //cmpss1 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX00_CMPSS1_CTRIPH_OR_L);

        //cmpss3 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX04_CMPSS3_CTRIPH_OR_L);

        //cmpss6 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX10_CMPSS6_CTRIPH_OR_L);

        //inputxbar2 trip
        XBAR_setEPWMMuxConfig(XBAR_TRIP4, XBAR_EPWM_MUX01_INPUTXBAR1);

        // Disable all the muxes first
        XBAR_disableEPWMMux(XBAR_TRIP4, 0xFFFF);

        // Enable Mux 0  OR Mux 4 to generate TRIP4
        XBAR_enableEPWMMux(XBAR_TRIP4, XBAR_MUX00 | XBAR_MUX04 | XBAR_MUX10 |
                                       XBAR_MUX01);
    }
    else if(handle == &halMtr[MTR_2])
    {
        curHi = 2048 + M2_CURRENT_SCALE(currentLimit);
        curLo = 2048 - M2_CURRENT_SCALE(currentLimit);

        tripInSet = EPWM_DC_TRIP_TRIPIN5;

        //Select GPIO14 as INPUTXBAR3
        XBAR_setInputPin(M2_XBAR_INPUT_NUM, M2_XBAR_INPUT_GPIO);

        // Configure TRIP 5 to OR the High and Low trips from both
        // comparator 5, 5, and 2, clear everything first
        EALLOW;
        HWREG(XBAR_EPWM_CFG_REG_BASE + XBAR_O_TRIP5MUX0TO15CFG) = 0;
        HWREG(XBAR_EPWM_CFG_REG_BASE + XBAR_O_TRIP5MUX16TO31CFG) = 0;
        EDIS;

        // Enable Muxes for ored input of CMPSS1H and 1L, mux for Mux0x
        //cmpss5 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP5, XBAR_EPWM_MUX08_CMPSS5_CTRIPH_OR_L);

        //cmpss5 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP5, XBAR_EPWM_MUX08_CMPSS5_CTRIPH_OR_L);

        //cmpss2 - tripH or tripL
        XBAR_setEPWMMuxConfig(XBAR_TRIP5, XBAR_EPWM_MUX02_CMPSS2_CTRIPH_OR_L);

        //inputxbar2 trip
        XBAR_setEPWMMuxConfig(XBAR_TRIP5, XBAR_EPWM_MUX03_INPUTXBAR2);

        // Disable all the muxes first
        XBAR_disableEPWMMux(XBAR_TRIP5, 0xFFFF);

        // Enable Mux 0  OR Mux 4 to generate TRIP5
        XBAR_enableEPWMMux(XBAR_TRIP5, XBAR_MUX08 | XBAR_MUX08 | XBAR_MUX02 |
                                       XBAR_MUX03);
    }


    //
    // Configure TRIP for motor inverter phases
    //
    for(cnt = 0; cnt < 3; cnt++)
    {
        // comparator references
        // Set DAC-H to allowed MAX +ve current
        CMPSS_setDACValueHigh(obj->cmpssHandle[cnt], curHi);

        // Set DAC-L to allowed MAX -ve current
        CMPSS_setDACValueLow(obj->cmpssHandle[cnt], curLo);

        //Trip 4 is the input to the DCAHCOMPSEL
        EPWM_selectDigitalCompareTripInput(obj->pwmHandle[cnt],
                                           tripInSet,
                                           EPWM_DC_TYPE_DCAH);

        EPWM_setTripZoneDigitalCompareEventCondition(obj->pwmHandle[cnt],
                                                     EPWM_TZ_DC_OUTPUT_A1,
                                                     EPWM_TZ_EVENT_DCXH_HIGH);

        EPWM_setDigitalCompareEventSource(obj->pwmHandle[cnt], EPWM_DC_MODULE_A,
                                          EPWM_DC_EVENT_1,
                                          EPWM_DC_EVENT_SOURCE_ORIG_SIGNAL);

        EPWM_setDigitalCompareEventSyncMode(obj->pwmHandle[cnt],
                                            EPWM_DC_MODULE_A,
                                            EPWM_DC_EVENT_1,
                                            EPWM_DC_EVENT_INPUT_NOT_SYNCED);

        EPWM_enableTripZoneSignals(obj->pwmHandle[cnt], EPWM_TZ_SIGNAL_DCAEVT1);

        // Emulator Stop
        EPWM_enableTripZoneSignals(obj->pwmHandle[cnt], EPWM_TZ_SIGNAL_CBC6);

        // What do we want the OST/CBC events to do?
        // TZA events can force EPWMxA
        // TZB events can force EPWMxB

        // EPWMxA will go low
        // EPWMxB will go low
        EPWM_setTripZoneAction(obj->pwmHandle[cnt],
                               EPWM_TZ_ACTION_EVENT_TZA,
                               EPWM_TZ_ACTION_LOW);

        EPWM_setTripZoneAction(obj->pwmHandle[cnt],
                               EPWM_TZ_ACTION_EVENT_TZB,
                               EPWM_TZ_ACTION_LOW);
    }

    // clear EPWM trip flags
    DEVICE_DELAY_US(1L);

    for(cnt = 0; cnt < 3; cnt++)
    {
        // clear any spurious  OST & DCAEVT1 flags
        EPWM_clearTripZoneFlag(obj->pwmHandle[cnt], (EPWM_TZ_FLAG_OST |
                                                     EPWM_TZ_FLAG_DCAEVT1 |
                                                     EPWM_TZ_FLAG_CBC ));

        // clear any spurious  HLATCH - (not in TRIP gen path)
        CMPSS_clearFilterLatchHigh(obj->cmpssHandle[cnt]);

        // clear any spurious  LLATCH - (not in TRIP gen path)
        CMPSS_clearFilterLatchLow(obj->cmpssHandle[cnt]);
    }

    DEVICE_DELAY_US(1L);

    return;
}

void HAL_setupQEP(HAL_MTR_Handle handle)
{
    HAL_MTR_Obj *obj = (HAL_MTR_Obj *)handle;

    // Configure the decoder for quadrature count mode, counting both
    // rising and falling edges (that is, 2x resolution)
    EQEP_setDecoderConfig(obj->qepHandle, (EQEP_CONFIG_2X_RESOLUTION |
                                           EQEP_CONFIG_QUADRATURE |
                                           EQEP_CONFIG_NO_SWAP) );

    EQEP_setEmulationMode(obj->qepHandle, EQEP_EMULATIONMODE_RUNFREE);

    // Configure the position counter to be latched on a unit time out
    // and latch on rising edge of index pulse
    EQEP_setLatchMode(obj->qepHandle, (EQEP_LATCH_RISING_INDEX |
                                       EQEP_LATCH_UNIT_TIME_OUT) );

    // Configure the position counter to reset on a maximum position
    if(handle == &halMtr[MTR_1])
    {
        EQEP_setPositionCounterConfig(obj->qepHandle,
                                      EQEP_POSITION_RESET_MAX_POS,
                                      ((4 * M1_ENCODER_LINES) - 1) );

        // Enable the unit timer, setting the frequency to 10KHz
        EQEP_enableUnitTimer(obj->qepHandle, M1_QEP_UNIT_TIMER_TICKS - 1);
    }
    else if(handle == &halMtr[MTR_2])
    {
        EQEP_setPositionCounterConfig(obj->qepHandle,
                                      EQEP_POSITION_RESET_MAX_POS,
                                      ((4 * M2_ENCODER_LINES) - 1) );

        // Enable the unit timer, setting the frequency to 10KHz
        EQEP_enableUnitTimer(obj->qepHandle, M2_QEP_UNIT_TIMER_TICKS - 1);
    }

    // Disables the eQEP module position-compare unit
    EQEP_disableCompare(obj->qepHandle);

    // Configure and enable the edge-capture unit. The capture clock divider is
    // SYSCLKOUT/128. The unit-position event divider is QCLK/32.
    EQEP_setCaptureConfig(obj->qepHandle, EQEP_CAPTURE_CLK_DIV_128,
                                          EQEP_UNIT_POS_EVNT_DIV_32);

    // Enable QEP edge-capture unit
    EQEP_enableCapture(obj->qepHandle);

    // Enable UTO on QEP
    EQEP_enableInterrupt(obj->qepHandle, EQEP_INT_UNIT_TIME_OUT);

    // Enable the eQEP module
    EQEP_enableModule(obj->qepHandle);

    return;
}

// end of the file
