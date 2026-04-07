//###########################################################################
//  This software is licensed for use with Texas Instruments C28x
//  family Microcontrollers.
// ------------------------------------------------------------------------
//          Copyright (C) 2015 Texas Instruments, Incorporated.
//                          All Rights Reserved.
// ==========================================================================
//
// FILE:   PM_endat22_Include.h
//
// TITLE:  Prototypes and Definitions for the Position Manager Endat22 Library
//
// AUTHOR: Subrahmanya Bharathi Akondy (C2000 Systems Solutions, Houston, TX)
//
// DATE:   Jun, 2015
//
// MODIFIED: Converted from HAL/bitfield style to driverlib.
//           Replaced #include "F28x_Project.h" with #include "device.h"
//           "F28x_Project.h" pulled in the bitfield register structs (SpiaRegs,
//           GpioCtrlRegs, etc.) and the old "interrupt" keyword via F28x_Project.h
//           -> DSP28x_Project.h -> DSP2837xD_Device.h. In a driverlib project,
//           device.h provides all necessary types and the SPI_REGS struct is
//           still available through the device peripheral headers.
//###########################################################################

#ifndef PM_ENDAT22_INCLUDE_H
#define PM_ENDAT22_INCLUDE_H

#include <stdint.h>
#include "device.h"         // replaces "F28x_Project.h" — provides SPI_REGS,
                            // uint16_t/uint32_t, DEVICE_DELAY_US, etc.

#include "F2837xD_device.h"   // for raw register struct types like SPI_REGS (SpibRegs) used in ENDAT_DATA_STRUCT

#define ENDAT22 1
#define ENDAT21 0

#define ENDAT_CRC_LENGTH            5
#define ENDAT_CRC_POLYNOMIAL        0x0B

// EnDat command mode codes
#define ENCODER_SEND_POSITION_VALUES                                    0x07
#define SELECTION_OF_MEMORY_AREA                                        0x0E
#define ENCODER_RECEIVE_PARAMETER                                       0x1C
#define ENCODER_SEND_PARAMETER                                          0x23
#define ENCODER_RECEIVE_RESET                                           0x2A
#define ENCODER_SEND_TEST_VALUES                                        0x15
#define ENCODER_RECEIVE_TEST_COMMAND                                    0x31
#define ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA               0x38
#define ENCODER_SEND_POSITION_VALUES_AND_SELECTION_OF_THE_MEMORY_AREA   0x09
#define ENCODER_SEND_POSITION_VALUES_AND_RECEIVE_PARAMETER              0x1B
#define ENCODER_SEND_POSITION_VALUES_AND_SEND_PARAMETER                 0x24
#define ENCODER_SEND_POSITION_VALUES_AND_RECEIVE_TEST_COMMAND           0x36
#define ENCODER_SEND_POSITION_VALUES_AND_RECEIVE_ERROR_RESET            0x2D
#define ENCODER_RECEIVE_COMMUNICATION_COMMAND                           0x12

// EnDat data structure
// The spi field points to the raw SPI register struct (SpibRegs).
// volatile struct SPI_REGS is defined in the device peripheral headers
// which are included transitively through device.h -> driverlib.h -> spi.h.
typedef struct {
    uint32_t  position_lo;
    uint32_t  position_hi;
    uint16_t  error1;
    uint16_t  error2;
    uint16_t  data;
    uint16_t  data_crc;
    uint16_t  address;
    uint32_t  additional_data1;
    uint32_t  additional_data2;
    uint32_t  additional_data1_crc;
    uint32_t  additional_data2_crc;
    uint32_t  test_lo;
    uint32_t  test_hi;
    uint32_t  position_clocks;
    volatile struct SPI_REGS *spi;
    uint32_t  delay_comp;
    uint32_t  sdata[16];
    uint32_t  rdata[16];
    volatile uint16_t dataReady;
    uint16_t  fifo_level;
} ENDAT_DATA_STRUCT;

extern ENDAT_DATA_STRUCT endat22Data;

// Library function prototypes (implemented in PM_endat22_lib.lib)
extern uint16_t PM_endat22_setupCommand(uint16_t cmd, uint16_t data1,
                                        uint16_t data2, uint16_t nAddData);
extern uint16_t PM_endat22_receiveData(uint16_t cmd, uint16_t nAddData);
extern void     PM_endat22_setupPeriph(void);
extern void     PM_endat22_setFreq(uint32_t Freq_us);
extern void     PM_endat22_startOperation(void);
extern uint16_t PM_endat22_getDelayCompVal(void);

// CRC library prototypes
#define NBITS_POLY1         5
#define POLY1               0x0B
#define RXLEN               4
#define PARITY              0
#define SIZEOF_ENDAT_CRCTABLE 256

extern uint16_t endat22CRCtable[SIZEOF_ENDAT_CRCTABLE];

extern void     PM_endat22_generateCRCTable(uint16_t nBits, uint16_t polynomial,
                                            uint16_t *pTable);
extern uint32_t PM_endat22_getCrcPos(uint32_t total_clocks, uint32_t endat22,
                                     uint32_t lowpos, uint32_t highpos,
                                     uint32_t error1, uint32_t error2,
                                     uint16_t *crc_table);
extern uint32_t PM_endat22_getCrcNorm(uint32_t param8, uint32_t param16,
                                      uint16_t *crc_table);
extern uint32_t PM_endat22_getCrcTest(uint32_t lowtest, uint32_t hightest,
                                      uint32_t error1, uint16_t *crc_table);

#endif // PM_ENDAT22_INCLUDE_H
