//#############################################################################
//
// FILE:    multi_axis_fsi_shared.h
//
// TITLE:   User settings
//
// Group:   C2000
//
// Target Family: F2838x/F28004x/f28002x
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
//! \file  solutions/common/sensored_foc/include/multi_axis_fsi_shared.h
//! \brief header file to be included in all labs
//!
//

#ifndef MULTI_AXIS_FSI_SHARED_H
#define MULTI_AXIS_FSI_SHARED_H

//
//! \defgroup MASTER_DRIVE
//! @{
//

//
// includes
//
#include "device.h"
#include <math.h>

#define POWER_ON_DELAY_MAX       36000000L
#define POWER_ON_DELAY_NODE       2000000L
#define POWER_ON_DELAY_MASTER     1000000L
#define POWER_ON_DELAY_DRIVE      2000000L

#define PRESCALER_VAL   	FSI_PRESCALE_50MHZ

// Transfer can be happen over single or double lane
#define FSI_TX_LANES        FSI_DATA_WIDTH_1_LANE
#define FSI_RX_LANES        FSI_DATA_WIDTH_1_LANE

// node number
#define FSI_SLAVE_N1        0
#define FSI_SLAVE_N2        1
#define FSI_SLAVE_N3        2
#define FSI_SLAVE_N4        3
#define FSI_SLAVE_N5        4
#define FSI_SLAVE_N6        5
#define FSI_SLAVE_N7        6
#define FSI_SLAVE_N8        7

//#define FSI_NODES         2               // Slave Nodes = 2
#define FSI_NODES           4               // Slave Nodes = 4
//#define FSI_NODES         8               // Maximum Slave Nodes = 8

#define FSI_NODE_NUM        FSI_NODES       // Node Tag Array Number

#define FSI_NODE_FIRST      FSI_SLAVE_N1
#define FSI_NODE_LAST       FSI_SLAVE_N4
//#define FSI_NODE_LAST       FSI_SLAVE_N2

#define FSI_USERTAG_CHK     0xAE

#define FSI_FRAME_TAG_NODE1     FSI_FRAME_TAG1
#define FSI_FRAME_TAG_NODE2     FSI_FRAME_TAG2
#define FSI_FRAME_TAG_NODE3     FSI_FRAME_TAG3
#define FSI_FRAME_TAG_NODE4     FSI_FRAME_TAG4
#define FSI_FRAME_TAG_NODE5     FSI_FRAME_TAG5
#define FSI_FRAME_TAG_NODE6     FSI_FRAME_TAG6
#define FSI_FRAME_TAG_NODE7     FSI_FRAME_TAG7
#define FSI_FRAME_TAG_NODE8     FSI_FRAME_TAG8

#define FSI_NODE_TAG_FIRST      FSI_FRAME_TAG_NODE1
#define FSI_NODE_TAG_LAST       FSI_FRAME_TAG_NODE4
#define FSI_NODE_TAG_OFFSET     FSI_FRAME_TAG_NODE1

#define FSI_TX_WORDS            5
#define FSI_RX_WORDS            5

#if(FSI_TX_WORDS >= FSI_RX_WORDS)
#define FSI_TSF_WORDS           FSI_TX_WORDS
#define FSI_TRX_WORDS           FSI_TX_WORDS
#else
#define FSI_TSF_WORDS           FSI_RX_WORDS
#define FSI_TRX_WORDS           FSI_RX_WORDS
#endif

#define FSI_DATA_NUM            FSI_TSF_WORDS       // Data Array Number



//
// used values of a FSI frame user data for slave
//
typedef enum
{
    FSI_UDATA_PS_SP_N = 0U,
    FSI_UDATA_PS_SP_F = 1U,
    FSI_UDATA_IS_FDB  = 2U,
    FSI_UDATA_TQ_POW  = 3U,
    FSI_UDATA_DV_MDT  = 4U,
    FSI_UDATA_REQ_TXM = 0x80U
} FSI_SlaveUserData_e;

//
// used values of a FSI frame user data for master
//
typedef enum
{
    FSI_UDATA_IS_REF   = 0U,
    FSI_UDATA_GAIN_ID  = 1U,
    FSI_UDATA_GAIN_IQ  = 2U,
    FSI_UDATA_UMN_ID   = 3U,
    FSI_UDATA_UMN_IQ   = 4U,
    FSI_UDATA_REQ_TXN  = 0x80U
} FSI_MasterUserData_e;

//
// used values of a FSI RX/TX action states
//
typedef enum
{
    FSI_TRx_IDLE  = 0,
    FSI_TRx_SET   = 1,
    FSI_TRx_DONE  = 2
} FSI_TRxState_e;

//
// used values of a FSI RX/TX action states
//
typedef enum
{
    FSI_HANDSHAKE_NO    = 0,
    FSI_HANDSHAKE_DONE  = 1
} FSI_HandShake_e;


#define FSI_FLOAT2PU_SF     10000L
#define FSI_PU2FLOAT_SF     0.0001F

static inline int32_t FSI_convertFloatToPU(float32_t valueFloat)
{
    int32_t resultPU = FSI_FLOAT2PU_SF * valueFloat;

    return(resultPU);
}

static inline float32_t FSI_convertPUToFloat(int32_t valuePu)
{
    int32_t valueTemp;

    valueTemp = ((valuePu > FSI_FLOAT2PU_SF) ?
            FSI_FLOAT2PU_SF : (valuePu < -FSI_FLOAT2PU_SF) ?
                    -FSI_FLOAT2PU_SF : valuePu);

    float32_t resultFloat = valueTemp * FSI_PU2FLOAT_SF;

    return(resultFloat);
}


#define CRC8_POLY   (uint16_t)(0x00FF)

static inline uint16_t getCRC8Value(uint16_t *ptrData, uint16_t length)
{
    uint16_t ci;
    uint16_t crc8 = CRC8_POLY;
    uint16_t crcT;

    for(ci = 0; ci < length; ci++)
    {
        crcT = (crc8 >> 4) ^ ((*ptrData) & 0x00FF);
        crcT ^= (crcT>> 2);
        crc8 = ((crc8<<4) ^ (crcT<<6) ^ (crcT<<3) ^ crcT) & 0x00FF;

        crcT = (crc8 >> 4) ^ (((*ptrData++)>>8) & 0x00FF);
        crcT ^= (crcT>> 2);
        crc8 = ((crc8<<4) ^ (crcT<<6) ^ (crcT<<3) ^ crcT) & 0x00FF;
    }

    return(crc8);
}


static inline uint16_t getChkSumValue(uint16_t *ptrData, uint16_t length)
{
    uint16_t ci;
    uint16_t checkSum = 0;

    for(ci = 0; ci < length; ci++)
    {
        checkSum += (*ptrData++);
    }

    checkSum = (checkSum + (checkSum>>8)) & 0x00FF;

    return(checkSum);
}

//
// Close the Doxygen group.
//! @} //defgroup MASTER_DRIVE
//

#endif  // end of MULTI_AXIS_FSI_SHARED_H definition

