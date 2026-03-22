//----------------------------------------------------------------------------------
//	FILE:			endat.c
//
//	Description:	Contains the SPI RX FIFO interrupt service routine for the
//					EnDat encoder interface.
//					All other functionality has been split into:
//					  endat_globals.c  - shared variable definitions
//					  endat_init.c     - hardware initialisation and delay comp
//					  endat_ops.c      - EnDat21/22 position read operations
//					  endat_utils.c    - CRC comparison and other utilities
//					  endat_commands.c - EnDat21/22 command set templates
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

#include "F28x_Project.h"
#include "endat.h"

//
// spiRxFifoIsr - SPI-B RX FIFO interrupt.
// Drains the RX FIFO into endat22Data.rdata[], clears interrupt flags, and
// immediately unpacks any pending position frame so the next PWM ISR sees a
// fresh EnDat sample without waiting for a background poll.
//
__interrupt void spiRxFifoIsr(void)
{
    uint16_t i;
    for (i = 0U; i <= endat22Data.fifo_level; i++)
    {
        endat22Data.rdata[i] = endat22Data.spi->SPIRXBUF;
    }
    endat22Data.spi->SPIFFRX.bit.RXFFOVFCLR = 1;  // Clear overflow flag
    endat22Data.spi->SPIFFRX.bit.RXFFINTCLR = 1;  // Clear interrupt flag
    endat22Data.dataReady = 1U;
    endat21_servicePositionRead();
    PieCtrlRegs.PIEACK.all |= 0x20;               // Issue PIE ACK for group 6
}

//***************************************************************************
// End of file
//***************************************************************************
