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

//----------------------------------------------------------------------------------
//	FILE:			dlog_2ch_f.h
//
//	Description:header file for data logging module
//
//	Version: 		1.0
//
//----------------------------------------------------------------------------------

#ifndef DLOG_2CH_F_H
#define DLOG_2CH_F_H

#include "device.h"

//*********** Structure Definition ********//
typedef struct{
	float32_t *input_ptr1;
	float32_t *input_ptr2;
	float32_t *output_ptr1;
	float32_t *output_ptr2;
	float32_t prev_value;
	float32_t trig_value;
	int16_t status;
	int16_t pre_scalar;
	int16_t skip_count;
	int16_t size;
	int16_t count;
}DLOG_2CH_F;

//*********** Function Declarations *******//
void DLOG_2CH_F_init(DLOG_2CH_F *v);
void DLOG_2CH_F_FUNC(DLOG_2CH_F *v);

#endif  // DLOG_2CH_F_H

// end of the file
