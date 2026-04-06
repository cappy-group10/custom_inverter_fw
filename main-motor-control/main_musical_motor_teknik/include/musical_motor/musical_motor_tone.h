

#ifndef MUSICAL_MOTOR_TONE_H
#define MUSICAL_MOTOR_TONE_H

#include "driverlib.h"

#include "musical_motor_songs.h"

void MusicalMotorTone_init(const MusicalMotorMelody *melody,
                           float32_t amplitudePu);
void MusicalMotorTone_setAmplitude(float32_t amplitudePu);

__interrupt void MusicalMotorTone_epwm1ISR(void);
__interrupt void MusicalMotorTone_cpuTimer0ISR(void);

#endif // MUSICAL_MOTOR_TONE_H
