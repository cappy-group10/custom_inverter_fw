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

/* =================================================================================
File name:       SVGEN.H  
===================================================================================*/


#ifndef __SVGEN_H__
#define __SVGEN_H__

typedef struct 	{
    float32_t  Ualpha; 			// Input: reference alpha-axis phase voltage
    float32_t  Ubeta;			// Input: reference beta-axis phase voltage
    float32_t  Ta;				// Output: reference phase-a switching function
    float32_t  Tb;				// Output: reference phase-b switching function
    float32_t  Tc;				// Output: reference phase-c switching function
    float32_t  tmp1;			// Variable: temp variable
    float32_t  tmp2;			// Variable: temp variable
    float32_t  tmp3;			// Variable: temp variable
	uint16_t VecSector;	        // Space vector sector
	} SVGEN;
																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																				

/*-----------------------------------------------------------------------------
Default initalizer for the SVGEN object.
-----------------------------------------------------------------------------*/                     
#define SVGEN_DEFAULTS { 0,0,0,0,0 }                       

/*------------------------------------------------------------------------------
	Space Vector  Generator (SVGEN) Macro Definition
------------------------------------------------------------------------------*/

static inline void runSVGenDQ(SVGEN * in)
{
	in->tmp1 = in->Ubeta;
	in->tmp2 = (in->Ubeta / 2) + (0.866 * in->Ualpha);
	in->tmp3 = in->tmp2 - in->tmp1;

	in->VecSector = 3;
	in->VecSector = (in->tmp2 > 0)?(in->VecSector - 1):in->VecSector;
	in->VecSector = (in->tmp3 > 0)?(in->VecSector - 1):in->VecSector;
	in->VecSector = (in->tmp1 < 0)?(7 - in->VecSector):in->VecSector;

	if(in->VecSector == 1 || in->VecSector == 4)
	{
		in->Ta = in->tmp2;
		in->Tb = in->tmp1 - in->tmp3;
		in->Tc = -in->tmp2;
	}
	else if(in->VecSector == 2 || in->VecSector == 5)
	{
		in->Ta = in->tmp3 + in->tmp2;
		in->Tb = in->tmp1;
		in->Tc = -in->tmp1;
	}
	else
	{
		in->Ta = in->tmp3;
		in->Tb = -in->tmp3;
		in->Tc = -(in->tmp1 + in->tmp2);
	}
}																	\

#endif // __SVGEN_H__
