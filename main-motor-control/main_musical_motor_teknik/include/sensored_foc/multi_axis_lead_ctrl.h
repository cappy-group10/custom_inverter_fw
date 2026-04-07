//#############################################################################
//
// FILE:    multi_axis_lead_ctrl.h
//
// TITLE:   Include header files used in the project
//
// Group:   C2000
//
// Target Family: F2838x
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

#ifndef MULTI_AXIS_LEAD_CTRL_H
#define MULTI_AXIS_LEAD_CTRL_H

//
//! \file   solutions/common/sensored_foc/include/multi_axis_fsi_lead_ctrl.h
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
#include "device.h"

// controllers header files
#include "pi.h"                 // Include header for the PI  object
#include "rampgen.h"            // Include header for the RAMPGEN object
#include "rmp_cntl.h"           // Include header for the RMPCNTL object
#include "pid_grando.h"
#include "dlog_6ch_f.h"

#include <math.h>

#include "fcl_enum.h"
#include "sfra_settings.h"

// DCL Library header files
#include "DCLF32.h"

#include "multi_axis_lead_hal_cpu1.h"

//
// EtherCAT Constants and Functions
//

//
// Defines
//
#define ALLOCATE_TO_CM           0x1U
#define ESC_USE_INT_PHY_CLK      0x1U

#define BOOT_KEY                 0x5A000000UL
#define CM_BOOT_FREQ_125MHZ      0x7D00U
#define BOOTMODE_BOOT_TO_RAM     0x5U
#define BOOTMODE_BOOT_TO_FLASH   0x3U

#define CCARD_ECAT_RUN_LED_GPIO  146U
#define CCARD_ECAT_ERR_LED_GPIO  145U


// node number
#define SYS_NODE_NUM        FSI_NODE_NUM + 1
#define IPC_NODE_NUM        FSI_NODE_NUM

#define POS_BUF_NUM         4
#define POS_PTR_MAX         4
#define POS_CNTR_MAX        1000

//
//! \brief Enumeration for controled node
//
typedef enum
{
    SYS_NODEM  = 0,
    SYS_NODE1  = 1,
    SYS_NODE2  = 2,
    SYS_NODE3  = 3,
    SYS_NODE4  = 4,
    SYS_NODE5  = 5,
    SYS_NODE6  = 6,
    SYS_NODE7  = 7,
    SYS_NODE8  = 8
} SysNode_e;

#if(SPD_CNTLR == SPD_PID_CNTLR)
//
// Default values for controller variables
//
#define CTRL_DEFAULTS  {                                                       \
    {2.5, -2.5, 3.5, -3.5},            /*posArray[POS_BUF_NUM] */              \
    0.001,                             /* posSlewRate */                       \
    (float32_t)M_BASE_FREQ,            /* baseFreq */                          \
                                                                               \
    0.0,                               /* IdRefStart */                        \
    0.0,                               /* IqRefStart */                        \
    0.0,                               /* ctrlIdRef */                         \
    0.0,                               /* ctrlIqRef */                         \
    0.0,                               /* ctrlSpeedRef */                      \
    0.0,                               /* ctrlPosRef */                        \
                                                                               \
    0.0,                               /* ctrlSpdOut */                        \
    0.0,                               /* ctrlPosOut */                        \
    1.0,                               /* ctrlSpdMaxOut */                     \
    0.1,                               /* ctrlSpdMinOut */                     \
    1.0,                               /* ctrlPosMaxOut */                     \
                                                                               \
    0.0,                               /* IdRefSet */                          \
    0.0,                               /* IqRefSet */                          \
                                                                               \
    0.0,                               /* IdRef */                             \
    0.05,                              /* IqRef */                             \
                                                                               \
    0.1,                               /* speedSet */                          \
    0.2,                               /* positionSet */                       \
                                                                               \
    0.1,                               /* speedRef */                          \
    0.0,                               /* positionRef */                       \
                                                                               \
    0.0,                               /* posElecTheta */                      \
    0.0,                               /* posMechTheta */                      \
    0.0,                               /* speedWe */                           \
    0.0,                               /* speedMech */                         \
    0.0,                               /* torque */                            \
                                                                               \
    0.0,                               /* speedWePrev */                       \
    0.0,                               /* posMechThetaPrev */                  \
    0.0,                               /* speedWeError */                      \
    0.0,                               /* posMechThetaError */                 \
    0.0,                               /* speedWeDelta */                      \
    0.0,                               /* posMechThetaDelta */                 \
                                                                               \
    0.0,                               /* Kp_Id; */                            \
    0.0,                               /* Ki_Id; */                            \
    0.0,                               /* Kp_Iq; */                            \
    0.0,                               /* Ki_Iq; */                            \
                                                                               \
    0.0,                               /* Umax_Id; */                          \
    0.0,                               /* Umin_Id; */                          \
    0.0,                               /* Umax_Iq; */                          \
    0.0,                               /* Umin_Iq;     */                      \
                                                                               \
    6.0,                               /* curLimit */                          \
                                                                               \
    RMPCNTL_DEFAULTS,                  /* rc */                                \
                                                                               \
    {PID_TERM_DEFAULTS, PID_PARAM_DEFAULTS, PID_DATA_DEFAULTS},  /* pid_spd */ \
    PI_CONTROLLER_DEFAULTS,            /* pi_pos */                            \
                                                                               \
    POS_CNTR_MAX,                      /* posRampMax */                        \
    0,                                 /* posRampCntr */                       \
    POS_PTR_MAX,                       /* posBufMax */                         \
    0,                                 /* posBufPtr */                         \
                                                                               \
    0,                                 /* faultFlag */                         \
    0,                                 /* runState */                          \
    0,                                 /* fsiState */                          \
    CTRL_MODE_STOP,                    /* ctrlModeSet */                       \
    CTRL_MODE_STOP,                    /* ctrlModeCom */                       \
    CTRL_STOP,                         /* ctrlSateSet */                       \
    CTRL_STOP,                         /* ctrlSateCom */                       \
    CTRL_STOP                          /* ctrlSateFdb */                       \
}
#endif  // (SPD_CNTLR == SPD_PID_CNTLR)

#if(SPD_CNTLR == SPD_DCL_CNTLR)
#define CTRL_DEFAULTS  {                                                       \
    {2.5, -2.5, 3.5, -3.5},            /*posArray[POS_BUF_NUM] */              \
    0.001,                             /* posSlewRate */                       \
    (float32_t)M_BASE_FREQ,            /* baseFreq */                          \
                                                                               \
    0.0,                               /* IdRefStart */                        \
    0.0,                               /* IqRefStart */                        \
    0.0,                               /* ctrlIdRef */                         \
    0.0,                               /* ctrlIqRef */                         \
    0.0,                               /* ctrlSpeedRef */                      \
    0.0,                               /* ctrlPosRef */                        \
                                                                               \
    0.0,                               /* ctrlSpdOut */                        \
    0.0,                               /* ctrlPosOut */                        \
    1.0,                               /* ctrlSpdMaxOut */                     \
    0.1,                               /* ctrlSpdMinOut */                     \
    1.0,                               /* ctrlPosMaxOut */                     \
                                                                               \
    0.0,                               /* IdRefSet */                          \
    0.0,                               /* IqRefSet */                          \
                                                                               \
    0.0,                               /* IdRef */                             \
    0.05,                              /* IqRef */                             \
                                                                               \
    0.1,                               /* speedSet */                          \
    0.2,                               /* positionSet */                       \
                                                                               \
    0.1,                               /* speedRef */                          \
    0.0,                               /* positionRef */                       \
                                                                               \
    0.0,                               /* posElecTheta */                      \
    0.0,                               /* posMechTheta */                      \
    0.0,                               /* speedWe */                           \
    0.0,                               /* speedMech */                         \
    0.0,                               /* torque */                            \
                                                                               \
    0.0,                               /* speedWePrev */                       \
    0.0,                               /* posMechThetaPrev */                  \
    0.0,                               /* speedWeError */                      \
    0.0,                               /* posMechThetaError */                 \
    0.0,                               /* speedWeDelta */                      \
    0.0,                               /* posMechThetaDelta */                 \
                                                                               \
    0.0,                               /* Kp_Id; */                            \
    0.0,                               /* Ki_Id; */                            \
    0.0,                               /* Kp_Iq; */                            \
    0.0,                               /* Ki_Iq; */                            \
                                                                               \
    0.0,                               /* Umax_Id; */                          \
    0.0,                               /* Umin_Id; */                          \
    0.0,                               /* Umax_Iq; */                          \
    0.0,                               /* Umin_Iq;     */                      \
                                                                               \
    6.0,                               /* curLimit */                          \
                                                                               \
    RMPCNTL_DEFAULTS,                  /* rc */                                \
                                                                               \
    PI_DEFAULTS,                        /* dcl_spd */                          \
    PI_DEFAULTS,                        /* dcl_pos */                          \
                                                                               \
    POS_CNTR_MAX,                      /* posRampMax */                        \
    0,                                 /* posRampCntr */                       \
    POS_PTR_MAX,                       /* posBufMax */                         \
    0,                                 /* posBufPtr */                         \
                                                                               \
    0,                                 /* faultFlag */                         \
    0,                                 /* runState */                          \
    0,                                 /* fsiState */                          \
    CTRL_MODE_STOP,                    /* ctrlModeSet */                       \
    CTRL_MODE_STOP,                    /* ctrlModeCom */                       \
    CTRL_STOP,                         /* ctrlSateSet */                       \
    CTRL_STOP,                         /* ctrlSateCom */                       \
    CTRL_STOP                          /* ctrlSateFdb */                       \
}
#endif  // (SPD_CNTLR == SPD_DCL_CNTLR)

//
// Default values for system variables
//
#define SYS_DEFAULTS  {                                                        \
    0,                                  /* isrTicker */                        \
    (0.001 / M_ISR_FREQUENCY),          /* Ts */                               \
                                                                               \
    0.1,                                /* speedSet */                         \
    0.2,                                /* positionSet */                      \
                                                                               \
    0,                                  /* focExecutionTime_us */              \
    0,                                  /* focClrCntr */                       \
    0,                                  /* focCycleCountMax */                 \
    0,                                  /* focCycleCount */                    \
                                                                               \
    0,                                  /* peedLoopPrescaler */                \
    0,                                  /* speedLoopCount */                   \
                                                                               \
    SYS_NODE1,                          /* dacNode */                          \
                                                                               \
    SYS_NODE1,                          /* fsiNode */                          \
    SYS_NODE1,                          /* fsiNodePrev */                      \
                                                                               \
    SYS_NODE1,                          /* ecatNode */                         \
                                                                               \
    SYS_NODE1,                          /* ctrlNode */                         \
    SYS_NODE1,                          /* ctrlNodePrev */                     \
                                                                               \
    CTRL_MODE_STOP,                     /* ctrlModeSet */                      \
    CTRL_STOP,                          /* ctrlStateSet */                     \
    CTRL_SYN_DISABLE,                   /* ctrlSynSet */                       \
    ECAT_CTRL_ENABLE                    /* ecatCtrlSet */                      \
}

//
//!  \brief typedefs for ctrlVars
//
typedef struct _CTRL_Vars_t_
{
    float32_t posArray[POS_BUF_NUM];
    float32_t posSlewRate;
    float32_t baseFreq;

    float32_t IdRefStart;           // Id reference (pu) for startup
    float32_t IqRefStart;           // Iq reference (pu) for startup
    float32_t ctrlIdRef;            // Id reference (pu) to controller
    float32_t ctrlIqRef;            // Iq reference (pu) to controller
    float32_t ctrlSpeedRef;         // Speed reference (pu) to controller
    float32_t ctrlPosRef;           // Position reference (pu) to controller

    float32_t ctrlSpdOut;           // the output of speed controller
    float32_t ctrlPosOut;           // the output of position controller
    float32_t ctrlSpdMaxOut;        // the maximum output of speed controller
    float32_t ctrlSpdMinOut;        // the minimum output of speed controller
    float32_t ctrlPosMaxOut;        // the maximum output of position controller

    float32_t IdRefSet;             // Id reference setting (pu)
    float32_t IqRefSet;             // Iq reference setting (pu)

    float32_t IdRef;                // Id reference (pu)
    float32_t IqRef;                // Iq reference (pu)

    float32_t speedSet;             // For Closed Loop tests
    float32_t positionSet;          // For Position Loop tests

    float32_t speedRef;             // speed reference Closed Loop (pu)
    float32_t positionRef;          // speed reference Position Loop (pu)

    float32_t posElecTheta;
    float32_t posMechTheta;
    float32_t speedWe;
    float32_t speedMech;
    float32_t torque;

    float32_t speedWePrev;
    float32_t posMechThetaPrev;
    float32_t speedWeError;
    float32_t posMechThetaError;
    float32_t speedWeDelta;
    float32_t posMechThetaDelta;

    float32_t Kp_Id;
    float32_t Ki_Id;

    float32_t Kp_Iq;
    float32_t Ki_Iq;

    float32_t Umax_Id;
    float32_t Umin_Id;

    float32_t Umax_Iq;
    float32_t Umin_Iq;

    float32_t curLimit;

    RMPCNTL rc;                      // ramp control

#if(SPD_CNTLR == SPD_PID_CNTLR)
    PID_CONTROLLER  pid_spd;
    PI_CONTROLLER   pi_pos;
#endif  // (SPD_CNTLR == SPD_PID_CNTLR)

#if(SPD_CNTLR == SPD_DCL_CNTLR)
    DCL_PI dcl_spd;
    DCL_PI dcl_pos;
#endif  // (SPD_CNTLR == SPD_DCL_CNTLR)

    uint16_t posRampMax;
    uint16_t posRampCntr;
    uint16_t posBufMax;
    uint16_t posBufPtr;

    uint16_t faultFlag;
    uint16_t runState;
    uint16_t fsiState;
    CtrlMode_e  ctrlModeSet;
    CtrlMode_e  ctrlModeCom;
    CtrlState_e ctrlStateSet;
    CtrlState_e ctrlStateCom;
    CtrlState_e ctrlStateFdb;

} CTRL_Vars_t;

//
//!  \brief typedefs for ctrlVars
//
typedef struct _SYS_Vars_t_
{
    uint32_t isrTicker;

    float32_t Ts;                       // Samping period (sec)

    float32_t speedSet;                 // For Closed Loop tests
    float32_t positionSet;              // For Position Loop tests

    float32_t focExecutionTime_us;      // FOC execution time since sampling
    uint16_t  focClrCntr;
    uint16_t  focCycleCountMax;
    uint16_t  focCycleCount;            // FOC execution time variable

    uint16_t  speedLoopPrescaler;       // Speed loop pre scalar
    uint16_t  speedLoopCount;           // Speed loop counter

    SysNode_e  dacNode;

    SysNode_e fsiNode;
    SysNode_e fsiNodePrev;

    SysNode_e ecatNode;

    SysNode_e ctrlNode;
    SysNode_e ctrlNodePrev;

    CtrlMode_e  ctrlModeSet;
    CtrlState_e ctrlStateSet;
    CtrlSync_e  ctrlSynSet;
    ECATCtrl_e  ecatCtrlSet;
} SYS_Vars_t;

//
// Global variables used in this system
//
extern CTRL_Vars_t ctrlVars[SYS_NODE_NUM];
extern SYS_Vars_t  sysVars;

// hardware abstraction layer varaibles
extern HAL_Handle halHandle;
extern HAL_Obj    hal;

// Variables for Datalog module
#ifdef DLOG_ENABLE
extern float32_t DBUFF_6CH[DLOG_CH_NUM][DLOG_BUF_SIZE];
extern float32_t dlog6ChData[DLOG_CH_NUM];

// Create an instance of DATALOG Module
extern DLOG_6CH_F dlog_6ch;
#endif  // DLOG_ENABLE

#ifdef DACOUT_EN
typedef struct
{
    float32_t dacOffset;
    float32_t dacGain;
    float32_t *dacPtr;
    uint16_t dacValue;
}DAC_DataView_t;

extern DAC_DataView_t dacDataView[2];

//
// Instrumentation code for timing verifications
// display variable A (in pu) on DAC
//
static inline uint16_t dacConvertData(DAC_DataView_t *pDAC)
{
    uint16_t dacValue;
    float32_t dacData = *pDAC->dacPtr;
    dacValue = (uint16_t)((pDAC->dacOffset + dacData * pDAC->dacGain) * 2048.0);

    pDAC->dacValue = dacValue;      // Just for debug

    return(dacValue);
}
#endif   // DACOUT_EN

// Variables for SFRA module
extern SFRA_F32    sfra1;
extern SFRATest_e  sfraTestLoop;
extern uint32_t    sfraCollectStart;
extern float32_t   sfraNoiseD;
extern float32_t   sfraNoiseQ;
extern float32_t   sfraNoiseW;

//
// struct def for ECAT command packet received in IPC of CPU1
//
typedef struct
{
    CtrlMode_e command;
    int32_t speedRef;
    int32_t positionRef;
}ECAT_CtrlNode_t;

typedef struct
{
    CtrlMode_e state;
    int32_t speed;
    int32_t position;
    int32_t IqRef;
    int32_t IdRef;
    int32_t torque;
}ECAT_StatusNode_t;

//
// IPC Struct
//
typedef struct
{
    uint16_t            exchangeDataFlag;
    uint16_t            exchangeDataNum;
    ECAT_StatusNode_t   statusNode[SYS_NODE_NUM];
}ECAT_IPC_PutDataBuffer;

typedef struct
{
    uint16_t        exchangeDataFlag;
    uint16_t        exchangeDataNum;
    CtrlSync_e      ctrlSynchron;
    ECAT_CtrlNode_t ctrlNode[SYS_NODE_NUM];
}ECAT_IPC_GetDataBuffer;

extern ECAT_IPC_GetDataBuffer ipcCMToCPUDataBuffer;
extern ECAT_IPC_PutDataBuffer ipcCPUToCMDataBuffer;

extern ECAT_IPC_GetDataBuffer dataBufferFromCM;
extern ECAT_IPC_PutDataBuffer dataBufferToCM;

// control command
extern CtrlMode_e ctrlCmdNode[SYS_NODE_NUM];
extern CtrlMode_e ctrlCmdNodePrev[SYS_NODE_NUM];

// speed reference command
extern float32_t speedRefNode[SYS_NODE_NUM];

// position reference command
extern float32_t positionRefNode[SYS_NODE_NUM];

extern uint16_t delayNodeCnt[SYS_NODE_NUM];
extern uint16_t delayNodeSet[SYS_NODE_NUM];

extern CtrlSync_e ctrlSyn;
extern CtrlSync_e ctrlSynPrev;

extern CtrlMode_e ctrlCmdNodeM;
extern float32_t speedRefNodeM;
extern float32_t positionRefNodeM;

extern uint16_t delaySynCnt;
extern uint16_t delaySynSet;

// ****************************************************************************
// function prototypes associated with EtherCAT - Connected Drive
// ****************************************************************************

// set up GPIO for EtherCAT
extern void ECAT_configureESCGPIOs(void);

// configure M-core and IPC for EtherCAT
extern void ECAT_configureAndReleaseCM(void);

// exchange the data between CPU and CM
extern void ECAT_exchangeDataCPUandCM(void);

// run control via EtherCAT
extern void ECAT_runControl(uint16_t nodes);

#define ECAT_FLOAT2PU_SF     10000L
#define ECAT_PU2FLOAT_SF     0.0001F

static inline int32_t ECAT_convertFloatToPU(float32_t valueFloat)
{
    int32_t resultPU = ECAT_FLOAT2PU_SF * valueFloat;

    return(resultPU);
}

static inline float32_t ECAT_convertPUToFloat(int32_t valuePu)
{
    int32_t valueTemp;

    valueTemp = ((valuePu > ECAT_FLOAT2PU_SF) ?
            ECAT_FLOAT2PU_SF : (valuePu < -ECAT_FLOAT2PU_SF) ?
                    -ECAT_FLOAT2PU_SF : valuePu);

    float32_t resultFloat = valueTemp * ECAT_PU2FLOAT_SF;

    return(resultFloat);
}

//
// struct def for DRIVE command packet received in IPC of CPU1 to CPU2
//
typedef struct
{
    CtrlState_e ctrlStateCom;
    float32_t   speedRef;
    float32_t   positionRef;
    float32_t   IdRef;
    float32_t   IqRef;
}DRIVE_IPC_dataToCPU2_t;

typedef struct
{
    CtrlState_e ctrlStateFdb;
    uint16_t    faultFlag;
    float32_t   speed;
    float32_t   posMechTheta;
    float32_t   Id;
    float32_t   Iq;
    float32_t   torque;
}DRIVE_IPC_dataFromCPU2_t;

extern DRIVE_IPC_dataFromCPU2_t ipcCPU2ToCPU1Data;
extern DRIVE_IPC_dataToCPU2_t   ipcCPU1ToCPU2Data;

extern DRIVE_IPC_dataFromCPU2_t dataFromCPU2;
extern DRIVE_IPC_dataToCPU2_t   dataToCPU2;


// exchange the data between CPU1 and CPU2
extern void IPC_exchangeDataCPU1andCPU2(void);

//
// the function prototypes
//

//
// SFRA utility functions
//
extern void injectSFRA(void);
extern void collectSFRA(CTRL_Vars_t *pCtrl);

//! \brief      interrupt subroutine for motor
//! \details    interrupt subroutine for motor
extern __interrupt void motorControlISR(void);


//! \brief      Initializes the parameters of motor
//! \details    Initializes all the parameters for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
//! \return
extern float32_t refPosGen(float32_t out, CTRL_Vars_t *pCtrl);

//! \brief      Initializes the parameters of motor
//! \details    Initializes all the parameters for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
//! \return
extern float32_t ramper(float32_t in, float32_t out, float32_t rampDelta);


//! \brief      Initializes the parameters of system
//! \details    Initializes all the parameters
//! \param[in]  pMotor   A pointer to the system object
extern void initSysParameters(SYS_Vars_t *pSys);

//! \brief      Initializes the parameters of controller
//! \details    Initializes all the parameters for each controller
//! \param[in]  pCtrl   A pointer to the controller object
extern void initCtrlParameters(CTRL_Vars_t *pCtrl);


//! \brief      Reset the control variables of motor
//! \details    Reset the control variables for each motor
//! \param[in]  pMotor   A pointer to the motorVars object
extern void resetControllerVars(CTRL_Vars_t *pCtrl);


//! \brief      Run motor control
//! \details    Set current limitation, check fault
//! \param[in]  pCtrl   A pointer to the motorVars object
extern void runController(SysNode_e node);

//
// Close the Doxygen group.
//! @} //defgroup
//

#endif // end of MULTI_AXIS_LEAD_CTRL_H definition
