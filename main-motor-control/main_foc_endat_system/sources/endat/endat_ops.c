//----------------------------------------------------------------------------------
//	FILE:			endat_ops.c
//
//	Description:	Contains all EnDat21 and EnDat22 position read and additional
//					data operations, including blocking, scheduled/non-blocking,
//					and feedback retrieval functions.
//
//	Version: 		1.0
//
//  Target:  		TMS320F28379D,
//
//----------------------------------------------------------------------------------
//  Copyright Texas Instruments © 2004-2015
//----------------------------------------------------------------------------------
//  Revision History:
//----------------------------------------------------------------------------------
//  Date	  | Description / Status
//----------------------------------------------------------------------------------
// Sep 2017  - Example project for PM EnDat22 Library TIDM-1008
//----------------------------------------------------------------------------------

#include <math.h>
#include "F28x_Project.h"
#include "endat.h"

//=============================================================================
// File-private state
// These variables are internal to the ops layer. Application code accesses
// position feedback exclusively through endat21_getPositionFeedback().
//=============================================================================

static volatile uint32_t  sRawPosition  = 0U;
static volatile float32_t sMechThetaPu  = 0.0F;
static volatile float32_t sElecThetaPu  = 0.0F;
static volatile uint16_t  sDataValid    = 0U;
static volatile uint16_t  sReadPending  = 0U;

//=============================================================================
// File-private inline helpers
//=============================================================================

//
// endatGetType - Returns the library constant matching the configured encoder
//               type (ENDAT22 or ENDAT21).
//
static inline uint16_t endatGetType(void)
{
    return (ENCODER_TYPE == 22) ? ENDAT22 : ENDAT21;
}

//
// endatGetPositionRaw - Extracts and masks the raw position count from the
//                       most recent transaction stored in endat22Data.
//
static inline uint32_t endatGetPositionRaw(void)
{
    uint32_t raw = ((uint32_t)endat22Data.position_hi << 16U) |
                    (uint32_t)endat22Data.position_lo;

    if (endat22Data.position_clocks < 32U)
    {
        const uint32_t mask = (1UL << endat22Data.position_clocks) - 1UL;
        raw &= mask;
    }

    return raw;
}

//
// endatValidatePositionCrc - Calculates the expected CRC for the last position
//                            frame and compares it against the received CRC.
//                            Returns 1 on match, 0 on mismatch.
//
static inline uint16_t endatValidatePositionCrc(void)
{
    uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks,
                                         endatGetType(),
                                         endat22Data.position_lo,
                                         endat22Data.position_hi,
                                         endat22Data.error1,
                                         endat22Data.error2,
                                         endat22CRCtable);
    return CheckCRC(crc5, endat22Data.data_crc);
}

//=============================================================================
// EnDat22 operations
//=============================================================================

void endat22_setupAddlData(void)
{
    // EnDat22: read position + select memory area
    // MRS 0xA1 - Encoder manufacturer parameters; additional data count = 0
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA,
                            0xA1, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);

    // EnDat22: read position + send parameter
    // Address 0xD - selects no. of clock pulses to shift out position value;
    // additional data count = 0
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES_AND_SEND_PARAMETER, 0xD, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES_AND_SEND_PARAMETER, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);

    // Enable Additional Data 1
    // MRS 0x45 - Acknowledge memory content LSB; additional data count = 0
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA,
                            0x45, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);

    // Enable Additional Data 2
    // MRS 0x59 - operating status; additional data count transitions from 1 to 2
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA,
                            0x59, 0, 1);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 1);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(2000L);
}

void endat22_readPositionWithAddlData(void)
{
    // Read position with 2 additional data channels previously enabled
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA, 0, 0, 2);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA, 2);

    // CRC check - position data
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }

    // CRC check - additional data 1
    {
        uint16_t crc5 = PM_endat22_getCrcNorm((endat22Data.additional_data1 >> 16),
                                               endat22Data.additional_data1,
                                               endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.additional_data1_crc)) { ESTOP0; }
    }

    // CRC check - additional data 2
    {
        uint16_t crc5 = PM_endat22_getCrcNorm((endat22Data.additional_data2 >> 16),
                                               endat22Data.additional_data2,
                                               endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.additional_data2_crc)) { ESTOP0; }
    }

    DELAY_US(200L);
}

//=============================================================================
// EnDat21 operations
//=============================================================================

void endat21_readPosition(void)
{
    // Blocking read of position value — no additional data
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    if (!endatValidatePositionCrc())
    {
        gEndatCrcFailCount++;
        return;
    }

    sRawPosition = endatGetPositionRaw();

    if (endat22Data.position_clocks != 0U)
    {
        const float32_t maxCount = (endat22Data.position_clocks >= 32U) ?
                4294967296.0F : (float32_t)(1UL << endat22Data.position_clocks);
        sMechThetaPu = (float32_t)sRawPosition / maxCount;
        sDataValid = 1U;
    }

    DELAY_US(200L);
}

void endat21_schedulePositionRead(void)
{
    if (sReadPending != 0U)
    {
        return;
    }

    endat22Data.dataReady = 0U;
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    sReadPending = 1U;
}

void endat21_servicePositionRead(void)
{
    if ((sReadPending == 0U) || (endat22Data.dataReady != 1U))
    {
        return;
    }

    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    if (!endatValidatePositionCrc())
    {
        gEndatCrcFailCount++;
        sReadPending = 0U;
        return;
    }

    sRawPosition = endatGetPositionRaw();

    if (endat22Data.position_clocks != 0U)
    {
        const float32_t maxCount = (endat22Data.position_clocks >= 32U) ?
                4294967296.0F : (float32_t)(1UL << endat22Data.position_clocks);
        sMechThetaPu = (float32_t)sRawPosition / maxCount;
        sDataValid = 1U;
    }

    sReadPending = 0U;
}

bool endat21_getPositionFeedback(float32_t *mechThetaPu,
                                 float32_t *elecThetaPu,
                                 uint32_t  *rawPosition,
                                 uint16_t   polePairs)
{
    if ((sDataValid == 0U) || (endat22Data.position_clocks == 0U))
    {
        return false;
    }

    *mechThetaPu = sMechThetaPu;
    *rawPosition = sRawPosition;

    sElecThetaPu = sMechThetaPu * (float32_t)polePairs;
    sElecThetaPu = sElecThetaPu - floorf(sElecThetaPu);
    *elecThetaPu = sElecThetaPu;

    return true;
}

//***************************************************************************
// End of file
//***************************************************************************
