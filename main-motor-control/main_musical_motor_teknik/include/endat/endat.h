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

// Public API
extern void     EnDat_Init(void);
extern void     EnDat_initDelayComp(void);
extern uint16_t CheckCRC(uint16_t expectcrc5, uint16_t receivecrc5);

extern void     endat21_readPosition(void);
extern void     endat22_readPositionWithAddlData(void);
extern void     endat22_setupAddlData(void);

// ISR — declared __interrupt for driverlib PIE vector table registration
// (registered via Interrupt_register(INT_SPIB_RX, &spiRxFifoIsr) in EnDat_Init)
extern __interrupt void spiRxFifoIsr(void);

#endif // ENDAT_H
