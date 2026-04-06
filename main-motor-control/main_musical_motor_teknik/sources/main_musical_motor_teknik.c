//#############################################################################
//
// FILE:   main_musical_motor_teknik.c
//
// TITLE:  Musical Motor Controller - Teknik Motor
//
// Target: TMS320F28379D (CPU1), LAUNCHXL-F28379D + BOOSTXL-3PhGaNInv
//
// Description:
//   Boots the board, selects a melody, and wires the musical motor modules
//   together. Song data lives in musical_motor_songs.c, the tone engine lives
//   in musical_motor_tone.c, and low-level PWM/GPIO setup lives in
//   musical_motor_hw.c.
//
//#############################################################################

#include "device.h"
#include "driverlib.h"

#include "musical_motor_hw.h"
#include "musical_motor_songs.h"
#include "musical_motor_tone.h"

#define SONG_SELECT     MUSICAL_MOTOR_SONG_MARIO
#define TONE_VQ_DEFAULT 0.2f

static void initPIE(void);

void main(void)
{
    Device_init();

    Device_initGPIO();
    MusicalMotorHw_initGPIO();

    initPIE();

    Interrupt_register(INT_EPWM1, &MusicalMotorTone_epwm1ISR);
    Interrupt_register(INT_TIMER0, &MusicalMotorTone_cpuTimer0ISR);

    MusicalMotorHw_initEPWM();

    MusicalMotorTone_init(MusicalMotorSongs_getById(SONG_SELECT),
                          TONE_VQ_DEFAULT);

    MusicalMotorHw_initCPUTimer0();

    Interrupt_enable(INT_EPWM1);
    Interrupt_enable(INT_TIMER0);

    MusicalMotorHw_enableGateDriver();

    EINT;
    ERTM;

    for(;;)
    {
        MusicalMotorHw_toggleHeartbeat();
        DEVICE_DELAY_US(500000);
    }
}

static void initPIE(void)
{
    Interrupt_initModule();
    Interrupt_initVectorTable();
}
