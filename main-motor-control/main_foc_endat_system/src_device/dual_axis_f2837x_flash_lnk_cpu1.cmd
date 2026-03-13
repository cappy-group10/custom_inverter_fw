//#############################################################################
//
// FILE:    dual_axis_f2837x_flash_lnk_cpu1.cmd.cmd
//
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
// In addition to this memory linker command file,
// add the header linker command file directly to the project.
// The header linker command file is required to link the
// peripheral structures to the proper locations within
// the memory map.
//
// For BIOS applications add:      F28004x_Headers_BIOS.cmd
// For nonBIOS applications add:   F28004x_Headers_nonBIOS.cmd
//
// The user must define CLA_C in the project linker settings if using the
// CLA C compiler
// Project Properties -> C2000 Linker -> Advanced Options -> Command File
// Preprocessing -> --define
#ifdef CLA_C
// Define a size for the CLA scratchpad area that will be used
// by the CLA compiler for local symbols and temps
// Also force references to the special symbols that mark the
// scratchpad are.
CLA_SCRATCHPAD_SIZE = 0x100;
--undef_sym=__cla_scratchpad_end
--undef_sym=__cla_scratchpad_start
#endif //CLA_C


MEMORY
{
    /* BEGIN is used for the "boot to Flash" bootloader mode   */
   BEGIN            : origin = 0x080000,   length = 0x000002
   BOOT_RSVD        : origin = 0x000002,   length = 0x000121     /* Part of M0, BOOT rom will use this for stack */
   RAMM0           	: origin = 0x000123,   length = 0x0002DD
   RAMM1            : origin = 0x000400,   length = 0x0003F8     /* on-chip RAM block M1 */
/* RAMM1_RSVD       : origin = 0x0007F8,   length = 0x000008 */  /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */

   RAMD0           	: origin = 0x00B000,   length = 0x000800
   RAMD1            : origin = 0x00B800,   length = 0x000800

   RAMLS0          	: origin = 0x008000,   length = 0x000800
   RAMLS1          	: origin = 0x008800,   length = 0x000800
   RAMLS2      		: origin = 0x009000,   length = 0x000800
   RAMLS3      		: origin = 0x009800,   length = 0x000800
   RAMLS4      	    : origin = 0x00A000,   length = 0x000800
   RAMLS5           : origin = 0x00A800,   length = 0x000800

   RAMGS0           : origin = 0x00C000,   length = 0x001000
   RAMGS1           : origin = 0x00D000,   length = 0x001000
   RAMGS2           : origin = 0x00E000,   length = 0x001000
   RAMGS3           : origin = 0x00F000,   length = 0x001000
   RAMGS4           : origin = 0x010000,   length = 0x001000
   RAMGS5           : origin = 0x011000,   length = 0x001000
   RAMGS6           : origin = 0x012000,   length = 0x001000
   RAMGS7           : origin = 0x013000,   length = 0x001000
   RAMGS8           : origin = 0x014000,   length = 0x001000
   RAMGS9           : origin = 0x015000,   length = 0x001000
   RAMGS10          : origin = 0x016000,   length = 0x001000

/* RAMGS11          : origin = 0x017000, length = 0x000FF8 */  /* Uncomment for F28374D, F28376D devices */
/* RAMGS11_RSVD     : origin = 0x017FF8, length = 0x000008 */  /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */

   RAMGS11          : origin = 0x017000, length = 0x001000     /* Only Available on F28379D, F28377D, F28375D devices. Remove line on other devices. */
   RAMGS12          : origin = 0x018000, length = 0x001000     /* Only Available on F28379D, F28377D, F28375D devices. Remove line on other devices. */
   RAMGS13          : origin = 0x019000, length = 0x001000     /* Only Available on F28379D, F28377D, F28375D devices. Remove line on other devices. */
   RAMGS14          : origin = 0x01A000, length = 0x001000     /* Only Available on F28379D, F28377D, F28375D devices. Remove line on other devices. */
   RAMGS15          : origin = 0x01B000, length = 0x000FF8     /* Only Available on F28379D, F28377D, F28375D devices. Remove line on other devices. */
/* RAMGS15_RSVD     : origin = 0x01BFF8, length = 0x000008 */  /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */
                                                               /* Only on F28379D, F28377D, F28375D devices. Remove line on other devices. */

   /* Flash sectors */
   FLASHA           : origin = 0x080002,   length = 0x001FFE	/* on-chip Flash */
   FLASHB           : origin = 0x082000,   length = 0x002000	/* on-chip Flash */
   FLASHC           : origin = 0x084000,   length = 0x002000	/* on-chip Flash */
   FLASHD           : origin = 0x086000,   length = 0x002000	/* on-chip Flash */
   FLASHE           : origin = 0x088000,   length = 0x008000	/* on-chip Flash */
   FLASHF           : origin = 0x090000,   length = 0x008000	/* on-chip Flash */
   FLASHG           : origin = 0x098000,   length = 0x008000	/* on-chip Flash */
   FLASHH           : origin = 0x0A0000,   length = 0x008000	/* on-chip Flash */
   FLASHI           : origin = 0x0A8000,   length = 0x008000	/* on-chip Flash */
   FLASHJ           : origin = 0x0B0000,   length = 0x008000	/* on-chip Flash */
   FLASHK           : origin = 0x0B8000,   length = 0x002000	/* on-chip Flash */
   FLASHL           : origin = 0x0BA000,   length = 0x002000	/* on-chip Flash */
   FLASHM           : origin = 0x0BC000,   length = 0x002000	/* on-chip Flash */
   FLASHN           : origin = 0x0BE000,   length = 0x001FF0	/* on-chip Flash */
/* FLASHN_RSVD      : origin = 0x0BFFF0,   length = 0x000010 */ /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */


   RESET           	: origin = 0x3FFFC0, length = 0x000002

   CPU2TOCPU1RAM    : origin = 0x03F800, length = 0x000400
   CPU1TOCPU2RAM    : origin = 0x03FC00, length = 0x000400

   CANA_MSG_RAM     : origin = 0x049000,   length = 0x000800
   CANB_MSG_RAM     : origin = 0x04B000,   length = 0x000800

   CLA1_MSGRAMLOW   : origin = 0x001480,   length = 0x000080
   CLA1_MSGRAMHIGH  : origin = 0x001500,   length = 0x000080
}

SECTIONS
{
   codestart           : > BEGIN, ALIGN(4)
   .text               : >> FLASHA | FLASHB | FLASHC | FLASHD, ALIGN(4)
   .cinit              : > FLASHG, ALIGN(4)
   .switch             : > FLASHG, ALIGN(4)
   .reset              : > RESET, TYPE = DSECT /* not used, */
   .stack              : > RAMM1

#if defined(__TI_EABI__)
   .init_array      : > FLASHG, ALIGN(8)
   .bss             : > RAMLS0 | RAMLS1, ALIGN(4)
   .bss:output      : > RAMLS0 | RAMLS1
   .bss:cio         : > RAMLS0 | RAMLS1
   .data            : > RAMLS0 | RAMLS1
   .sysmem          : > RAMLS0 | RAMLS1
   /* Initalized sections go in Flash */
   .const           : > FLASHG, ALIGN(8)
#else
   .pinit           : > FLASHG, ALIGN(8)
   .ebss            : > RAMLS0 | RAMLS1
   .esysmem         : > RAMLS0 | RAMLS1
   .cio             : > RAMLS0 | RAMLS1
   /* Initalized sections go in Flash */
   .econst          : > FLASHG, ALIGN(8)
#endif

   MSGRAM_CPU1_TO_CPU2 : > CPU1TOCPU2RAM, type=NOINIT
   MSGRAM_CPU2_TO_CPU1 : > CPU2TOCPU1RAM, type=NOINIT

    /* CLA specific sections */
#if defined(__TI_EABI__)
   Cla1Prog         :   LOAD = FLASHF,
                        RUN = RAMLS4 | RAMLS5,
                        LOAD_START(Cla1funcsLoadStart),
                        LOAD_END(Cla1funcsLoadEnd),
                        RUN_START(Cla1funcsRunStart),
                        LOAD_SIZE(Cla1funcsLoadSize),
                        ALIGN(8)
#else
   Cla1Prog         :   LOAD = FLASHF,
                        RUN = RAMLS4 | RAMLS5,
                        LOAD_START(_Cla1funcsLoadStart),
                        LOAD_END(_Cla1funcsLoadEnd),
                        RUN_START(_Cla1funcsRunStart),
                        LOAD_SIZE(_Cla1funcsLoadSize),
                        ALIGN(8)
#endif

   ClaData			: > RAMLS3

   Cla1ToCpuMsgRAM  : > CLA1_MSGRAMLOW, type=NOINIT
   CpuToCla1MsgRAM  : > CLA1_MSGRAMHIGH, type=NOINIT

   /* SFRA specific sections */
   SFRA_F32_Data	: > RAMGS5, ALIGN = 64
   sfra_data    	: > RAMGS5

#ifdef CLA_C
   /* CLA C compiler sections */
   //
   // Must be allocated to memory the CLA has write access to
   //
   CLAscratch       :
                     { *.obj(CLAscratch)
                     . += CLA_SCRATCHPAD_SIZE;
                     *.obj(CLAscratch_end) } >  RAMLS2

   .scratchpad      : > RAMLS2
   .bss_cla         : > RAMLS2

#if defined(__TI_EABI__)
   .const_cla       :   LOAD = FLASHF,
                        RUN = RAMLS4,
                        RUN_START(Cla1ConstRunStart),
                        LOAD_START(Cla1ConstLoadStart),
                        LOAD_SIZE(Cla1ConstLoadSize),
                        ALIGN(8)
#else
   .const_cla       :   LOAD = FLASHF,
                        RUN = RAMLS4,
                        RUN_START(Cla1ConstRunStart),
                        LOAD_START(Cla1ConstLoadStart),
                        LOAD_SIZE(Cla1ConstLoadSize),
                        ALIGN(8)
#endif
#endif //CLA_C

#if defined(__TI_EABI__)
   .TI.ramfunc : {} LOAD = FLASHE,
                    RUN = RAMGS0 | RAMGS1 | RAMGS2 | RAMGS3,
                    LOAD_START(RamfuncsLoadStart),
                    LOAD_SIZE(RamfuncsLoadSize),
                    LOAD_END(RamfuncsLoadEnd),
                    RUN_START(RamfuncsRunStart),
                    RUN_SIZE(RamfuncsRunSize),
                    RUN_END(RamfuncsRunEnd),
                    ALIGN(4)
#else
   .TI.ramfunc : {} LOAD = FLASHE,
                    RUN = RAMGS0 | RAMGS1 | RAMGS2 | RAMGS3,
                    LOAD_START(_RamfuncsLoadStart),
                    LOAD_SIZE(_RamfuncsLoadSize),
                    LOAD_END(_RamfuncsLoadEnd),
                    RUN_START(_RamfuncsRunStart),
                    RUN_SIZE(_RamfuncsRunSize),
                    RUN_END(_RamfuncsRunEnd),
                    ALIGN(4)
#endif

}

//===========================================================================
// End of file.
//===========================================================================
