//----------------------------------------------------------------------------------
//  FILE:           endat.c
//
//  Description:    EnDat encoder interface - CONVERTED TO DRIVERLIB
//                  Original used F28x HAL/bitfield style (F28x_Project.h).
//                  This version uses C2000 driverlib exclusively to match the
//                  dual_axis_servo_drive project build environment.
//
//  Target:         TMS320F28379D
//
//  Conversion notes:
//    - EALLOW/EDIS       -> not needed; driverlib handles internally
//    - CpuSysRegs        -> SysCtl_enablePeripheral()
//    - GpioCtrlRegs      -> GPIO_setPinConfig() / GPIO_setDirectionMode() /
//                           GPIO_setQualificationMode()
//    - GpioDataRegs      -> GPIO_writePin() / GPIO_readPin()
//    - PieVectTable      -> Interrupt_register()
//    - PieCtrlRegs       -> Interrupt_enable()
//    - IER |= 0x20       -> Interrupt_enable(INT_SPIB_RX)  (CPU INT6 = SPIB)
//    - EINT              -> Interrupt_enableMaster() / __enable_interrupts()
//    - EPwm4Regs         -> EPWM_setTripZoneAction() / EPWM_forceTripZoneEvent()
//    - InputXbarRegs     -> XBAR_setInputPin()
//    - SpibRegs (pointer)-> unchanged in ENDAT_DATA_STRUCT; spi field kept as-is
//    - DELAY_US          -> DEVICE_DELAY_US() [defined in device.h, same macro]
//    - ESTOP0            -> asm(" ESTOP0") — unchanged
//
//  GPIO assignments (same as original):
//    GPIO6  -> EPWM4A / EnDat CLK master (mux=1 for PWM, mux=0 for GPIO)
//    GPIO7  -> EPWM4B / SPI CLK slave    (mux=1)
//    GPIO9  -> SPIB TX-EN                (mux=3)
//    GPIO63 -> SPISIMOB                  (gmux=3, mux=3)
//    GPIO64 -> SPISOMIB                  (gmux=3, mux=3)
//    GPIO65 -> SPICLKB                   (gmux=3, mux=3)
//    GPIO66 -> SPISTEB                   (gmux=3, mux=3)
//    GPIO139-> EnDat 5V power control    (GPIO output)
//----------------------------------------------------------------------------------

#include "device.h"         // driverlib device header (pulls in driverlib.h)
#include "endat.h"

uint16_t endat22CRCtable[SIZEOF_ENDAT_CRCTABLE];	// Declare CRC table for EnDat CRC calculations

ENDAT_DATA_STRUCT endat22Data;		//PM EnDat22 data structure

uint16_t crc5_result;	//variable for calculated crc result checking
uint16_t retval1;		//used for function return val storage and checks

//
// Forward declarations of static helpers
//
static void EPWM4_Config(void);
static void Endat_setup_GPIO(void);
static void Endat_config_XBAR(void);

// ---------------------------------------------------------------------------
// EnDat_Init
// ---------------------------------------------------------------------------
void EnDat_Init(void)
{
    // ----- Enable EPWM1-4 clocks -----
    // Original: CpuSysRegs.PCLKCR2.bit.EPWMx = 1  (inside EALLOW/EDIS)
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM1);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM2);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM3);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM4);

    // ----- Configure EPWM4 trip-zone defaults (CLK lines high at rest) -----
    EPWM4_Config();

    // ----- Generate CRC table for EnDat Polynomial defied as POLY1 -----
    PM_endat22_generateCRCTable(NBITS_POLY1, POLY1, endat22CRCtable);

    // ----- GPIO pin configuration -----
    Endat_setup_GPIO();

    // ----- Input XBAR -----
    Endat_config_XBAR();

    // ----- Point EnDat data struct at SPIB register set -----
    // The ENDAT_DATA_STRUCT.spi field is a volatile struct SPI_REGS*
    // SpibRegs is still valid in driverlib projects (it's the raw register struct).
    endat22Data.spi = &SpibRegs;
    PM_endat22_setupPeriph();

    // ----- Interrupt setup -----
    // Original:
	//   EALLOW;
    //   PieVectTable.SPIB_RX_INT = &spiRxFifoIsr;
    //   PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
    //   PieCtrlRegs.PIEIER6.bit.INTx3 = 1;   // Enable PIE Group 6, INT 9, int 3 = SPIB RX
    //   IER |= 0x20;                          // CPU INT6
    //   EINT;
	// 	 EDIS;
    Interrupt_register(INT_SPIB_RX, &spiRxFifoIsr);
    Interrupt_enable(INT_SPIB_RX);          // enables PIE group 6 INT3 + CPU INT6
    Interrupt_enableMaster();               // EINT equivalent

    // ----- Power up EnDat 5V supply via GPIO139 -----
    // Original: GpioDataRegs.GPEDAT.bit.GPIO139 = 1
    GPIO_writePin(139, 1);
	//EncCLK high for 100ms
    DEVICE_DELAY_US(10000UL);   // 10 ms

    // ----- EnDat power-on CLK sequencing (section 7 of EnDat spec) -----
    // Drive GPIO6 high as plain GPIO output for 100 ms
    // Original sequence:
    //   GpioCtrlRegs.GPADIR.bit.GPIO6 = 1   -> output
    //   GpioDataRegs.GPASET.bit.GPIO6 = 1
    //   GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0  -> plain GPIO
    GPIO_setDirectionMode(6, GPIO_DIR_MODE_OUT);
    GPIO_writePin(6, 1);
    GPIO_setPinConfig(GPIO_6_GPIO6);        // mux=0: plain GPIO
    DEVICE_DELAY_US(100000UL);  // 100 ms

    // CLK low for >125 ns (using 425 ms to be safe, matching original)
    // Original: GpioDataRegs.GPADAT.bit.GPIO6 = 0
    GPIO_writePin(6, 0);
    DEVICE_DELAY_US(425000UL);  // 425 ms

    // CLK high for >381 ms via EPWM mux
    // Original: GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1  -> restore EPWM function
    GPIO_setPinConfig(GPIO_6_EPWM4A);      // mux=1: back to EPWM4A
    DEVICE_DELAY_US(425000UL);  // 425 ms

    // ----- Verify encoder data line is low -----
    // Original: if (GpioDataRegs.GPBDAT.bit.GPIO63 == 1) ESTOP0;
    if (GPIO_readPin(63) == 1U)
    {
        asm(" ESTOP0");
    }

    // ----- Set initial communication frequency -----
    PM_endat22_setFreq(ENDAT_INIT_FREQ_DIVIDER);

    // ----- Encoder Receive Reset (command ERR) x2 -----
    retval1 = PM_endat22_setupCommand(ENCODER_RECEIVE_RESET, 0xAA, 0x2222, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_RECEIVE_RESET, 0);
    DEVICE_DELAY_US(1000000UL); // 1 s

    retval1 = PM_endat22_setupCommand(ENCODER_RECEIVE_RESET, 0xAA, 0x2222, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_RECEIVE_RESET, 0);
    DEVICE_DELAY_US(2000UL);

    // ----- Select Memory Area: encoder manufacturer parameters (MRS=0xA1) -----
    retval1 = PM_endat22_setupCommand(SELECTION_OF_MEMORY_AREA, 0xA1, 0x5555, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(SELECTION_OF_MEMORY_AREA, 0);
    crc5_result = PM_endat22_getCrcNorm(endat22Data.address, endat22Data.data, endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc))
    {
        asm(" ESTOP0");
    }
    DEVICE_DELAY_US(200UL);

    // ----- Read position clock count (address 0xD) -----
    retval1 = PM_endat22_setupCommand(ENCODER_SEND_PARAMETER, 0xD, 0xAAAA, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_SEND_PARAMETER, 0);
    crc5_result = PM_endat22_getCrcNorm(endat22Data.address, endat22Data.data, endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc))
    {
        asm(" ESTOP0");
    }

    endat22Data.position_clocks = endat22Data.data & 0xFF;
    DEVICE_DELAY_US(200UL);
}

// ---------------------------------------------------------------------------
// Endat_setup_GPIO  (static helper)
// ---------------------------------------------------------------------------
// Pin config values come from driverlib's pin_map.h for F2837xD.
// GPIO_x_EPWMy_z / GPIO_x_SPIByyy macros encode the MUX+GMUX values.
// ---------------------------------------------------------------------------
static void Endat_setup_GPIO(void)
{
    // GPIO6 -> EPWM4A (mux=1): EnDat CLK master output
    // Confirmed from pin_map.h: GPIO_6_EPWM4A
    GPIO_setPinConfig(GPIO_6_EPWM4A);

    // GPIO7 -> EPWM4B (mux=1): SPI CLK slave reference
    // Confirmed from pin_map.h: GPIO_7_EPWM4B
    GPIO_setPinConfig(GPIO_7_EPWM4B);

    // GPIO9 -> EnDat TxEN.
    // The original HAL set GPIO9 mux=3 but pin_map.h has no SPIB/peripheral
    // entry at mux=3 for GPIO9 on this device — it is a plain GPIO output.
    // Drive it high to assert the RS-485 transmit enable on the EnDat interface.
    GPIO_setPinConfig(GPIO_9_GPIO9);
    GPIO_setDirectionMode(9, GPIO_DIR_MODE_OUT);
    GPIO_writePin(9, 1);

    // GPIO63 -> SPISIMOB
    // Confirmed from pin_map.h: GPIO_63_SPISIMOB
    GPIO_setPinConfig(GPIO_63_SPISIMOB);
    GPIO_setQualificationMode(63, GPIO_QUAL_ASYNC);

    // GPIO64/65/66 -> SPIB pins.
    // grep of pin_map.h returned NO SPIB entries for these GPIOs in this
    // C2000Ware version. Two possibilities:
    //   (a) SPIB is muxed onto different GPIOs in this device variant — check
    //       pin_map.h with: grep -E "SPIB|SPISIMOB|SPISOMIB" include/driverlib/pin_map.h
    //   (b) These GPIOs exist but use a raw hex value not matching the grep filter.
    // TODO: run the grep above and replace the raw hex values below with the
    //       correct #define names once confirmed.
    //
    // Raw bitfield register values from the original HAL code (gmux=3, mux=3):
    //   GPIO64: GPCGMUX1.GPIO64=3, GPCMUX1.GPIO64=3 -> SPISOMIB
    //   GPIO65: GPCGMUX1.GPIO65=3, GPCMUX1.GPIO65=3 -> SPICLKB
    //   GPIO66: GPCGMUX1.GPIO66=3, GPCMUX1.GPIO66=3 -> SPISTEB
    //
    // If the defines are simply missing from this pin_map version, you can
    // write the mux value directly using the raw hex (gmux<<2|mux packed):
    // Confirmed from pin_map.h: GPIO_64_SPISOMIB (0x0086000FU)
    GPIO_setPinConfig(GPIO_64_SPISOMIB);
    GPIO_setQualificationMode(64, GPIO_QUAL_ASYNC);

    // Confirmed from pin_map.h: GPIO_65_SPICLKB (0x0086020FU)
    GPIO_setPinConfig(GPIO_65_SPICLKB);
    GPIO_setQualificationMode(65, GPIO_QUAL_ASYNC);

    // Confirmed from pin_map.h: GPIO_66_SPISTEB (0x0086040FU)
    GPIO_setPinConfig(GPIO_66_SPISTEB);
    GPIO_setQualificationMode(66, GPIO_QUAL_ASYNC);

    // GPIO139 -> EnDat 5V power control (plain GPIO output, starts low)
    // Confirmed from pin_map.h: GPIO_139_GPIO139
    GPIO_setPinConfig(GPIO_139_GPIO139);
    GPIO_setDirectionMode(139, GPIO_DIR_MODE_OUT);
    GPIO_writePin(139, 0);
}

// ---------------------------------------------------------------------------
// Endat_config_XBAR  (static helper)
// ---------------------------------------------------------------------------
static void Endat_config_XBAR(void)
{
    // Original: InputXbarRegs.INPUT1SELECT = 63  (TRIP1 -> GPIO63 / SPISIMOB)
    XBAR_setInputPin(XBAR_INPUT1, 63);
}

// ---------------------------------------------------------------------------
// EPWM4_Config  (static helper)
// ---------------------------------------------------------------------------
// Sets EPWM4 TZ action to force both A and B outputs high (=1 in TZCTL means
// force high), then triggers a one-shot trip so the outputs sit high at rest,
// which is the idle state for EnDat CLK.
// ---------------------------------------------------------------------------
static void EPWM4_Config(void)
{
    // Original:
    //   EPwm4Regs.TZCTL.bit.TZA = 1  (force high on trip)
    //   EPwm4Regs.TZCTL.bit.TZB = 1
    //   EPwm4Regs.TZFRC.bit.OST = 1  (force one-shot trip event)
    EPWM_setTripZoneAction(EPWM4_BASE,
                           EPWM_TZ_ACTION_EVENT_TZA,
                           EPWM_TZ_ACTION_HIGH);
    EPWM_setTripZoneAction(EPWM4_BASE,
                           EPWM_TZ_ACTION_EVENT_TZB,
                           EPWM_TZ_ACTION_HIGH);
    EPWM_forceTripZoneEvent(EPWM4_BASE, EPWM_TZ_FORCE_EVENT_OST);
}

// ---------------------------------------------------------------------------
// EnDat_initDelayComp
// ---------------------------------------------------------------------------
void EnDat_initDelayComp(void)
{
    uint16_t delay1, delay2;

    retval1 = PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT21,
                                       endat22Data.position_lo,
                                       endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);
    delay1 = PM_endat22_getDelayCompVal();

    retval1 = PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT21,
                                       endat22Data.position_lo,
                                       endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);
    delay2 = PM_endat22_getDelayCompVal();

    endat22Data.delay_comp = (delay1 + delay2) >> 1;
}

// ---------------------------------------------------------------------------
// spiRxFifoIsr  — SPIB RX FIFO interrupt
// ---------------------------------------------------------------------------
// Original used PieCtrlRegs.PIEACK.all |= 0x20 (PIE group 6 ACK).
// Driverlib equivalent: Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP6)
// The ISR keyword and PIE vector registration via Interrupt_register() above
// means CCS will automatically generate the correct interrupt attribute.
// ---------------------------------------------------------------------------
__interrupt void spiRxFifoIsr(void)
{
    uint16_t i;
    for (i = 0; i <= endat22Data.fifo_level; i++)
    {
        endat22Data.rdata[i] = endat22Data.spi->SPIRXBUF;
    }
    endat22Data.spi->SPIFFRX.bit.RXFFOVFCLR = 1;   // Clear overflow flag
    endat22Data.spi->SPIFFRX.bit.RXFFINTCLR = 1;   // Clear interrupt flag

    // Original: PieCtrlRegs.PIEACK.all |= 0x20  (ACK group 6)
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP6);

    endat22Data.dataReady = 1;
}

// ---------------------------------------------------------------------------
// CheckCRC
// ---------------------------------------------------------------------------
uint16_t CheckCRC(uint16_t expectcrc5, uint16_t receivecrc5)
{
    return (expectcrc5 == receivecrc5) ? 1U : 0U;
}

// ---------------------------------------------------------------------------
// endat22_setupAddlData
// ---------------------------------------------------------------------------
void endat22_setupAddlData(void)
{
    retval1 = PM_endat22_setupCommand(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0xA1, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0);
    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);

    retval1 = PM_endat22_setupCommand(
        ENCODER_SEND_POSITION_VALUES_AND_SEND_PARAMETER, 0xD, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(
        ENCODER_SEND_POSITION_VALUES_AND_SEND_PARAMETER, 0);
    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);

    retval1 = PM_endat22_setupCommand(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0x45, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0);
    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);

    retval1 = PM_endat22_setupCommand(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 0x59, 0, 1);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(
        ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA, 1);
    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(2000UL);
}

// ---------------------------------------------------------------------------
// endat22_readPositionWithAddlData
// ---------------------------------------------------------------------------
void endat22_readPositionWithAddlData(void)
{
    retval1 = PM_endat22_setupCommand(
        ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA, 0, 0, 2);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(
        ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA, 2);

    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT22,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }

    crc5_result = PM_endat22_getCrcNorm(
        (endat22Data.additional_data1 >> 16), endat22Data.additional_data1, endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.additional_data1_crc)) { asm(" ESTOP0"); }

    crc5_result = PM_endat22_getCrcNorm(
        (endat22Data.additional_data2 >> 16), endat22Data.additional_data2, endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.additional_data2_crc)) { asm(" ESTOP0"); }

    DEVICE_DELAY_US(200UL);
}

// ---------------------------------------------------------------------------
// endat21_readPosition
// ---------------------------------------------------------------------------
void endat21_readPosition(void)
{
    retval1 = PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, 0, 0, 0);
    PM_endat22_startOperation();
    while (endat22Data.dataReady != 1) {}
    retval1 = PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0);

    crc5_result = PM_endat22_getCrcPos(endat22Data.position_clocks, ENDAT21,
                                       endat22Data.position_lo, endat22Data.position_hi,
                                       endat22Data.error1, endat22Data.error2,
                                       endat22CRCtable);
    if (!CheckCRC(crc5_result, endat22Data.data_crc)) { asm(" ESTOP0"); }
    DEVICE_DELAY_US(200UL);
}

//***************************************************************************
// End of file
//***************************************************************************
