//----------------------------------------------------------------------------------
//  FILE:           endat.h
//
//  Description:    EnDat encoder interface header - CONVERTED TO DRIVERLIB
//                  Original used F28x HAL/bitfield style.
//                  Changes:
//                    - "interrupt" keyword -> "__interrupt" (driverlib/CCS style)
//                    - Endat_setup_GPIO, Endat_config_XBAR, EPWM4_Config removed
//                      from extern list (now static in endat.c, not public API)
//                    - error() removed (replaced with inline asm(" ESTOP0"))
//
//  Target:         TMS320F28379D
//----------------------------------------------------------------------------------

#ifndef ENDAT_H
#define ENDAT_H

#include <stdbool.h>

#include "endat_shared.h"
#include "PM_endat22_Include.h"

// Encoder type: 22 = EnDat 2.2, 21 = EnDat 2.1
// Tests may fail if this is set incorrectly for your encoder
#define ENCODER_TYPE                21

// Frequency dividers:
//   EnDat CLK = SYSCLK /  (4 * DIVIDER)
//   INIT (~200 kHz at 200 MHz SYSCLK): 200e6 / (4*250) = 200 kHz
//   RUNTIME (~8 MHz at 200 MHz SYSCLK): 200e6 / (4*6) = 8.33 MHz
//   Only even values >= 6 are supported.
#define ENDAT_RUNTIME_FREQ_DIVIDER  6
#define ENDAT_INIT_FREQ_DIVIDER     250
#define ENDAT_PRODUCER_TIMEOUT_TICKS 4U

// Public API
extern void     EnDat_Init(void);
extern void     EnDat_initDelayComp(void);
extern uint16_t CheckCRC(uint16_t expectcrc5, uint16_t receivecrc5);

extern void     endat21_readPosition(void);
extern void     endat21_runCommandSet(void);
extern void     endat21_initProducer(uint16_t polePairs);
extern void     endat21_startProducer(void);
extern void     endat21_runProducerTick(void);
extern void     endat21_schedulePositionRead(void);
extern void     endat21_servicePositionRead(void);
extern bool     endat21_getPublishedPosition(EndatPositionSample *sample);
extern bool     endat21_getPositionFeedback(float32_t *mechThetaPu,
                                            float32_t *elecThetaPu,
                                            uint32_t *rawPosition,
                                            uint16_t polePairs);
extern void     endat22_readPositionWithAddlData(void);
extern void     endat22_setupAddlData(void);

extern volatile uint32_t gEndatCrcFailCount;
extern volatile uint32_t gEndatTimeoutCount;

// ISR — declared __interrupt for PIE vector table registration in EnDat_Init
extern __interrupt void spiRxFifoIsr(void);
extern __interrupt void endatProducerISR(void);

#endif // ENDAT_H
