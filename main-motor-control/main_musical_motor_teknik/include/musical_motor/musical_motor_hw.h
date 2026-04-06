#ifndef MUSICAL_MOTOR_HW_H
#define MUSICAL_MOTOR_HW_H

#include "device.h"
#include "driverlib.h"

#define MUSICAL_MOTOR_HW_SYSCLK_FREQ_HZ         DEVICE_SYSCLK_FREQ
#define MUSICAL_MOTOR_HW_PWM_CARRIER_FREQ_HZ    20000U
#define MUSICAL_MOTOR_HW_ISR_PERIOD_S           (1.0f / (float32_t)MUSICAL_MOTOR_HW_PWM_CARRIER_FREQ_HZ)

void MusicalMotorHw_initGPIO(void);
void MusicalMotorHw_initEPWM(void);
void MusicalMotorHw_initCPUTimer0(void);
void MusicalMotorHw_initCPUTimer1(void);

void MusicalMotorHw_enableGateDriver(void);
__interrupt void MusicalMotorHw_heartbeatISR(void);
void MusicalMotorHw_setDebug1(uint16_t value);

void MusicalMotorHw_writeTonePwm(float32_t tc, float32_t ta, float32_t tb);
void MusicalMotorHw_writeSilentPwm(void);

#endif // MUSICAL_MOTOR_HW_H
