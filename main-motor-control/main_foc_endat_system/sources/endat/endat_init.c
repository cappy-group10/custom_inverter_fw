//----------------------------------------------------------------------------------
//	FILE:			endat_init.c
//
//	Description:	Hardware initialisation, GPIO/XBAR/EPWM configuration, and
//					delay compensation for the EnDat encoder interface.
//
//					GPIO, XBAR and EPWM helpers are static — they are
//					implementation details of EnDat_Init() and are not part of
//					the public API.
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

//=============================================================================
// File-private hardware setup helpers
// Not exposed in endat.h — called only from EnDat_Init().
//=============================================================================

static void EPWM4_Config(void)
{
    // Force PWMA and PWMB outputs high as the default idle state for EnDat clk
    EALLOW;
    EPwm4Regs.TZCTL.bit.TZA = 1;
    EPwm4Regs.TZCTL.bit.TZB = 1;
    EPwm4Regs.TZFRC.bit.OST = 1;
    EDIS;
}

static void Endat_setup_GPIO(void)
{
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1; // GPIO6  -> EnDat CLK master (EPWM4A)
    GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 1; // GPIO7  -> SPI CLK slave (EPWM4B)

    // Group-mux (GMUX) must be set before MUX for peripheral mode > 1
    GpioCtrlRegs.GPBGMUX2.bit.GPIO63 = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO64 = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO65 = 3;
    GpioCtrlRegs.GPCGMUX1.bit.GPIO66 = 3;

    GpioCtrlRegs.GPBMUX2.bit.GPIO63 = 3; // GPIO63 -> SPISIMOB
    GpioCtrlRegs.GPCMUX1.bit.GPIO64 = 3; // GPIO64 -> SPISOMIB
    GpioCtrlRegs.GPCMUX1.bit.GPIO65 = 3; // GPIO65 -> SPICLKB
    GpioCtrlRegs.GPCMUX1.bit.GPIO66 = 3; // GPIO66 -> SPISTEB

    // Asynchronous input qualification for SPI-B pins
    GpioCtrlRegs.GPBQSEL2.bit.GPIO63 = 3;
    GpioCtrlRegs.GPCQSEL1.bit.GPIO64 = 3;
    GpioCtrlRegs.GPCQSEL1.bit.GPIO65 = 3;
    GpioCtrlRegs.GPCQSEL1.bit.GPIO66 = 3;

    GpioCtrlRegs.GPAMUX1.bit.GPIO9  = 3; // GPIO9   -> EnDat TxEN
    GpioCtrlRegs.GPEDIR.bit.GPIO139 = 1; // GPIO139 -> EnDat 5 V power control (output)
    EDIS;
}

static void Endat_config_XBAR(void)
{
    EALLOW;
    // Route SPISIMOB (GPIO63) to InputXBAR Input1 for use as GPTRIP XBAR TRIP1
    InputXbarRegs.INPUT1SELECT = 63;
    EDIS;
}

//=============================================================================
// Public API
//=============================================================================

void EnDat_Init(void)
{
    EALLOW;
    // Enable clocks to EPWM1–4
    CpuSysRegs.PCLKCR2.bit.EPWM1 = 1;
    CpuSysRegs.PCLKCR2.bit.EPWM2 = 1;
    CpuSysRegs.PCLKCR2.bit.EPWM3 = 1;
    CpuSysRegs.PCLKCR2.bit.EPWM4 = 1;
    EDIS;

    EPWM4_Config();

    // Build the CRC lookup table for polynomial POLY1
    PM_endat22_generateCRCTable(NBITS_POLY1, POLY1, endat22CRCtable);

    Endat_setup_GPIO();
    Endat_config_XBAR();

    endat22Data.spi = &SpibRegs;
    PM_endat22_setupPeriph();

    EALLOW;
    PieVectTable.SPIB_RX_INT        = &spiRxFifoIsr;
    PieCtrlRegs.PIECTRL.bit.ENPIE   = 1; // Enable PIE block
    PieCtrlRegs.PIEIER6.bit.INTx3   = 1; // Enable PIE group 6, INT 3 (SPI-B RX)
    IER |= 0x20;                          // Enable CPU INT6
    EINT;
    EDIS;

    // Power up EnDat 5 V supply via GPIO139, then perform the encoder
    // power-on clock sequence as required by the EnDat specification.
    GpioDataRegs.GPEDAT.bit.GPIO139 = 1;
    DELAY_US(10000L);    // 10 ms — supply ramp time

    // Drive CLK high for 100 ms via GPIO (EPWM mux disabled temporarily)
    EALLOW;
    GpioCtrlRegs.GPADIR.bit.GPIO6   = 1; // Output
    GpioDataRegs.GPASET.bit.GPIO6   = 1; // High
    GpioCtrlRegs.GPAMUX1.bit.GPIO6  = 0; // Plain GPIO
    DELAY_US(100000L);   // 100 ms

    GpioDataRegs.GPADAT.bit.GPIO6   = 0; // CLK low (>125 ns pulse)
    DELAY_US(425000L);   // 425 ms — encoder reset time

    GpioCtrlRegs.GPAMUX1.bit.GPIO6  = 1; // Restore EPWM4A mux
    EDIS;
    DELAY_US(425000L);   // 425 ms — encoder ready time (>381 ms)

    // EncData must be low before proceeding
    if (GpioDataRegs.GPBDAT.bit.GPIO63 == 1)
    {
        ESTOP0;
    }

    PM_endat22_setFreq(ENDAT_INIT_FREQ_DIVIDER);

    // Encoder Receive Reset — issued twice per EnDat specification
    PM_endat22_setupCommand(ENCODER_RECEIVE_RESET, 0xAA, 0x2222, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_RECEIVE_RESET, 0);
    DELAY_US(1000000L);  // 1 s

    PM_endat22_setupCommand(ENCODER_RECEIVE_RESET, 0xAA, 0x2222, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_RECEIVE_RESET, 0);
    DELAY_US(2000L);

    // Select memory area: MRS Code 0xA1 — Parameters of Encoder Manufacturer
    PM_endat22_setupCommand(SELECTION_OF_MEMORY_AREA, 0xA1, 0x5555, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(SELECTION_OF_MEMORY_AREA, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcNorm(endat22Data.address,
                                              endat22Data.data,
                                              endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);

    // Address 0xD: number of clock pulses required to shift out position data
    PM_endat22_setupCommand(ENCODER_SEND_PARAMETER, 0xD, 0xAAAA, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_PARAMETER, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcNorm(endat22Data.address,
                                              endat22Data.data,
                                              endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }

    // Must be set before any position data access
    endat22Data.position_clocks = endat22Data.data & 0xFF;

    DELAY_US(200L);
}

//
// EnDat_initDelayComp
// Measures propagation delay over two transactions and stores the average in
// endat22Data.delay_comp to activate hardware delay compensation.
// NOTE: Must only be called at low frequency (~200 kHz).
//
void EnDat_initDelayComp(void)
{
    uint16_t delay1, delay2;

    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT21,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);
    delay1 = PM_endat22_getDelayCompVal();

    PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);
    {
        uint16_t crc5 = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT21,
                                             endat22Data.position_lo, endat22Data.position_hi,
                                             endat22Data.error1, endat22Data.error2,
                                             endat22CRCtable);
        if (!CheckCRC(crc5, endat22Data.data_crc)) { ESTOP0; }
    }
    DELAY_US(200L);
    delay2 = PM_endat22_getDelayCompVal();

    endat22Data.delay_comp = (delay1 + delay2) >> 1;
}

//***************************************************************************
// End of file
//***************************************************************************