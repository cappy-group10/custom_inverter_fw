//----------------------------------------------------------------------------------
//	FILE:			endat_globals.c
//
//	Description:	Defines all shared global variables used across the EnDat
//					driver modules. Public symbols are declared extern in endat.h.
//					Variables only accessed within endat_ops.c are kept static
//					there and are NOT declared here.
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
// CRC lookup table — populated once during EnDat_Init() via
// PM_endat22_generateCRCTable(). Shared between endat_init.c and endat_ops.c.
//
uint16_t endat22CRCtable[SIZEOF_ENDAT_CRCTABLE];

//
// Primary EnDat22 data structure. Shared by all driver modules and the ISR.
//
ENDAT_DATA_STRUCT endat22Data;

//
// Double-buffered position snapshots published by the EnDat producer and
// consumed by both the CPU motor ISR and CLA Task1 at the PWM edge.
//
#pragma DATA_SECTION(gEndatPositionSamples, "ClaData");
volatile EndatPositionSample gEndatPositionSamples[ENDAT_POSITION_BUFFER_COUNT];

//
// Shared EnDat producer state visible to CPU and CLA.
//
#pragma DATA_SECTION(gEndatRuntimeState, "ClaData");
volatile EndatRuntimeState gEndatRuntimeState;

//
// Running count of position-frame CRC failures. Exposed publicly so that
// application code can monitor communication health.
//
volatile uint32_t gEndatCrcFailCount = 0U;
volatile uint32_t gEndatTimeoutCount = 0U;

//***************************************************************************
// End of file
//***************************************************************************
