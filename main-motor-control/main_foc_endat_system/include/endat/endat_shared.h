#ifndef ENDAT_SHARED_H
#define ENDAT_SHARED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENDAT_POSITION_BUFFER_COUNT  2U

typedef struct
{
    uint32_t rawPosition;
    float    mechThetaPu;
    float    elecThetaPu;
    uint32_t sampleCounter;
    uint16_t valid;
    uint16_t reserved;
} EndatPositionSample;

typedef struct
{
    volatile uint16_t activeIndex;
    volatile uint16_t readPending;
    volatile uint16_t frameReady;
    volatile uint16_t timeoutTicks;
    volatile uint16_t positionClocks;
    volatile uint16_t angleOffsetValid;
    volatile uint32_t publishCount;
    volatile uint32_t crcFailCount;
    volatile uint32_t timeoutCount;
    volatile int32_t angleOffsetCounts;
} EndatRuntimeState;

extern volatile EndatPositionSample gEndatPositionSamples[ENDAT_POSITION_BUFFER_COUNT];
extern volatile EndatRuntimeState gEndatRuntimeState;

#ifdef __cplusplus
}
#endif

#endif // ENDAT_SHARED_H
