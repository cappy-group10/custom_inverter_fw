#ifndef MUSICAL_MOTOR_SONGS_H
#define MUSICAL_MOTOR_SONGS_H

#include "driverlib.h"

typedef struct
{
    float32_t freqHz;
    uint32_t durationMs;
} MusicalMotorNoteEntry;

typedef struct
{
    const MusicalMotorNoteEntry *notes;
    uint16_t length;
} MusicalMotorMelody;

typedef enum
{
    MUSICAL_MOTOR_SONG_MARIO = 0,
    MUSICAL_MOTOR_SONG_MEGALOVANIA = 1,
    MUSICAL_MOTOR_SONG_JINGLE_BELLS = 2
} MusicalMotorSongId;

const MusicalMotorMelody *MusicalMotorSongs_getById(MusicalMotorSongId songId);

#endif // MUSICAL_MOTOR_SONGS_H
