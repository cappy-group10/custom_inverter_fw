//#############################################################################
// $Copyright:
// Copyright (C) 2017-2025 Texas Instruments Incorporated
//     http://www.ti.com/ ALL RIGHTS RESERVED
// $
//#############################################################################

//------------------------------------------------------------------------------
//  Software:       Motor Control SDK
//
// FILE:    fcl_cpu_code_dm.c
//
// TITLE:   FCL motor handler functions from CPU
//
// Target Family: F2837x/F28004x/F28P55x/F28P65x
//
//-----------------------------------------------------------------------------


//
// includes
//

#include "cpu_cla_shared_dm.h"
#include "fcl_cpu_cla_dm.h"
#include "fcl_cla_dm.h"

#ifndef __cplusplus
    #ifdef __TI_COMPILER_VERSION__
        #if __TI_COMPILER_VERSION__ >= 15009000
            #pragma CODE_SECTION(FCL_runPICtrl_M1,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runComplexCtrl_M1,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runPICtrlWrap_M1,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runComplexCtrlWrap_M1,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runQEPWrap_M1,".TI.ramfunc");

            #pragma CODE_SECTION(FCL_runPICtrl_M2,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runComplexCtrl_M2,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runPICtrlWrap_M2,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runComplexCtrlWrap_M2,".TI.ramfunc");
            #pragma CODE_SECTION(FCL_runQEPWrap_M2,".TI.ramfunc");

            #pragma CODE_SECTION(FCL_resetController,".TI.ramfunc");
        #else
            #pragma CODE_SECTION(FCL_runPICtrl_M1,"ramfuncs");
            #pragma CODE_SECTION(FCL_runComplexCtrl_M1,"ramfuncs");
            #pragma CODE_SECTION(FCL_runPICtrlWrap_M1,"ramfuncs");
            #pragma CODE_SECTION(FCL_runComplexCtrlWrap_M1,"ramfuncs");
            #pragma CODE_SECTION(FCL_runQEPWrap_M1,"ramfuncs");

            #pragma CODE_SECTION(FCL_runPICtrl_M2,"ramfuncs");
            #pragma CODE_SECTION(FCL_runComplexCtrl_M2,"ramfuncs");
            #pragma CODE_SECTION(FCL_runPICtrlWrap_M2,"ramfuncs");
            #pragma CODE_SECTION(FCL_runComplexCtrlWrap_M2,"ramfuncs");
            #pragma CODE_SECTION(FCL_runQEPWrap_M2,"ramfuncs");

            #pragma CODE_SECTION(FCL_resetController,"ramfuncs");
        #endif
    #endif
#endif

FCL_Vars_t fclVars[2];
#pragma DATA_SECTION(fclVars, "ClaData");

#ifndef ONEbySQRT3
#define  ONEbySQRT3   (0.57735026918963)    /* 1/sqrt(3) */
#endif

//
// FCL MACRO implementing CLARKE transform on currents using pointers to
// registers used in both PI CONTROL and COMPLEX CONTROL
// -assigned to clarke1 struct for user access (adds 20nS to FCL_CLARKE_STYLE_1)
//
#define FCL_CLARKE_STYLE_11()                                                  \
    clarke1.Alpha = (float32_t)((int16_t)(HWREGH(pMotor->curA_PPBRESULT)) *    \
                            FCL_params.adcScale);                              \
    clarke1.Beta  = ((clarke1.Alpha +                                          \
              (2.0 * ((float32_t)((int16_t)(HWREGH(pMotor->curB_PPBRESULT))) * \
              FCL_params.adcScale))) * ONEbySQRT3);

//
// FCL MACRO implementing CLARKE transform on currents using pointers to
// registers used in both PI CONTROL and COMPLEX CONTROL
//
#define FCL_CLARKE_STYLE_1()                                                   \
    clarke1Alpha = (float32_t)((int16_t)HWREGH(pMotor->curA_PPBRESULT) *       \
                         pMotor->FCL_params.adcScale);                         \
    clarke1Beta  = ((clarke1Alpha +                                            \
               (2.0 * ((float32_t)((int16_t)HWREGH(pMotor->curB_PPBRESULT)) *  \
               pMotor->FCL_params.adcScale))) * ONEbySQRT3);

//
// FCL MACRO implementing CLARKE transform on currents using bit field type
// register access used in both PI CONTROL and COMPLEX CONTROL
//
#define FCL_CURRENT3_A  (IFB_LEMV_PPB * FCL_params.adcScale)
#define FCL_CURRENT3_B  (IFB_LEMW_PPB * FCL_params.adcScale)

#define FCL_CLARKE_STYLE_3()                                                   \
        clarke1Alpha = (FCL_CURRENT3_A);                                       \
        clarke1Beta  = ((clarke1Alpha + (2.0 * FCL_CURRENT3_B)) * ONEbySQRT3);

//
// FCL MACRO implementing PARK on position feed back and CLARKE on currents
// used in both PI CONTROL and COMPLEX CONTROL
//
// Motor 1
//
#define M1_FCL_POSITION_CURRENT_CLARKE_MACRO()                                 \
/*                                                                             \
 *-----------------------------------------------------------------------------\
 *  Wait for QEP sense to complete (Position encoder suite module)             \
 * ----------------------------------------------------------------------------\
 */                                                                            \
    /* SETGPIO18_HIGH;      */                                                 \
    /* check CLA1_1 status   */                                                \
    /* below line took 2 additional cycles vs bitfield style*/                 \
    /*while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX1) == false);*/\
    while(PieCtrlRegs.PIEIFR11.bit.INTx1 == 0);                                \
    /* SETGPIO18_LOW;      */                                                  \
/*                                                                             \
 *  ---------------------------------------------------------------------------\
 *  Connect inputs of the PARK module and call the park trans. macro           \
 *  ---------------------------------------------------------------------------\
 */                                                                            \
    park1Sine   = __sinpuf32(fclVars[0].pangle);                               \
    park1Cosine = __cospuf32(fclVars[0].pangle);                               \
/*                                                                             \
 * ----------------------------------------------------------------------------\
 * Measure phase currents, and normalize to (-1,+1).                           \
 * Connect inputs of the CLARKE module and call the clarke xform macro         \
 * ----------------------------------------------------------------------------\
 */                                                                            \
    /*  SETGPIO18_HIGH; */                                                     \
    /* below line took 2 additional cycles vs bitfield style*/                 \
    /*while((HWREGH(adcBasePhaseW) & ADC_INTFLG_ADCINT1) == false);*/          \
    while(pMotor->AdcIntFlag->bit.ADCINT1 == 0);                               \
    /* asm(" NOP");   */                                                       \
    /* SETGPIO18_LOW; */                                                       \
    FCL_CLARKE_STYLE_1();

//
// Motor 2
//
#define M2_FCL_POSITION_CURRENT_CLARKE_MACRO()                                 \
/*                                                                             \
 *-----------------------------------------------------------------------------\
 *  Wait for QEP sense to complete (Position encoder suite module)             \
 * ----------------------------------------------------------------------------\
 */                                                                            \
    /* SETGPIO18_HIGH; */                                                      \
    /* check CLA1_5 status   */                                                \
    /* below line took 2 additional cycles vs bitfield style*/                 \
    /*while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX5) == false);*/\
    while(PieCtrlRegs.PIEIFR11.bit.INTx5 == 0);                                \
    /* SETGPIO18_LOW; */                                                       \
/*                                                                             \
 *  ---------------------------------------------------------------------------\
 *  Connect inputs of the PARK module and call the park trans. macro           \
 *  ---------------------------------------------------------------------------\
 */                                                                            \
    park1Sine   = __sinpuf32(fclVars[1].pangle);                          \
    park1Cosine = __cospuf32(fclVars[1].pangle);                          \
/*                                                                             \
 * ----------------------------------------------------------------------------\
 * Measure phase currents, and normalize to (-1,+1).                           \
 * Connect inputs of the CLARKE module and call the clarke xform macro         \
 * ----------------------------------------------------------------------------\
 */                                                                            \
    /* SETGPIO18_HIGH; */                                                      \
    /* below line took 2 additional cycles vs bitfield style*/                 \
    /*while((HWREGH(adcBasePhaseW) & ADC_INTFLG_ADCINT2) == false);*/          \
    while(pMotor->AdcIntFlag->bit.ADCINT2 == 0);                               \
    /* asm(" NOP");   */                                                       \
    /* SETGPIO18_LOW; */                                                       \
    FCL_CLARKE_STYLE_1();

//
// FCL MACRO implementing PWM updates using pointers access to registers
// used in both PI CONTROL and COMPLEX CONTROL
//
#define FCL_PWM_UPDATE_STYLE_1()                                               \
    EPWM_setCounterCompareValue(pwmHandle[0], EPWM_COUNTER_COMPARE_A,          \
                                (uint32_t)(svgen2.Tc + svgen2.tmp1));          \
    EPWM_setCounterCompareValue(pwmHandle[1], EPWM_COUNTER_COMPARE_A,          \
                                (uint32_t)(svgen2.Ualpha + svgen2.tmp1));      \
    EPWM_setCounterCompareValue(pwmHandle[2], EPWM_COUNTER_COMPARE_A,          \
                                (uint32_t)(svgen2.Tb + svgen2.tmp1));

//
// FCL MACRO implementing PWM updates using direct address access to registers
// (or pointers) used in both PI CONTROL and COMPLEX CONTROL
// This macro saves about 40ns of cycle time
//
#define FCL_PWM_UPDATE_STYLE_2()                                               \
    *(volatile uint32_t *)(0x406A) = (uint32_t)(svgen2.Tc + svgen2.tmp1);      \
    *(volatile uint32_t *)(0x416A) = (uint32_t)(svgen2.Ualpha + svgen2.tmp1);  \
    *(volatile uint32_t *)(0x426A) = (uint32_t)(svgen2.Tb + svgen2.tmp1);

#define FCL_PWM_UPDATE_STYLE_3()                                               \
    *(pMotor->pwmCompA) = (uint32_t)(svgen2.Tc + svgen2.tmp1);                 \
    *(pMotor->pwmCompB) = (uint32_t)(svgen2.Ualpha + svgen2.tmp1);             \
    *(pMotor->pwmCompC) = (uint32_t)(svgen2.Tb + svgen2.tmp1);

// for test
//#define FCL_PWM_UPDATE_STYLE_3()                                               \
//    *(pMotor->pwmCompC) = (uint32_t)(svgen2.Tc + svgen2.tmp1);                 \
//    *(pMotor->pwmCompA) = (uint32_t)(svgen2.Ualpha + svgen2.tmp1);             \
//    *(pMotor->pwmCompB) = (uint32_t)(svgen2.Tb + svgen2.tmp1);


// FCL MACRO implementing SVGEN and PWM updates
// used in both PI CONTROL and COMPLEX CONTROL
//
#define FCL_SVGEN_PWM_PDATE_MACRO()                                            \
/*                                                                             \
 * ----------------------------------------------------------------------------\
 * Call the space vector gen. macro                                            \
 * ----------------------------------------------------------------------------\
 */                                                                            \
    svgen2.Tb = (svgen2.Ubeta - svgen2.Ualpha) / 2;                            \
    svgen2.Tc = svgen2.Tb - svgen2.Ubeta;                                      \
                                                                               \
    svgen2.tmp2 = __fmax(__fmax(svgen2.Ualpha,                                 \
                                       svgen2.Tc), svgen2.Tb);                 \
    svgen2.tmp2 += __fmin(__fmin(svgen2.Ualpha,                                \
                                        svgen2.Tc), svgen2.Tb);                \
                                                                               \
    svgen2.tmp1  =  ((-svgen2.tmp2) / 2) +                                     \
                             pMotor->FCL_params.carrierMid;                    \
                                                                               \
/*                                                                             \
* -----------------------------------------------------------------------------\
* Computed Duty and Write to CMPA register                                     \
* -----------------------------------------------------------------------------\
*/                                                                             \
     FCL_PWM_UPDATE_STYLE_3();

//
// To do instrumentation from library use below
//
extern volatile uint16_t FCL_cycleCount[2];

#define READ_FCL_COUNT_M1                                                      \
    FCL_cycleCount[0] = HWREGH(M1_U_PWM_BASE + EPWM_O_TBCTR);

#define READ_FCL_COUNT_M2                                                      \
    FCL_cycleCount[1] = HWREGH(M2_U_PWM_BASE + EPWM_O_TBCTR);

//
//  Function to initialize PWMs for the FCL operation, this will be called by
//  the user application during the initialization or setup process
//
void FCL_initPWM(MOTOR_Vars_t *ptrMotor,
                 uint32_t basePhaseU, uint32_t basePhaseV, uint32_t basePhaseW)
{
    EPWM_disableCounterCompareShadowLoadMode(basePhaseU,
                                             EPWM_COUNTER_COMPARE_A);

    EPWM_disableCounterCompareShadowLoadMode(basePhaseV,
                                             EPWM_COUNTER_COMPARE_A);

    EPWM_disableCounterCompareShadowLoadMode(basePhaseW,
                                             EPWM_COUNTER_COMPARE_A);

    ptrMotor->pwmCompA = (uint32_t *)(basePhaseU + EPWM_O_CMPA);
    ptrMotor->pwmCompB = (uint32_t *)(basePhaseV + EPWM_O_CMPA);
    ptrMotor->pwmCompC = (uint32_t *)(basePhaseW + EPWM_O_CMPA);

    return;
}

//
// initialize ADC for 2 current sensors
//
void FCL_initADC_2I(MOTOR_Vars_t *ptrMotor, uint32_t basePhaseW,
                    uint32_t resultBaseV, ADC_PPBNumber baseV_PPB,
                    uint32_t resultBaseW, ADC_PPBNumber baseW_PPB)
{
    uint16_t ppbCurV, ppbCurW;

    ppbCurV = (uint16_t)(ADC_PPBxRESULT_OFFSET_BASE + (uint16_t)baseV_PPB * 2);
    ppbCurW = (uint16_t)(ADC_PPBxRESULT_OFFSET_BASE + (uint16_t)baseW_PPB * 2);

    ptrMotor->curA_PPBRESULT = resultBaseV + ppbCurV;
    ptrMotor->curB_PPBRESULT = resultBaseW + ppbCurW;

    ptrMotor->AdcIntFlag = (union ADCINTFLG_REG *)(basePhaseW + ADC_O_INTFLG);

    return;
}

//
// initialize ADC for 3 current sensors
//
void FCL_initADC_3I(MOTOR_Vars_t *ptrMotor, uint32_t basePhaseW,
                    uint32_t resultBaseA, ADC_PPBNumber baseA_PPB,
                    uint32_t resultBaseB, ADC_PPBNumber baseB_PPB,
                    uint32_t resultBaseC, ADC_PPBNumber baseC_PPB)
{
    uint16_t ppbCurA, ppbCurB, ppbCurC;

    ppbCurA = (uint16_t)(ADC_PPBxRESULT_OFFSET_BASE + (uint16_t)baseA_PPB * 2);
    ppbCurB = (uint16_t)(ADC_PPBxRESULT_OFFSET_BASE + (uint16_t)baseB_PPB * 2);
    ppbCurC = (uint16_t)(ADC_PPBxRESULT_OFFSET_BASE + (uint16_t)baseC_PPB * 2);

    ptrMotor->curA_PPBRESULT = resultBaseA + ppbCurA;
    ptrMotor->curB_PPBRESULT = resultBaseB + ppbCurB;
    ptrMotor->curC_PPBRESULT = resultBaseC + ppbCurC;

    ptrMotor->AdcIntFlag = (union ADCINTFLG_REG *)(basePhaseW + ADC_O_INTFLG);

    return;
}

//
// initialize QEP
//
void FCL_initQEP(MOTOR_Vars_t *ptrMotor, const uint32_t baseA)
{
    ptrMotor->ptrFCL->ptrQEP = (struct EQEP_REGS *)baseA;

    return;
}

//
// This function is called to reset the FCL variables and is useful when user
// wants to stop the motor and restart the motor
//
void FCL_resetController(MOTOR_Vars_t *ptrMotor)
{
    ptrMotor->pi_id.carryOver = 0;
    ptrMotor->pi_id.out = 0;
    ptrMotor->D_cpu.carryOver = 0;
    ptrMotor->D_cpu.idErr = 0;
    ptrMotor->D_cpu.iqErr = 0;

    ptrMotor->ptrFCL->pi_iq.carryOver = 0;
    ptrMotor->ptrFCL->pi_iq.out = 0;
    ptrMotor->ptrFCL->Q_cla.carryOver = 0;
    ptrMotor->ptrFCL->Q_cla.idErr = 0;
    ptrMotor->ptrFCL->Q_cla.iqErr = 0;
    ptrMotor->ptrFCL->speedWePrev = 0;
}

//
//  This function returns a 32-bit constant and for this version the value
//  returned is 0x00000006
//  Ver   Date       Platform              Description   Location
//   1   07/2016  IDDK / TMDSCNCDF28379D   Beta MSS      \FCL\v01_00_00_00
//   2   03/2017  IDDK / TMDSCNCDF28379D   cSUITE GA     \FCL\v02_00_00_00
//   3   11/2017  F28379DXL/ GaN/ DRV8305  Beta MSS      \FCL\v03_00_00_02
//   4   03/2018  F28379DXL/ GaN/ DRV8305  cSUITE GA     \FCL_SFRA\v01_00_00_00
//   5   03/2019  IDDK / TMDSCNCDF28379D   MCSDK_1
//   6   06/2019  IDDK / TMDSCNCDF28379D   MCSDK_2
//   7   09/2019  IDDK / F28838x/F2837x/F28004x, MCSDK_V21,
//                                         support absolute encoder
//   8   09/2019  LPD-F2837x/F28004x/GaN,   MCSDK_V21, dual motor control
//
uint32_t FCL_getSwVersion(void)
{
    //
    // for this version return 0x00000008
    //
    return((uint32_t)(0x00000008));
}

//
// FCL internal function called in the complex control API
// This function implements the complex control algorithm
//
void complexCtrl_M1(MOTOR_Vars_t *pMotor)
{
    // SETGPIO18_HIGH; // only for debug

    pMotor->pi_id.out += pMotor->D_cpu.kDirect *
                         (fclVars[0].Q_cla.idErr * pMotor->D_cpu.cosWTs -
                          fclVars[0].Q_cla.iqErr * pMotor->D_cpu.sinWTs) +
                          pMotor->D_cpu.carryOver ;

    CLAMP_MACRO(pMotor->pi_id);

    // SETGPIO18_LOW; // only for debug

    return;
}

void complexCtrl_M2(MOTOR_Vars_t *pMotor)
{
    // SETGPIO18_HIGH; // only for debug

    pMotor->pi_id.out += pMotor->D_cpu.kDirect *
                         (fclVars[1].Q_cla.idErr * pMotor->D_cpu.cosWTs -
                          fclVars[1].Q_cla.iqErr * pMotor->D_cpu.sinWTs) +
                          pMotor->D_cpu.carryOver ;

    CLAMP_MACRO(pMotor->pi_id);

    // SETGPIO18_LOW; // only for debug

    return;
}

//
// Function that performs the PI Control as part of the Fast Current Loop
//
#pragma CODE_ALIGN(FCL_runPICtrl_M1, 2)
#pragma FUNCTION_OPTIONS(FCL_runPICtrl_M1, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runPICtrl_M1, "--opt_for_speed")

void FCL_runPICtrl_M1(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    SVGEN2_t            svgen2;

    //
    //  MACRO to :-
    //      1. read QEP position
    //      2. get current feed back
    //      3. do the Clarke
    //
    M1_FCL_POSITION_CURRENT_CLARKE_MACRO();

    //
    //  PARK Transformation
    //  Connect inputs of the PI module and
    //  call the PI IQ controller macro in CLA
    //
    fclVars[0].pi_iq.err = fclVars[0].pi_iq.ref -
                ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));

    pMotor->pi_id.err = pMotor->pi_id.ref -
                ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    //
    // Connect inputs of the PI module and call the PI ID controller macro
    // CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_2);
    //
    Cla1ForceTask2();

    FCL_PI_MACRO(pMotor->pi_id)             // Id loop - PI controller - CPU

    register float32_t  piidc, piids;

    piidc = pMotor->pi_id.out * park1Cosine;
    piids = pMotor->pi_id.out * park1Sine;

    //
    //  Wait for PI IQ calc in CLA (CLA1_2) to complete
    //
    // SETGPIO18_HIGH; // only for debug
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX2) == false);
    // SETGPIO18_LOW;  // only for debug

    //
    //  Perform the inverse park and connect inputs of the SVGEN_DQ module
    //
    svgen2.Ualpha = ( piidc - (fclVars[0].pi_iq.out * park1Sine)) *
            pMotor->FCL_params.carrierMid;

    svgen2.Ubeta  = ((fclVars[0].pi_iq.out * park1Cosine) + piids) *
            pMotor->FCL_params.cmidsqrt3;

    //
    //  MACRO to :-
    //      1. do SVGEN
    //      2. do PWMupdates
    //
    FCL_SVGEN_PWM_PDATE_MACRO();

    READ_FCL_COUNT_M1;

    return;
}

//
//  Wrap up function to be called by the user application at the completion of
// Fast Current Loop in PI Control Mode
//
#pragma CODE_ALIGN(FCL_runPICtrlWrap_M1, 2)
#pragma FUNCTION_OPTIONS(FCL_runPICtrlWrap_M1, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runPICtrlWrap_M1, "--opt_for_speed")

void FCL_runPICtrlWrap_M1(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase = pMotor->FCL_params.Vdcbus * 1.15 / 2;
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //   this wrap function and .CarryOver carries it to the next iteration
    //   Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //   change much between iterations - equation tweaked to fit here
    //

    //
    //  CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_4);
    //
    Cla1ForceTask4();

    //
    // Update PI ID parameters
    //
    pMotor->pi_id.Kp = pMotor->FCL_params.Ld * invZbase *
            pMotor->FCL_params.wccD;

    pMotor->pi_id.Ki = pMotor->FCL_params.Rd * invZbase *
            pMotor->FCL_params.wccD * pMotor->FCL_params.tSamp;

    pMotor->pi_id.Kerr = (pMotor->pi_id.Ki * 0.5F) + pMotor->pi_id.Kp;
    pMotor->pi_id.KerrOld = (pMotor->pi_id.Ki * 0.5F) - pMotor->pi_id.Kp;
    pMotor->pi_id.carryOver = (pMotor->pi_id.err * pMotor->pi_id.KerrOld);

    //
    // Update PI IQ parameters
    //
    fclVars[0].pi_iq.Kp = pMotor->FCL_params.Lq * invZbase *
            pMotor->FCL_params.wccQ;
    fclVars[0].pi_iq.Ki = pMotor->FCL_params.Rq * invZbase *
            pMotor->FCL_params.wccQ * pMotor->FCL_params.tSamp;

    fclVars[0].pi_iq.Kerr = (fclVars[0].pi_iq.Ki * 0.5) +
            fclVars[0].pi_iq.Kp;

    fclVars[0].pi_iq.KerrOld = (fclVars[0].pi_iq.Ki * 0.5) -
            fclVars[0].pi_iq.Kp;

    fclVars[0].pi_iq.carryOver = (fclVars[0].pi_iq.err *
            fclVars[0].pi_iq.KerrOld) +
                   ( pMotor->FCL_params.BemfK *
                           (pMotor->speed.Speed - fclVars[0].speedWePrev));

    fclVars[0].speedWePrev = pMotor->speed.Speed;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->pi_id.fbk = pMotor->pi_id.ref - pMotor->pi_id.err;

    fclVars[0].pi_iq.fbk = fclVars[0].pi_iq.ref -
            fclVars[0].pi_iq.err;

    //
    //give enough time before clearing INTx4 for CLA TASK 4 completion
    //
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX4) == false);

    //
    // clear CLA task flags 1, 2, and 4
    //
    HWREGH(PIECTRL_BASE + PIE_O_IFR11) &= ~(PIE_IFR11_INTX1 |
                                            PIE_IFR11_INTX2 |
                                            PIE_IFR11_INTX4 );
    return;
}

//
// The functions for motor_1, task 1, 2, 3, and 4 for motor_1
//

//
// Function that performs the Complex control as part of the Fast Current Loop
//
#pragma CODE_ALIGN(FCL_runComplexCtrl_M1, 2)
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrl_M1, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrl_M1, "--opt_for_speed")

void FCL_runComplexCtrl_M1(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    register float32_t  piidc, piids;
    SVGEN2_t            svgen2;

    //
    //  MACRO to :-
    //      1. read QEP position
    //      2. get current feed back
    //      3. do the Clarke
    //
    M1_FCL_POSITION_CURRENT_CLARKE_MACRO();

    //
    //    PARK Transformation
    //    Connect inputs of the CMPLX module and call the CMPLX controller
    //
    fclVars[0].Q_cla.iqErr = fclVars[0].pi_iq.ref -
                  ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));
    fclVars[0].Q_cla.idErr = pMotor->pi_id.ref -
                  ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    // SETGPIO18_HIGH; // only for debug
    Cla1ForceTask3();               // Iq loop - complex control - CLA
    // SETGPIO18_LOW;  //only for debug

    //
    // Id loop - complex control - CPU
    //  complexCtrl(pMotor);
    //
    pMotor->pi_id.out += pMotor->D_cpu.kDirect *
                         (fclVars[0].Q_cla.idErr * pMotor->D_cpu.cosWTs -
                          fclVars[0].Q_cla.iqErr * pMotor->D_cpu.sinWTs) +
                          pMotor->D_cpu.carryOver;

    CLAMP_MACRO(pMotor->pi_id);

    piidc = pMotor->pi_id.out * park1Cosine;
    piids = pMotor->pi_id.out * park1Sine;

    //
    //  Wait for CMPLX IQ calc in CLA to complete
    //
    // SETGPIO18_HIGH;  // only for debug
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX3) == false);
    // SETGPIO18_LOW;   // only for debug

    //
    //  Perform the inverse park and connect inputs of the SVGEN_DQ module
    //
    svgen2.Ualpha = ( piidc - (fclVars[0].pi_iq.out * park1Sine)) *
            pMotor->FCL_params.carrierMid;

    svgen2.Ubeta = ((fclVars[0].pi_iq.out * park1Cosine) + piids) *
            pMotor->FCL_params.cmidsqrt3;

    //
    //  MACRO to :-
    //      1. do SVGEN
    //      2. do PWMupdates
    //
    FCL_SVGEN_PWM_PDATE_MACRO();

    READ_FCL_COUNT_M1;

    return;
}

//
// Wrap up function to be called by the user application at the completion of
// Fast Current Loop in Complex Control Mode
//
#pragma CODE_ALIGN(FCL_runComplexCtrlWrap_M1, 2)
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrlWrap_M1, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrlWrap_M1, "--opt_for_speed")

void FCL_runComplexCtrlWrap_M1(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase    = pMotor->FCL_params.Vdcbus * 1.15 / 2;
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    float32_t WTs      = pMotor->speed.Speed *
            pMotor->FCL_params.Wbase * pMotor->FCL_params.tSamp;

    //
    //  CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_4);
    //
    Cla1ForceTask4();

    pMotor->D_cpu.cosWTs  = __cos(WTs);
    fclVars[0].Q_cla.cosWTs = pMotor->D_cpu.cosWTs;

    pMotor->D_cpu.sinWTs  = __sin(WTs);
    fclVars[0].Q_cla.sinWTs = pMotor->D_cpu.sinWTs;

    pMotor->D_cpu.expVal =
            expf(-(pMotor->FCL_params.Rd / pMotor->FCL_params.Ld) *
                 pMotor->FCL_params.tSamp);

    fclVars[0].Q_cla.expVal =
            expf(-(pMotor->FCL_params.Rq / pMotor->FCL_params.Lq) *
                 pMotor->FCL_params.tSamp);

    pMotor->D_cpu.kDirect = pMotor->FCL_params.Rd * invZbase *
            __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccD *
                                       pMotor->FCL_params.tSamp)) /
                                       (1.0F - pMotor->D_cpu.expVal);

    fclVars[0].Q_cla.kDirect = pMotor->FCL_params.Rq * invZbase *
            __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccQ *
                                      pMotor->FCL_params.tSamp)) /
                                      (1.0F - fclVars[0].Q_cla.expVal);

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //    this wrap function and 'carryOver' carries it to the next iteration
    // Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //    change much between iterations - equation tweaked to fit here
    //
    pMotor->D_cpu.carryOver = -(pMotor->D_cpu.kDirect *
            fclVars[0].Q_cla.idErr * pMotor->D_cpu.expVal);

    fclVars[0].Q_cla.carryOver = -(fclVars[0].Q_cla.kDirect *
            fclVars[0].Q_cla.iqErr * fclVars[0].Q_cla.expVal) +
                    (pMotor->FCL_params.BemfK *
                           (pMotor->speed.Speed - fclVars[0].speedWePrev));

    fclVars[0].speedWePrev = pMotor->speed.Speed;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->pi_id.fbk = pMotor->pi_id.ref - fclVars[0].Q_cla.idErr;

    fclVars[0].pi_iq.fbk = fclVars[0].pi_iq.ref -
            fclVars[0].Q_cla.iqErr;

    //
    //give enough time before clearing INTx4 for CLA TASK 4 completion
    //
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX4) == false);

    //
    // clear CLA task flags 1, 3 and 4
    //
    HWREGH(PIECTRL_BASE + PIE_O_IFR11) &= ~(PIE_IFR11_INTX1 |
                                            PIE_IFR11_INTX3 |
                                            PIE_IFR11_INTX4 );
    return;
}

//
// Function that performs the PI Control as part of the Fast Current Loop
//
#pragma CODE_ALIGN(FCL_runPICtrl_M2, 2)
#pragma FUNCTION_OPTIONS(FCL_runPICtrl_M2, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runPICtrl_M2, "--opt_for_speed")

void FCL_runPICtrl_M2(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    SVGEN2_t            svgen2;

    //
    //  MACRO to :-
    //      1. read QEP position
    //      2. get current feed back
    //      3. do the Clarke
    //
    M2_FCL_POSITION_CURRENT_CLARKE_MACRO();

    //
    //  PARK Transformation
    //  Connect inputs of the PI module and
    //  call the PI IQ controller macro in CLA
    //
    fclVars[1].pi_iq.err = fclVars[1].pi_iq.ref -
                ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));

    pMotor->pi_id.err = pMotor->pi_id.ref -
                ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    //
    //    Connect inputs of the PI module and call the PI ID controller macro
    //  CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_6);
    //
    Cla1ForceTask6();

    FCL_PI_MACRO(pMotor->pi_id)             // Id loop - PI controller - CPU

    register float32_t  piidc, piids;

    piidc = pMotor->pi_id.out * park1Cosine;
    piids = pMotor->pi_id.out * park1Sine;

    //
    //  Wait for PI IQ calc in CLA (CLA1_6) to complete
    //
    // SETGPIO18_HIGH; // only for debug
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX6) == false);
    // SETGPIO18_LOW;  // only for debug

    //
    //  Perform the inverse park and connect inputs of the SVGEN_DQ module
    //
    svgen2.Ualpha = ( piidc - (fclVars[1].pi_iq.out * park1Sine)) *
            pMotor->FCL_params.carrierMid;

    svgen2.Ubeta  = ((fclVars[1].pi_iq.out * park1Cosine) + piids) *
            pMotor->FCL_params.cmidsqrt3;

    //
    //  MACRO to :-
    //      1. do SVGEN
    //      2. do PWMupdates
    //
    FCL_SVGEN_PWM_PDATE_MACRO();

    READ_FCL_COUNT_M2;

    return;
}

//
// Duplicate the functions for motor_2, but change some codes for CLA tasks
// trigger and its status check
// task 5, 6, 7, and 8 for motor_2
//

//
//  Wrap up function to be called by the user application at the completion of
// Fast Current Loop in PI Control Mode
//
#pragma CODE_ALIGN(FCL_runPICtrlWrap_M2, 2)
#pragma FUNCTION_OPTIONS(FCL_runPICtrlWrap_M2, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runPICtrlWrap_M2, "--opt_for_speed")

void FCL_runPICtrlWrap_M2(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase = pMotor->FCL_params.Vdcbus * 1.15 / 2;
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //   this wrap function and .CarryOver carries it to the next iteration
    // Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //   change much between iterations - equation tweaked to fit here
    //

    //
    //  CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_8);
    //
    Cla1ForceTask8();

    //
    // Update PI ID parameters
    //
    pMotor->pi_id.Kp = pMotor->FCL_params.Ld * invZbase *
            pMotor->FCL_params.wccD;

    pMotor->pi_id.Ki = pMotor->FCL_params.Rd * invZbase *
            pMotor->FCL_params.wccD * pMotor->FCL_params.tSamp;

    pMotor->pi_id.Kerr = (pMotor->pi_id.Ki * 0.5F) + pMotor->pi_id.Kp;
    pMotor->pi_id.KerrOld = (pMotor->pi_id.Ki * 0.5F) - pMotor->pi_id.Kp;
    pMotor->pi_id.carryOver = (pMotor->pi_id.err * pMotor->pi_id.KerrOld);

    //
    // Update PI IQ parameters
    //
    fclVars[1].pi_iq.Kp = pMotor->FCL_params.Lq * invZbase *
            pMotor->FCL_params.wccQ;
    fclVars[1].pi_iq.Ki = pMotor->FCL_params.Rq * invZbase *
            pMotor->FCL_params.wccQ * pMotor->FCL_params.tSamp;

    fclVars[1].pi_iq.Kerr = (fclVars[1].pi_iq.Ki * 0.5) +
            fclVars[1].pi_iq.Kp;

    fclVars[1].pi_iq.KerrOld = (fclVars[1].pi_iq.Ki * 0.5) -
            fclVars[1].pi_iq.Kp;

    fclVars[1].pi_iq.carryOver = (fclVars[1].pi_iq.err *
            fclVars[1].pi_iq.KerrOld) +
                   ( pMotor->FCL_params.BemfK *
                          (pMotor->speed.Speed - fclVars[1].speedWePrev));

    fclVars[1].speedWePrev = pMotor->speed.Speed;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->pi_id.fbk = pMotor->pi_id.ref - pMotor->pi_id.err;

    fclVars[1].pi_iq.fbk =  fclVars[1].pi_iq.ref -
            fclVars[1].pi_iq.err;

    //
    // give enough time before clearing INTx8 for CLA task 8 completion
    //
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX8) == false);

    //
    // clear CLA task flags 5, 6, and 8
    //
    HWREGH(PIECTRL_BASE + PIE_O_IFR11) &= ~(PIE_IFR11_INTX5 |
                                            PIE_IFR11_INTX6 |
                                            PIE_IFR11_INTX8 );
    return;
}


//
// Function that performs the Complex control as part of the Fast Current Loop
//
#pragma CODE_ALIGN(FCL_runComplexCtrl_M2, 2)
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrl_M2, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrl_M2, "--opt_for_speed")

void FCL_runComplexCtrl_M2(MOTOR_Vars_t *pMotor)
{
    register float32_t  clarke1Alpha, clarke1Beta;
    register float32_t  park1Sine, park1Cosine;
    register float32_t  piidc, piids;
    SVGEN2_t            svgen2;

    //
    //  MACRO to :-
    //      1. read QEP position
    //      2. get current feed back
    //      3. do the Clarke
    //
    M2_FCL_POSITION_CURRENT_CLARKE_MACRO();

    //
    //    PARK Transformation
    //    Connect inputs of the CMPLX module and call the CMPLX controller
    //
    fclVars[1].Q_cla.iqErr = fclVars[1].pi_iq.ref -
                  ((clarke1Beta * park1Cosine) - (clarke1Alpha * park1Sine));
    fclVars[1].Q_cla.idErr = pMotor->pi_id.ref -
                  ((clarke1Alpha * park1Cosine) + (clarke1Beta * park1Sine));

    //
    // Connect inputs of the CC module and call the CC controller macro
    //
    // SETGPIO18_HIGH;  // only for debug
    Cla1ForceTask7();               // Iq loop - complex control - CLA task 7
    // SETGPIO18_LOW;   // only for debug

    //
    // Id loop - complex control - CPU
    // complexCtrl(pMotor);
    //
    pMotor->pi_id.out += pMotor->D_cpu.kDirect *
                         (fclVars[1].Q_cla.idErr * pMotor->D_cpu.cosWTs -
                          fclVars[1].Q_cla.iqErr * pMotor->D_cpu.sinWTs) +
                          pMotor->D_cpu.carryOver ;

    CLAMP_MACRO(pMotor->pi_id);

    piidc = pMotor->pi_id.out * park1Cosine;
    piids = pMotor->pi_id.out * park1Sine;

    //
    //  Wait for CMPLX IQ calc in CLA task 7 to complete
    //
    // SETGPIO18_HIGH;      // only for debug
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX7) == false);
    // SETGPIO18_LOW;       // only for debug

    //
    //  Perform the inverse park and connect inputs of the SVGEN_DQ module
    //
    svgen2.Ualpha = ( piidc - (fclVars[1].pi_iq.out * park1Sine)) *
            pMotor->FCL_params.carrierMid;

    svgen2.Ubeta = ((fclVars[1].pi_iq.out * park1Cosine) + piids) *
            pMotor->FCL_params.cmidsqrt3;

    //
    //  MACRO to :-
    //      1. do SVGEN
    //      2. do PWMupdates
    //
    FCL_SVGEN_PWM_PDATE_MACRO();

    READ_FCL_COUNT_M2;

    return;
}

//
// Wrap up function to be called by the user application at the completion of
// Fast Current Loop in Complex Control Mode
//
#pragma CODE_ALIGN(FCL_runComplexCtrlWrap_M2, 2)
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrlWrap_M2, "--auto_inline")
#pragma FUNCTION_OPTIONS(FCL_runComplexCtrlWrap_M2, "--opt_for_speed")

void FCL_runComplexCtrlWrap_M2(MOTOR_Vars_t *pMotor)
{
    float32_t Vbase    = pMotor->FCL_params.Vdcbus * 1.15 / 2;
    float32_t invZbase = pMotor->FCL_params.Ibase / Vbase;

    float32_t WTs      = pMotor->speed.Speed *
            pMotor->FCL_params.Wbase * pMotor->FCL_params.tSamp;

    //
    //  CLA_forceTasks(CLA1_BASE, CLA_TASKFLAG_8);
    //
    Cla1ForceTask8();           // CLA task8

    pMotor->D_cpu.cosWTs  = __cos(WTs);
    fclVars[1].Q_cla.cosWTs = pMotor->D_cpu.cosWTs;

    pMotor->D_cpu.sinWTs  = __sin(WTs);
    fclVars[1].Q_cla.sinWTs = pMotor->D_cpu.sinWTs;

    pMotor->D_cpu.expVal =
            expf(-(pMotor->FCL_params.Rd / pMotor->FCL_params.Ld) *
                 pMotor->FCL_params.tSamp);

    fclVars[1].Q_cla.expVal =
            expf(-(pMotor->FCL_params.Rq / pMotor->FCL_params.Lq) *
                 pMotor->FCL_params.tSamp);

    pMotor->D_cpu.kDirect = pMotor->FCL_params.Rd * invZbase *
            __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccD *
                                       pMotor->FCL_params.tSamp)) /
                                       (1.0F - pMotor->D_cpu.expVal);

    fclVars[1].Q_cla.kDirect = pMotor->FCL_params.Rq * invZbase *
            __sqrt(2.0F - 2.0F * __cos(pMotor->FCL_params.wccQ *
                                       pMotor->FCL_params.tSamp)) /
                                       (1.0F - fclVars[1].Q_cla.expVal);

    //
    // To save CPU cycles and speed up calcn, carry over math is done within
    //    this wrap function and 'carryOver' carries it to the next iteration
    // Bemf calc is rolled in to the Q calcs as the speed and flux does not
    //    change much between iterations - equation tweaked to fit here
    //
    pMotor->D_cpu.carryOver = -(pMotor->D_cpu.kDirect *
            fclVars[1].Q_cla.idErr * pMotor->D_cpu.expVal);

    fclVars[1].Q_cla.carryOver = -(fclVars[1].Q_cla.kDirect *
            fclVars[1].Q_cla.iqErr * fclVars[1].Q_cla.expVal) +
                    (pMotor->FCL_params.BemfK *
                         (pMotor->speed.Speed - fclVars[1].speedWePrev));

    fclVars[1].speedWePrev = pMotor->speed.Speed;

    //
    // to pass on the id and iq current feedback back to user,
    // update them in the wrap function
    //
    pMotor->pi_id.fbk = pMotor->pi_id.ref - fclVars[1].Q_cla.idErr;

    fclVars[1].pi_iq.fbk = fclVars[1].pi_iq.ref -
            fclVars[1].Q_cla.iqErr;

    //
    // give enough time before clearing INTx8 for CLA TASK 8 completion
    //
    while((HWREGH(PIECTRL_BASE + PIE_O_IFR11) & PIE_IFR11_INTX8) == false);

    //
    // clear CLA task flags 5, 7 and 8
    //
    HWREGH(PIECTRL_BASE + PIE_O_IFR11) &= ~(PIE_IFR11_INTX5 |
                                            PIE_IFR11_INTX7 |
                                            PIE_IFR11_INTX8 );
    return;
}

//
// end of this file
//
