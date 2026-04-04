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
File name:        RMP_CNTL.H  
===================================================================================*/


#ifndef __RMP_CNTL_H__
#define __RMP_CNTL_H__

#include <math.h>

typedef struct {
    float32_t TargetValue; 	  // Input: Target input (pu)
    uint32_t  RampDelayMax;	  // Parameter: Maximum delay rate (Q0) - independently with global Q
    float32_t RampLowLimit;	  // Parameter: Minimum limit (pu)
    float32_t RampHighLimit;  // Parameter: Maximum limit (pu)
	uint32_t  RampDelayCount; // Variable: Incremental delay (Q0) - independently with global Q
	float32_t SetpointValue;  // Output: Target output (pu)
	uint32_t  EqualFlag;	  // Output: Flag output (Q0) - independently with global Q
	float32_t Tmp;			  // Variable: Temp variable
	float32_t RampStepSize;   // Parameter: Ramp increment per step (pu)
} RMPCNTL;


/*-----------------------------------------------------------------------------
Default initalizer for the RMPCNTL object.
-----------------------------------------------------------------------------*/                     
#define RMPCNTL_DEFAULTS {  \
    0, /* TargetValue */ \
    1, /* RampDelayMax */ \
   -1, /* RampLowLimit */ \
    1, /* RampHighLimit */ \
    0, /* RampDelayCount */ \
    0, /* SetpointValue */ \
    0, /* EqualFlag */ \
    0, /* Tmp */ \
    0.00001525f  /* RampStepSize */ \
    }

/*------------------------------------------------------------------------------
 	RAMP Controller Macro Definition
------------------------------------------------------------------------------*/
static inline void fclRampControl(RMPCNTL * rc1)
{
	rc1->Tmp = (rc1->TargetValue) - (rc1->SetpointValue);

#if defined(__TMS320C28XX_TMU__)
    if(fabsf(rc1->Tmp) >= rc1->RampStepSize)
#elif defined(__TMS320C28XX_FPU32__)
    if(fabs(rc1->Tmp) >= rc1->RampStepSize)
#else
    if((rc1->Tmp >= rc1->RampStepSize) && (rc1->Tmp <= -(rc1->RampStepSize)))
#endif
	{
		rc1->RampDelayCount++;

		if((rc1->RampDelayCount) >= (rc1->RampDelayMax))
		{
			if(rc1->TargetValue >= rc1->SetpointValue)
			{
				rc1->SetpointValue += rc1->RampStepSize;
			}
			else
			{
				rc1->SetpointValue -= rc1->RampStepSize;
			}

			rc1->RampDelayCount = 0;
		}
	}
	else
	{
	    rc1->SetpointValue = rc1->TargetValue;
		rc1->EqualFlag = 0x7FFFFFFF;
	}

#if defined(__TMS320C28XX_TMU1__)
    rc1->SetpointValue = __fmax((__fmin(rc1->SetpointValue, rc1->RampHighLimit)),
                                rc1->RampLowLimit);
#elif defined(__TMS320C28XX_FPU32__)
    rc1->SetpointValue = fmaxf((fminf(rc1->SetpointValue,rc1->RampHighLimit)),
                              rc1->RampLowLimit);
#else
    rc1->SetpointValue  = (rc1->SetpointValue  > rc1->RampHighLimit) ? rc1->RampHighLimit : rc1->SetpointValue;
    rc1->SetpointValue  = (rc1->SetpointValue  < rc1->RampLowLimit) ? rc1->RampLowLimit : rc1->SetpointValue;

#endif

    return;
}


#define RC_MACRO(v)																	\
	v.Tmp = v.TargetValue - v.SetpointValue;										\
/*  0.0000305 is resolution of Q15 */												\
if (_IQabs(v.Tmp) >= _IQ(0.0000305))				    							\
{																					\
	v.RampDelayCount++	;															\
		if (v.RampDelayCount >= v.RampDelayMax)										\
		{																			\
			if (v.TargetValue >= v.SetpointValue)									\
				v.SetpointValue += _IQ(0.0000305);									\
			else																	\
				v.SetpointValue -= _IQ(0.0000305);									\
																					\
			v.SetpointValue=_IQsat(v.SetpointValue,v.RampHighLimit,v.RampLowLimit);	\
			v.RampDelayCount = 0;													\
																					\
		}																			\
}																					\
else v.EqualFlag = 0x7FFFFFFF;

#endif // __RMP_CNTL_H__
