//#############################################################################
//
// FILE:    multi_axis_lead.h
//
// TITLE:   User settings
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
//
//! \file  solutions/common/sensored_foc/include/multi_axis_lead.h
//! \brief header file to be included in all labs
//!
//

#ifndef MULTI_AXIS_LEAD_H
#define MULTI_AXIS_LEAD_H

//
//! \defgroup MASTER_DRIVE
//! @{
//

//
// includes
//
#include "device.h"
#include <math.h>

#include "multi_axis_fsi_shared.h"
#include "multi_axis_lead_hal_cpu1.h"

#define FSI_TX_TIME_WAIT_CNTR       10
#define FSI_PING_TIME_OUT_CNTR      0x200000
#define FSI_TX_TIME_OUT_CNTR        0x200000
#define FSI_RX_TIME_OUT_CNTR        0x040000

#define FSI_TX_TIME_OUT_SET         4000

#define EPWM_SYNC_STOP_VALUE        M_INV_PWM_TICKS
#define EPWM_SYNC_SHIFT_VALUE       50U
#define EPWM_SYNC_EVENT_COUNT       2U

//
// FSI
//
#define GPIO_PIN_FSI_TXCLK  27U                 // GPIO number for FSI TXCLK
#define GPIO_PIN_FSI_TX0    26U                 // GPIO number for FSI TX0
#define GPIO_PIN_FSI_RXCLK  9U                  // GPIO number for FSI RXCLK
#define GPIO_PIN_FSI_RX0    8U                  // GPIO number for FSI RX0

#define GPIO_CFG_FSI_TXCLK  GPIO_27_FSITXA_CLK  // "pinConfig" for FSI TXCLK
#define GPIO_CFG_FSI_TX0    GPIO_26_FSITXA_D0   // "pinConfig" for FSI TX0
#define GPIO_CFG_FSI_RXCLK  GPIO_9_FSIRXA_CLK   // "pinConfig" for FSI RXCLK
#define GPIO_CFG_FSI_RX0    GPIO_8_FSIRXA_D0    // "pinConfig" for FSI RX0

#define FSI_NODE_ACTIVE     FSI_SLAVE_N1

//
// Global variables for FSI
//
extern uint16_t fsiTxStartFlag;
extern uint16_t epwmSyncCmpStop;
extern uint16_t epwmSyncCmpDelay;
extern uint16_t epwmSyncPreScale;

extern uint16_t fsiTxStatus;
extern uint16_t fsiRxStatus;

extern uint16_t fsiTxIndex;
extern uint16_t fsiRxIndex;
extern uint16_t fsiSlaveNodeFirst;
extern uint16_t fsiSlaveNodeLast;
extern uint16_t fsiSlaveNodeActive;
extern uint16_t fsiSlaveNodeRecived;
extern uint16_t fsiSlaveNodeNext;

extern uint16_t *fsiTxDataBufAddr;  // FSI TX data buffer address
extern uint16_t *fsiRxDataBufAddr;  // FSI RX data buffer address
extern uint16_t *fsiTxTagBufAddr;   // FSI TX Tag & user data buffer address
extern uint16_t *fsiRxTagBufAddr;   // FSI RX Tag & user data buffer address

extern uint16_t *fsiTxDataAddr;     // FSI TX frame data address
extern uint16_t *fsiRxDataAddr;     // FSI RX frame data address
extern uint16_t *fsiTxTagAddr;      // FSI TX frame tag and user data address
extern uint16_t *fsiRxTagAddr;      // FSI RX frame tag and user data address

extern uint16_t fsiTxDataBuf[FSI_NODE_NUM][FSI_DATA_NUM];   // TX data array
extern uint16_t fsiRxDataBuf[FSI_NODE_NUM][FSI_DATA_NUM];   // RX data array
extern uint16_t fsiTxTagUdataBuf[FSI_NODE_NUM];
extern uint16_t fsiRxTagUdataBuf[FSI_NODE_NUM];

// Frame tag used with Data/Ping transfers
extern FSI_FrameTag fsiFrameTag[FSI_NODE_NUM];
extern FSI_FrameTag fsiPingTag0;
extern FSI_FrameTag fsiPingTag1;

extern uint16_t fsiTxFrameTag;
extern uint16_t fsiRxFrameTag;

extern uint16_t fsiTxDataWords;
extern uint16_t fsiRxDataWords;
extern uint16_t fsiTRxNodeNum;

extern uint16_t fsiTxDataBufWords;
extern uint16_t fsiRxDataBufWords;

extern FSI_DataWidth fsiTxLanes;
extern FSI_DataWidth fsiRxLanes;

extern volatile uint16_t fsiTxUserData[FSI_NODE_NUM];
extern volatile uint16_t fsiRxUserData[FSI_NODE_NUM];

// User data to be sent with Data frame(for CPU control)
extern volatile uint16_t fsiTxUserDataTag;
extern volatile uint16_t fsiRxUserDataTag;

extern volatile FSI_HandShake_e fsiHandShakeState;

extern volatile FSI_TRxState_e fsiTxInt1Received;
extern volatile FSI_TRxState_e fsiRxInt1Received;


extern volatile uint32_t fsiTxInt2Received;
extern volatile uint32_t fsiRxInt2Received;

extern volatile uint32_t fsiError;

extern uint32_t fsiDelayTapRX0;
extern uint32_t fsiDelayTapRX1;
extern uint32_t fsiDelayTapCLK;

extern uint32_t fsiTxTimeOutCntr;
extern uint32_t fsiRxTimeOutCntr;
extern uint32_t fsiTxTimeOutSet;

extern uint16_t fsiTxTimeWaitCntr;
extern uint16_t fsiTxTimeWaitSet;


extern uint16_t frameDataRX[FSI_TSF_WORDS];
extern uint16_t frameDataTX[FSI_TSF_WORDS];

extern uint16_t fsienableDeltaChk;
extern uint16_t fsienableCrcChk;

extern uint16_t dataCrcCalc;
extern uint16_t dataCrcRX;
extern uint16_t dataCrcTX;

extern const FSI_FrameTag fsiFrameTagCon[8];
//-----------------------------------------------------------------------------
//
// functions for FSI
//

//
// compare16 - Compares two 16 bit values and increments global error flag by 1
//             for mismatch
//
static inline uint16_t FSI_compareData(uint16_t val1, uint16_t val2)
{
    if(val1 != val2)
    {
        return(1);
    }
    else
    {
        return(0);
    }
}

//
// disableAllFSIInterrupts - Disables all event interrupts in both FSI Tx/Rx,
//                           also clear them
//
extern void disableAllFSIInterrupts(void);

//
// fsiTxInt1ISR - FSI Tx Interrupt on INsT1 line
//
extern __interrupt void fsiTxInt1ISR(void);

//
// fsiTxInt2ISR - FSI Tx Interrupt on INT2 line
//
extern __interrupt void fsiTxInt2ISR(void);

//
// fsiRxInt1ISR - FSI Rx Interrupt on INT1 line
//
extern __interrupt void fsiRxInt1ISR(void);

//
// fsiRxInt2ISR - FSI Rx Interrupt on INT2 line
//
extern __interrupt void fsiRxInt2ISR(void);

//
// fsiRxDMAISR - FSI RX DMA ISR
//
extern __interrupt void fsiRxDMAISR(void);

//
// fsiTxDMAISR - FSI TX DMA ISR
//
extern __interrupt void fsiTxDMAISR(void);

//
// FSI initialize
//
extern void FSI_initParams(HAL_Handle handle);

//
// Checks received frame type/tag and updates global error flag
//
extern void FSI_checkReceivedFrameTypeTag(FSI_FrameType type, FSI_FrameTag tag);

//
// FSI handshake
//
extern void FSI_handshakeLead(HAL_Handle handle);

//
// setup Tx/Rx frame data
//
extern void FSI_setupTRxFrameData(HAL_Handle handle);

//
// Starts TX/RX FSI data
//
extern void FSI_runTRxData(HAL_Handle handle);

//
// Updates new data for frame transfer
//
extern void FSI_writeTxFrameData(void);

//! \brief      Reads user defined data and frame tag from received
//! \param[in]  handle is the hardware abstraction layer (HAL) handle
static inline uint16_t FSI_readRxFrameTag(HAL_Handle handle)
{
    HAL_Obj *obj = (HAL_Obj *)handle;
    uint16_t userDataTag = HWREGH(obj->fsiRxHandle + FSI_O_RX_FRAME_TAG_UDATA);
    uint16_t frameTag = (userDataTag>>1) & 0x000F;

    return(frameTag);
}

//! \brief      Reads user defined data and frame tag from received
//! \param[in]  handle is the hardware abstraction layer (HAL) handle
static inline uint16_t FSI_readRxUserData(HAL_Handle handle)
{
    HAL_Obj *obj = (HAL_Obj *)handle;
    uint16_t userDataTag = HWREGH(obj->fsiRxHandle + FSI_O_RX_FRAME_TAG_UDATA);
    uint16_t userData = (userDataTag>>8) & 0x00FF;

    return(userData);
}

//! \brief      Reads data from FSI Rx buffer
//! \param[in]  handle is the hardware abstraction layer (HAL) handle
//! \param[out] array is the address of the array of words to receive the data
//! \param[in]  length is the number of words in the array to be received
static inline void
FSI_readRxDataBuffer(HAL_Handle handle, uint16_t *pRxData, uint16_t length)
{
    HAL_Obj *obj = (HAL_Obj *)handle;
    uint16_t i;

    for(i = 0U; i < length; i++)
    {
        //
        // Read one 16 bit word, increment buffer pointer
        //
        *(pRxData+i) = HWREGH(obj->fsiRxHandle + FSI_O_RX_BUF_BASE(i));
    }

    return;
}

//! \brief     Writes user defined data and frame tag for transmission
//! \param[in] handle     The hardware abstraction layer (HAL) handle
//! \param[in] array is the address of the array of words to be transmitted.
//! \param[in] length is the number of words in the array to be transmitted.
static inline void
FSI_writeTxTagUserData(HAL_Handle handle, uint16_t userDataTag)
{
    HAL_Obj *obj = (HAL_Obj *)handle;
    HWREGH(obj->fsiTxHandle + FSI_O_TX_FRAME_TAG_UDATA) = userDataTag;
    return;
}

//! \brief     Writes data in FSI Tx buffer
//! \param[in] handle is the hardware abstraction layer (HAL) handle
//! \param[in] array is the address of the array of words to be transmitted.
//! \param[in] length is the number of words in the array to be transmitted.
static inline void
FSI_writeTxDataBuffer(HAL_Handle handle, uint16_t *pTxData, uint16_t length)
{
    HAL_Obj *obj = (HAL_Obj *)handle;
    uint16_t i;

    for(i = 0U; i < length; i++)
    {
        // Write one 16 bit word, increment buffer pointer
        //
        HWREGH(obj->fsiTxHandle + FSI_O_TX_BUF_BASE(i)) = *(pTxData+i);
    }

    return;
}

//
// read new data for frame received
//
static inline void FSI_readRxFrameData(HAL_Handle handle)
{
    //
    // Reads user defined data and frame tag from received
    //
    fsiRxFrameTag = FSI_readRxFrameTag(handle);

    if(fsiRxFrameTag == fsiFrameTag[fsiSlaveNodeActive])
    {

        fsiSlaveNodeRecived = fsiSlaveNodeActive;
        fsiTxTimeWaitCntr = 0;
    }
    else
    {
        fsiSlaveNodeRecived = fsiRxFrameTag - FSI_NODE_TAG_OFFSET;
    }

    fsiSlaveNodeActive++;

    if(fsiSlaveNodeActive > fsiSlaveNodeLast)
    {
        fsiSlaveNodeActive = fsiSlaveNodeFirst;
    }

    fsiSlaveNodeNext = fsiSlaveNodeActive + 1;

    if(fsiSlaveNodeNext > fsiSlaveNodeLast)
    {
        fsiSlaveNodeNext = fsiSlaveNodeFirst;
    }

    fsiRxUserData[fsiSlaveNodeRecived] = FSI_readRxUserData(handle);

    fsiTxUserDataTag = ((fsiTxUserData[fsiSlaveNodeActive]<<8) & 0xFF00) +
                       fsiFrameTag[fsiSlaveNodeActive];

    fsiRxDataBufAddr = (uint16_t *)(&fsiRxDataBuf[fsiSlaveNodeRecived][0]);
    fsiTxDataBufAddr = (uint16_t *)(&fsiTxDataBuf[fsiSlaveNodeActive][0]);

    // read RX data buffer
    FSI_readRxDataBuffer(handle, fsiRxDataBufAddr, fsiRxDataWords);
}

// update the received data
//
extern void FSI_updateReceivedData(void);

//
// update the transmission data
//
extern void FSI_updateTransmissionData(void);

//
// Close the Doxygen group.
//! @} //defgroup MASTER_DRIVE
//

#endif  // end of MULTI_AXIS_LEAD_H definition

