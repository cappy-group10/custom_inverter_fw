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
// The producer publishes decoded position snapshots into the shared
// gEndatPositionSamples[] double buffer. Application code accesses the latest
// stable sample through endat21_getPublishedPosition().
//=============================================================================

static volatile uint16_t sPolePairs = 0U;

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

//
// endatComputeElecThetaPu - Converts the mechanical angle to electrical angle
// using the pole-pair value configured during producer initialization.
//
static inline float32_t endatComputeElecThetaPu(float32_t mechThetaPu)
{
    float32_t elecThetaPu = mechThetaPu * (float32_t)sPolePairs;
    elecThetaPu = elecThetaPu - floorf(elecThetaPu);
    return elecThetaPu;
}

//
// endatClearPublishedPosition - Clears the shared EnDat position buffers.
//
static inline void endatClearPublishedPosition(void)
{
    uint16_t i;

    for(i = 0U; i < ENDAT_POSITION_BUFFER_COUNT; i++)
    {
        gEndatPositionSamples[i].rawPosition = 0U;
        gEndatPositionSamples[i].mechThetaPu = 0.0F;
        gEndatPositionSamples[i].elecThetaPu = 0.0F;
        gEndatPositionSamples[i].sampleCounter = 0U;
        gEndatPositionSamples[i].valid = 0U;
        gEndatPositionSamples[i].reserved = 0U;
    }
}

//
// endatResetProducerState - Resets the shared producer state and counters.
//
static inline void endatResetProducerState(void)
{
    gEndatRuntimeState.activeIndex = 0U;
    gEndatRuntimeState.readPending = 0U;
    gEndatRuntimeState.frameReady = 0U;
    gEndatRuntimeState.timeoutTicks = 0U;
    gEndatRuntimeState.publishCount = 0U;
    gEndatRuntimeState.crcFailCount = 0U;
    gEndatRuntimeState.timeoutCount = 0U;
    endat22Data.dataReady = 0U;
}

//
// endatDecodePositionSample - Validates and decodes the most recent position
// frame already unpacked into endat22Data.
//
static inline bool endatDecodePositionSample(EndatPositionSample *sample)
{
    uint32_t rawPosition;
    float32_t maxCount;

    if(!endatValidatePositionCrc())
    {
        gEndatCrcFailCount++;
        gEndatRuntimeState.crcFailCount = gEndatCrcFailCount;
        return false;
    }

    if(endat22Data.position_clocks == 0U)
    {
        return false;
    }

    rawPosition = endatGetPositionRaw();
    maxCount = (endat22Data.position_clocks >= 32U) ?
            4294967296.0F : (float32_t)(1UL << endat22Data.position_clocks);

    sample->rawPosition = rawPosition;
    sample->mechThetaPu = (float32_t)rawPosition / maxCount;
    sample->elecThetaPu = endatComputeElecThetaPu(sample->mechThetaPu);
    sample->sampleCounter = gEndatRuntimeState.publishCount + 1U;
    sample->valid = 1U;
    sample->reserved = 0U;

    return true;
}

//
// endatPublishPositionSample - Publishes a fully decoded sample into the shared
// double buffer and flips the active index as the final step.
//
static inline void endatPublishPositionSample(const EndatPositionSample *sample)
{
    uint16_t publishIndex = gEndatRuntimeState.activeIndex ^ 1U;

    gEndatPositionSamples[publishIndex].valid = 0U;
    gEndatPositionSamples[publishIndex].rawPosition = sample->rawPosition;
    gEndatPositionSamples[publishIndex].mechThetaPu = sample->mechThetaPu;
    gEndatPositionSamples[publishIndex].elecThetaPu = sample->elecThetaPu;
    gEndatPositionSamples[publishIndex].sampleCounter = sample->sampleCounter;
    gEndatPositionSamples[publishIndex].reserved = 0U;
    gEndatPositionSamples[publishIndex].valid = 1U;

    gEndatRuntimeState.publishCount = sample->sampleCounter;
    gEndatRuntimeState.activeIndex = publishIndex;
}

//
// endatScheduleReadInternal - Arms a single non-blocking position read.
//
static inline void endatScheduleReadInternal(void)
{
    if(gEndatRuntimeState.readPending != 0U)
    {
        return;
    }

    endat22Data.dataReady = 0U;
    gEndatRuntimeState.frameReady = 0U;
    gEndatRuntimeState.timeoutTicks = 0U;
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    gEndatRuntimeState.readPending = 1U;
}

//
// endatServiceReadInternal - Services a completed non-blocking frame, decodes
// it, and publishes it into the shared snapshot buffer.
//
static inline bool endatServiceReadInternal(void)
{
    EndatPositionSample sample = {0};

    if((gEndatRuntimeState.readPending == 0U) ||
       (gEndatRuntimeState.frameReady == 0U))
    {
        return false;
    }

    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    gEndatRuntimeState.readPending = 0U;
    gEndatRuntimeState.frameReady = 0U;
    gEndatRuntimeState.timeoutTicks = 0U;
    endat22Data.dataReady = 0U;

    if(!endatDecodePositionSample(&sample))
    {
        return false;
    }

    endatPublishPositionSample(&sample);

    return true;
}

//
// endatHandleReadTimeout - Drops a stalled read after a bounded number of
// producer ticks so the producer can recover automatically.
//
static inline void endatHandleReadTimeout(void)
{
    if((gEndatRuntimeState.readPending == 0U) ||
       (gEndatRuntimeState.frameReady != 0U))
    {
        return;
    }

    gEndatRuntimeState.timeoutTicks++;

    if(gEndatRuntimeState.timeoutTicks < ENDAT_PRODUCER_TIMEOUT_TICKS)
    {
        return;
    }

    gEndatRuntimeState.readPending = 0U;
    gEndatRuntimeState.frameReady = 0U;
    gEndatRuntimeState.timeoutTicks = 0U;
    endat22Data.dataReady = 0U;

    gEndatTimeoutCount++;
    gEndatRuntimeState.timeoutCount = gEndatTimeoutCount;
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
    EndatPositionSample sample = {0};

    // Blocking read of position value — no additional data
    endat22Data.dataReady = 0U;
    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    endat22Data.dataReady = 0U;

    if(endatDecodePositionSample(&sample))
    {
        endatPublishPositionSample(&sample);
    }

    DELAY_US(200L);
}

void endat21_initProducer(uint16_t polePairs)
{
    sPolePairs = polePairs;
    gEndatCrcFailCount = 0U;
    gEndatTimeoutCount = 0U;

    endatClearPublishedPosition();
    endatResetProducerState();
}

void endat21_startProducer(void)
{
    gEndatRuntimeState.readPending = 0U;
    gEndatRuntimeState.frameReady = 0U;
    gEndatRuntimeState.timeoutTicks = 0U;
    gEndatRuntimeState.crcFailCount = gEndatCrcFailCount;
    gEndatRuntimeState.timeoutCount = gEndatTimeoutCount;
    endat22Data.dataReady = 0U;
}

void endat21_runProducerTick(void)
{
    if(gEndatRuntimeState.readPending != 0U)
    {
        if(gEndatRuntimeState.frameReady != 0U)
        {
            (void)endatServiceReadInternal();
        }
        else
        {
            endatHandleReadTimeout();
        }
    }

    if(gEndatRuntimeState.readPending == 0U)
    {
        endatScheduleReadInternal();
    }
}

void endat21_schedulePositionRead(void)
{
    endatScheduleReadInternal();
}

void endat21_servicePositionRead(void)
{
    (void)endatServiceReadInternal();
}

bool endat21_getPublishedPosition(EndatPositionSample *sample)
{
    uint16_t activeIndex;
    uint16_t verifyIndex;

    if(sample == (EndatPositionSample *)0)
    {
        return false;
    }

    do
    {
        activeIndex = gEndatRuntimeState.activeIndex;
        sample->rawPosition = gEndatPositionSamples[activeIndex].rawPosition;
        sample->mechThetaPu = gEndatPositionSamples[activeIndex].mechThetaPu;
        sample->elecThetaPu = gEndatPositionSamples[activeIndex].elecThetaPu;
        sample->sampleCounter = gEndatPositionSamples[activeIndex].sampleCounter;
        sample->valid = gEndatPositionSamples[activeIndex].valid;
        sample->reserved = gEndatPositionSamples[activeIndex].reserved;
        verifyIndex = gEndatRuntimeState.activeIndex;
    } while(activeIndex != verifyIndex);

    return (sample->valid != 0U);
}

bool endat21_getPositionFeedback(float32_t *mechThetaPu,
                                 float32_t *elecThetaPu,
                                 uint32_t  *rawPosition,
                                 uint16_t   polePairs)
{
    EndatPositionSample sample = {0};

    (void)polePairs;

    if(!endat21_getPublishedPosition(&sample))
    {
        return false;
    }

    *mechThetaPu = sample.mechThetaPu;
    *elecThetaPu = sample.elecThetaPu;
    *rawPosition = sample.rawPosition;

    return true;
}

//***************************************************************************
// End of file
//***************************************************************************
