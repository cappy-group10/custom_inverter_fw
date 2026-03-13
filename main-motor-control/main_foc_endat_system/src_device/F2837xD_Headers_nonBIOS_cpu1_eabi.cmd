
MEMORY
{
   ADCA_RESULT   : origin = 0x000B00, length = 0x000020
   ADCB_RESULT   : origin = 0x000B20, length = 0x000020
   ADCC_RESULT   : origin = 0x000B40, length = 0x000020
   ADCD_RESULT   : origin = 0x000B60, length = 0x000020

   ADCA          : origin = 0x007400, length = 0x000080
   ADCB          : origin = 0x007480, length = 0x000080
   ADCC          : origin = 0x007500, length = 0x000080
   ADCD          : origin = 0x007580, length = 0x000080

   ANALOG_SUBSYS : origin = 0x05D180, length = 0x000080

   CANA          : origin = 0x048000, length = 0x000800
   CANB          : origin = 0x04A000, length = 0x000800

   CLA1          : origin = 0x001400, length = 0x000040     /* CLA registers */

   CLB_XBAR      : origin = 0x007A40, length = 0x000040

   CMPSS1        : origin = 0x005C80, length = 0x000020
   CMPSS2        : origin = 0x005CA0, length = 0x000020
   CMPSS3        : origin = 0x005CC0, length = 0x000020
   CMPSS4        : origin = 0x005CE0, length = 0x000020
   CMPSS5        : origin = 0x005D00, length = 0x000020
   CMPSS6        : origin = 0x005D20, length = 0x000020
   CMPSS7        : origin = 0x005D40, length = 0x000020
   CMPSS8        : origin = 0x005D60, length = 0x000020

   CPU_TIMER0    : origin = 0x000C00, length = 0x000008     /* CPU Timer0 registers */
   CPU_TIMER1    : origin = 0x000C08, length = 0x000008     /* CPU Timer1 registers */
   CPU_TIMER2    : origin = 0x000C10, length = 0x000008     /* CPU Timer2 registers */

   DACA          : origin = 0x005C00, length = 0x000010
   DACB          : origin = 0x005C10, length = 0x000010
   DACC          : origin = 0x005C20, length = 0x000010

   DMA          : origin = 0x001000, length = 0x000200
   DMACLASRCSEL : origin = 0x007980, length = 0x000040

   ECAP1        : origin = 0x005000, length = 0x000020     /* Enhanced Capture 1 registers */
   ECAP2        : origin = 0x005020, length = 0x000020     /* Enhanced Capture 2 registers */
   ECAP3        : origin = 0x005040, length = 0x000020     /* Enhanced Capture 3 registers */
   ECAP4        : origin = 0x005060, length = 0x000020     /* Enhanced Capture 4 registers */
   ECAP5        : origin = 0x005080, length = 0x000020     /* Enhanced Capture 5 registers */
   ECAP6        : origin = 0x0050A0, length = 0x000020     /* Enhanced Capture 6 registers */

   EMIF1        : origin = 0x047000, length = 0x000800
   EMIF2        : origin = 0x047800, length = 0x000800

   EQEP1        : origin = 0x005100, length = 0x000040     /* Enhanced QEP 1 registers */
   EQEP2        : origin = 0x005140, length = 0x000040     /* Enhanced QEP 2 registers */
   EQEP3        : origin = 0x005180, length = 0x000040     /* Enhanced QEP 3 registers */

   EPWM1        : origin = 0x004000, length = 0x000100     /* Enhanced PWM 1 registers */
   EPWM2        : origin = 0x004100, length = 0x000100     /* Enhanced PWM 2 registers */
   EPWM3        : origin = 0x004200, length = 0x000100     /* Enhanced PWM 3 registers */
   EPWM4        : origin = 0x004300, length = 0x000100     /* Enhanced PWM 4 registers */
   EPWM5        : origin = 0x004400, length = 0x000100     /* Enhanced PWM 5 registers */
   EPWM6        : origin = 0x004500, length = 0x000100     /* Enhanced PWM 6 registers */
   EPWM7        : origin = 0x004600, length = 0x000100     /* Enhanced PWM 7 registers */
   EPWM8        : origin = 0x004700, length = 0x000100     /* Enhanced PWM 8 registers */
   EPWM9        : origin = 0x004800, length = 0x000100     /* Enhanced PWM 9 registers */
   EPWM10       : origin = 0x004900, length = 0x000100     /* Enhanced PWM 10 registers */
   EPWM11       : origin = 0x004A00, length = 0x000100     /* Enhanced PWM 11 registers */
   EPWM12       : origin = 0x004B00, length = 0x000100     /* Enhanced PWM 12 registers */

   EPWM_XBAR    : origin = 0x007A00, length = 0x000040

   FLASH0_CTRL  : origin = 0x05F800, length = 0x000300
   FLASH0_ECC   : origin = 0x05FB00, length = 0x000040

   GPIOCTRL     : origin = 0x007C00, length = 0x000180     /* GPIO control registers */
   GPIODAT      : origin = 0x007F00, length = 0x000030     /* GPIO data registers */

   OUTPUT_XBAR  : origin = 0x007A80, length = 0x000040
   I2CA         : origin = 0x007300, length = 0x000040     /* I2C-A registers */
   I2CB         : origin = 0x007340, length = 0x000040     /* I2C-B registers */

   IPC          : origin = 0x050000, length = 0x000024

   FLASHPUMPSEMAPHORE   : origin = 0x050024, length = 0x000002

   ROMPREFETCH  : origin = 0x05E608, length = 0x000002

   MEMCFG       : origin = 0x05F400, length = 0x000080     /* Mem Config registers */
   EMIF1CONFIG  : origin = 0x05F480, length = 0x000020     /* Emif-1 Config registers */
   EMIF2CONFIG  : origin = 0x05F4A0, length = 0x000020     /* Emif-2 Config registers */
   ACCESSPROTECTION  : origin = 0x05F4C0, length = 0x000040     /* Access Protection registers */
   MEMORYERROR  : origin = 0x05F500, length = 0x000040     /* Access Protection registers */
   ROMWAITSTATE : origin = 0x05F540, length = 0x000002     /* ROM Config registers */


   MCBSPA       : origin = 0x006000, length = 0x000040     /* McBSP-A registers */
   MCBSPB       : origin = 0x006040, length = 0x000040     /* McBSP-A registers */

   NMIINTRUPT   : origin = 0x007060, length = 0x000010     /* NMI Watchdog Interrupt Registers */

   PIE_CTRL     : origin = 0x000CE0, length = 0x000020     /* PIE control registers */
   PIE_VECT     : origin = 0x000D00, length = 0x000200     /* PIE Vector Table */
   SCIA         : origin = 0x007200, length = 0x000010     /* SCI-A registers */
   SCIB         : origin = 0x007210, length = 0x000010     /* SCI-B registers */
   SCIC         : origin = 0x007220, length = 0x000010     /* SCI-C registers */
   SCID         : origin = 0x007230, length = 0x000010     /* SCI-D registers */

   SDFM1        : origin = 0x005E00, length = 0x000080     /* Sigma delta 1 registers */
   SDFM2        : origin = 0x005E80, length = 0x000080     /* Sigma delta 2 registers */

   SPIA         : origin = 0x006100, length = 0x000010
   SPIB         : origin = 0x006110, length = 0x000010
   SPIC         : origin = 0x006120, length = 0x000010
   SPID         : origin = 0x006130, length = 0x000010

   UPP          : origin = 0x006200, length = 0x000100     /* uPP registers */

   DEV_CFG     : origin = 0x05D000, length = 0x000180
   CLK_CFG     : origin = 0x05D200, length = 0x000100
   CPU_SYS     : origin = 0x05D300, length = 0x000100

   INPUT_XBAR   : origin = 0x007900, length = 0x000020
   XBAR         : origin = 0x007920, length = 0x000020
   SYNC_SOC     : origin = 0x007940, length = 0x000010
   WD           : origin = 0x007000, length = 0x000040

   XINT         : origin = 0x007070, length = 0x000010

   DCSM_Z1      : origin = 0x05F000, length = 0x000030     /* Zone 1 Dual code security module registers */
   DCSM_Z2      : origin = 0x05F040, length = 0x000030     /* Zone 2 Dual code security module registers */
   DCSM_COMMON  : origin = 0x05F070, length = 0x000010     /* Common Dual code security module registers */

}


SECTIONS
{
/*** PIE Vect Table and Boot ROM Variables Structures ***/
  UNION run = PIE_VECT
   {
      PieVectTableFile
      GROUP
      {
         EmuBModeVar
         EmuBootPinsVar
      }
   }

   AdcaResultFile        : > ADCA_RESULT, type=NOINIT
   AdcbResultFile        : > ADCB_RESULT, type=NOINIT
   AdccResultFile        : > ADCC_RESULT, type=NOINIT
   AdcdResultFile        : > ADCD_RESULT,  type=NOINIT

   AdcaRegsFile          : > ADCA,         type=NOINIT
   AdcbRegsFile          : > ADCB,         type=NOINIT
   AdccRegsFile          : > ADCC,         type=NOINIT
   AdcdRegsFile          : > ADCD,         type=NOINIT

   AnalogSubsysRegsFile  : > ANALOG_SUBSYS, type=NOINIT

   CanaRegsFile          : > CANA,         type=NOINIT
   CanbRegsFile          : > CANB,         type=NOINIT

   Cla1RegsFile          : > CLA1,         type=NOINIT
   Cla1SoftIntRegsFile   : > PIE_CTRL,     type=NOINIT, type=DSECT

   ClbXbarRegsFile       : > CLB_XBAR     type=NOINIT

   Cmpss1RegsFile        : > CMPSS1,      type=NOINIT
   Cmpss2RegsFile        : > CMPSS2,      type=NOINIT
   Cmpss3RegsFile        : > CMPSS3,      type=NOINIT
   Cmpss4RegsFile        : > CMPSS4,      type=NOINIT
   Cmpss5RegsFile        : > CMPSS5,      type=NOINIT
   Cmpss6RegsFile        : > CMPSS6,      type=NOINIT
   Cmpss7RegsFile        : > CMPSS7,      type=NOINIT
   Cmpss8RegsFile        : > CMPSS8,      type=NOINIT

   CpuTimer0RegsFile     : > CPU_TIMER0,    type=NOINIT
   CpuTimer1RegsFile     : > CPU_TIMER1,    type=NOINIT
   CpuTimer2RegsFile     : > CPU_TIMER2,    type=NOINIT

   DacaRegsFile          : > DACA          type=NOINIT
   DacbRegsFile          : > DACB          type=NOINIT
   DaccRegsFile          : > DACC          type=NOINIT

   DcsmZ1RegsFile        : > DCSM_Z1,          type=NOINIT
   DcsmZ2RegsFile        : > DCSM_Z2,          type=NOINIT
   DcsmCommonRegsFile    : > DCSM_COMMON,      type=NOINIT

   DmaRegsFile           : > DMA           type=NOINIT
   DmaClaSrcSelRegsFile  : > DMACLASRCSEL  type=NOINIT

   ECap1RegsFile         : > ECAP1,        type=NOINIT
   ECap2RegsFile         : > ECAP2,        type=NOINIT
   ECap3RegsFile         : > ECAP3,        type=NOINIT
   ECap4RegsFile         : > ECAP4,        type=NOINIT
   ECap5RegsFile         : > ECAP5,        type=NOINIT
   ECap6RegsFile         : > ECAP6,        type=NOINIT

   Emif1RegsFile         : > EMIF1         type=NOINIT
   Emif2RegsFile         : > EMIF2         type=NOINIT

   EPwm1RegsFile         : > EPWM1,        type=NOINIT
   EPwm2RegsFile         : > EPWM2,        type=NOINIT
   EPwm3RegsFile         : > EPWM3,        type=NOINIT
   EPwm4RegsFile         : > EPWM4,        type=NOINIT
   EPwm5RegsFile         : > EPWM5,        type=NOINIT
   EPwm6RegsFile         : > EPWM6,        type=NOINIT
   EPwm7RegsFile         : > EPWM7,        type=NOINIT
   EPwm8RegsFile         : > EPWM8,        type=NOINIT
   EPwm9RegsFile         : > EPWM9,        type=NOINIT
   EPwm10RegsFile        : > EPWM10,       type=NOINIT
   EPwm11RegsFile        : > EPWM11,       type=NOINIT
   EPwm12RegsFile        : > EPWM12,       type=NOINIT

   EPwmXbarRegsFile      : > EPWM_XBAR     type=NOINIT

   EQep1RegsFile         : > EQEP1,        type=NOINIT
   EQep2RegsFile         : > EQEP2,        type=NOINIT
   EQep3RegsFile         : > EQEP3,        type=NOINIT

   Flash0CtrlRegsFile     : > FLASH0_CTRL    type=NOINIT
   Flash0EccRegsFile      : > FLASH0_ECC     type=NOINIT

   GpioCtrlRegsFile      : > GPIOCTRL,     type=NOINIT
   GpioDataRegsFile      : > GPIODAT,      type=NOINIT

   OutputXbarRegsFile    : > OUTPUT_XBAR    type=NOINIT
   I2caRegsFile          : > I2CA,          type=NOINIT
   I2cbRegsFile          : > I2CB,          type=NOINIT
   InputXbarRegsFile     : > INPUT_XBAR     type=NOINIT
   XbarRegsFile          : > XBAR           type=NOINIT
   IpcRegsFile           : > IPC,           type=NOINIT

   FlashPumpSemaphoreRegsFile   : > FLASHPUMPSEMAPHORE,    type=NOINIT

   RomPrefetchRegsFile       : > ROMPREFETCH,       type=NOINIT
   MemCfgRegsFile            : > MEMCFG,            type=NOINIT
   Emif1ConfigRegsFile       : > EMIF1CONFIG,       type=NOINIT
   Emif2ConfigRegsFile       : > EMIF2CONFIG,       type=NOINIT
   AccessProtectionRegsFile  : > ACCESSPROTECTION,  type=NOINIT
   MemoryErrorRegsFile       : > MEMORYERROR,       type=NOINIT
   RomWaitStateRegsFile      : > ROMWAITSTATE,      type=NOINIT

   McbspaRegsFile        : > MCBSPA,       type=NOINIT
   McbspbRegsFile        : > MCBSPB,       type=NOINIT

   UppRegsFile           : > UPP,          type=NOINIT

   NmiIntruptRegsFile    : > NMIINTRUPT,   type=NOINIT
   PieCtrlRegsFile       : > PIE_CTRL,     type=NOINIT

   SciaRegsFile          : > SCIA,         type=NOINIT
   ScibRegsFile          : > SCIB,         type=NOINIT
   ScicRegsFile          : > SCIC,         type=NOINIT
   ScidRegsFile          : > SCID,         type=NOINIT

   Sdfm1RegsFile         : > SDFM1,        type=NOINIT
   Sdfm2RegsFile         : > SDFM2,        type=NOINIT

   SpiaRegsFile          : > SPIA,        type=NOINIT
   SpibRegsFile          : > SPIB,        type=NOINIT
   SpicRegsFile          : > SPIC,        type=NOINIT
   SpidRegsFile          : > SPID,        type=NOINIT

   DevCfgRegsFile        : > DEV_CFG,     type=NOINIT
   ClkCfgRegsFile        : > CLK_CFG,     type=NOINIT
   CpuSysRegsFile        : > CPU_SYS,     type=NOINIT

   SyncSocRegsFile       : > SYNC_SOC,    type=NOINIT

   WdRegsFile            : > WD,           type=NOINIT

   XintRegsFile          : > XINT          type=NOINIT
   MemCfgRegs            : > MEMCFG        type=NOINIT

}

/*
//===========================================================================
// End of file.
//===========================================================================
*/
