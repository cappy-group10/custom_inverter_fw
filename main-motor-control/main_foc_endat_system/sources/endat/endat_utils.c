//----------------------------------------------------------------------------------
//	FILE:			endat_utils.c
//
//	Description:	Utility functions for the EnDat encoder driver. Currently
//					contains CRC comparison; extend this file with any future
//					stateless helper routines.
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
// CheckCRC
// Compares an expected 5-bit CRC value against the one received from the
// encoder. Returns 1 if they match, 0 otherwise.
//
uint16_t CheckCRC(uint16_t expectcrc5, uint16_t receivecrc5)
{
    return (expectcrc5 == receivecrc5) ? 1U : 0U;
}

//***************************************************************************
// End of file
//***************************************************************************