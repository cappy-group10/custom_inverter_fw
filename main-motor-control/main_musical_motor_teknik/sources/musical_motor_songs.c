//#############################################################################
//
// FILE:   musical_motor_songs.c
//
// TITLE:  Musical Motor song tables
//
//#############################################################################

#include "musical_motor_songs.h"

#define NOTE_REST   0.0f

// Musical note frequencies in Hz (A4 = 440 Hz tuning)
#define NOTE_A3     220.0f
#define NOTE_Ab4    415.0f
#define NOTE_A4     440.0f
#define NOTE_Ab5    831.0f
#define NOTE_A5     880.0f

#define NOTE_Bb3    233.0f
#define NOTE_B3     247.0f
#define NOTE_Bb4    466.0f
#define NOTE_B4     494.0f
#define NOTE_Bb5    932.0f
#define NOTE_B5     988.0f

#define NOTE_C4     262.0f
#define NOTE_C5     523.0f
#define NOTE_C6     1047.0f

#define NOTE_D3     147.0f
#define NOTE_D4     294.0f
#define NOTE_D5     587.0f

#define NOTE_E3     165.0f
#define NOTE_E4     330.0f
#define NOTE_E5     659.0f

#define NOTE_F3     175.0f
#define NOTE_F4     349.0f
#define NOTE_Fs4    370.0f
#define NOTE_F5     698.0f
#define NOTE_Fs5    740.0f

#define NOTE_G3     196.0f
#define NOTE_G4     392.0f
#define NOTE_G5     784.0f

// Note duration definitions in ms
#define W   1200U
#define H   600U
#define Q   300U
#define Q_D 450U
#define E   150U
#define S   75U

// Helper macro
#define ARRAY_LEN(x) ((uint16_t)(sizeof(x) / sizeof((x)[0])))


// Define melodies here.
//

static const MusicalMotorNoteEntry melodyMario[] = {
    { NOTE_E5,  E }, { NOTE_E5,  E }, { NOTE_REST, E }, { NOTE_E5,  E },
    { NOTE_REST, E }, { NOTE_C5, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_G5,  Q }, { NOTE_REST, Q },
    { NOTE_G4,  Q }, { NOTE_REST, Q },

    { NOTE_C5,  Q_D }, { NOTE_G4, E }, { NOTE_REST, Q },
    { NOTE_E4,  Q_D }, { NOTE_A4, E }, { NOTE_REST, E },
    { NOTE_B4,  E }, { NOTE_REST, E }, { NOTE_Bb4, E }, { NOTE_A4,  E },
    { NOTE_REST, E },

    { NOTE_G4,  E }, { NOTE_E5,  E }, { NOTE_G5,  E },
    { NOTE_A5,  E }, { NOTE_REST, E }, { NOTE_F5,  E }, { NOTE_G5,  E },
    { NOTE_REST, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_C5,  E }, { NOTE_D5,  E }, { NOTE_B4,  Q_D },

    { NOTE_C5,  Q_D }, { NOTE_G4, E }, { NOTE_REST, Q },
    { NOTE_E4,  Q_D }, { NOTE_A4, E }, { NOTE_REST, E },
    { NOTE_B4,  E }, { NOTE_REST, E }, { NOTE_Bb4, E }, { NOTE_A4,  E },
    { NOTE_REST, E },

    { NOTE_G4,  E }, { NOTE_E5,  E }, { NOTE_G5,  E },
    { NOTE_A5,  E }, { NOTE_REST, E }, { NOTE_F5,  E }, { NOTE_G5,  E },
    { NOTE_REST, E }, { NOTE_E5,  E }, { NOTE_REST, E },
    { NOTE_C5,  E }, { NOTE_D5,  E }, { NOTE_B4,  Q_D },

    { NOTE_REST, H },
};


static const MusicalMotorNoteEntry melodyMegalovania[] = {
    { NOTE_D4,  S }, { NOTE_D4,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    { NOTE_C4,  S }, { NOTE_C4,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    { NOTE_B3,  S }, { NOTE_B3,  S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    { NOTE_Bb3, S }, { NOTE_Bb3, S }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  Q_D }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  E }, { NOTE_D4,  E }, { NOTE_F4,  S }, { NOTE_G4,  S },

    { NOTE_D4,  E }, { NOTE_D4,  E }, { NOTE_D5,  E }, { NOTE_REST, S },
    { NOTE_A4,  E }, { NOTE_REST, S },
    { NOTE_Ab4, E }, { NOTE_G4,  E },
    { NOTE_F4,  Q },

    { NOTE_D4,  S }, { NOTE_F4,  S }, { NOTE_G4,  E },
    { NOTE_D4,  S }, { NOTE_F4,  S }, { NOTE_G4,  E },
    { NOTE_E4,  S }, { NOTE_G4,  S }, { NOTE_A4,  E },
    { NOTE_A4,  E }, { NOTE_A4,  E }, { NOTE_G4,  E },

    { NOTE_REST, H },
};

static const MusicalMotorNoteEntry melodyJingleBells[] = {
    // Jingle bells, jingle bells, jingle all the way
    { NOTE_E4,  Q }, { NOTE_E4,  Q }, { NOTE_E4,  H },
    { NOTE_E4,  Q }, { NOTE_E4,  Q }, { NOTE_E4,  H },
    { NOTE_E4,  Q }, { NOTE_G4,  Q }, { NOTE_C4,  Q_D }, { NOTE_D4,  E },
    { NOTE_E4,  W },

    // Oh what fun it is to ride in a one-horse open sleigh
    { NOTE_F4,  Q }, { NOTE_F4,  Q }, { NOTE_F4,  Q }, { NOTE_F4,  E },
    { NOTE_F4,  E }, { NOTE_E4,  Q }, { NOTE_E4,  Q }, { NOTE_E4,  E }, { NOTE_E4,  E },
    { NOTE_E4,  Q }, { NOTE_D4,  Q }, { NOTE_D4,  Q }, { NOTE_E4,  Q },
    { NOTE_D4,  H }, { NOTE_G4,  H },

    // Jingle bells, jingle bells, jingle all the way
    { NOTE_E4,  Q }, { NOTE_E4,  Q }, { NOTE_E4,  H },
    { NOTE_E4,  Q }, { NOTE_E4,  Q }, { NOTE_E4,  H },
    { NOTE_E4,  Q }, { NOTE_G4,  Q }, { NOTE_C4,  Q_D }, { NOTE_D4,  E },
    { NOTE_E4,  W },

    // Oh what fun it is to ride in a one-horse open sleigh
    { NOTE_F4,  Q }, { NOTE_F4,  Q }, { NOTE_F4,  Q }, { NOTE_F4,  Q },
    { NOTE_F4,  E }, { NOTE_E4,  Q }, { NOTE_E4,  E }, { NOTE_E4,  E }, { NOTE_E4,  E },
    { NOTE_G4,  Q }, { NOTE_G4,  Q }, { NOTE_F4,  Q }, { NOTE_D4,  Q },
    { NOTE_C4,  W },

    { NOTE_REST, H },
};


// External Facing API to define melodies as MusicalMotorMelody structs
// 
static const MusicalMotorMelody marioMelody = {
    melodyMario,
    ARRAY_LEN(melodyMario)
};

static const MusicalMotorMelody megalovaniaMelody = {
    melodyMegalovania,
    ARRAY_LEN(melodyMegalovania)
};

static const MusicalMotorMelody jingleBellsMelody = {
    melodyJingleBells,
    ARRAY_LEN(melodyJingleBells)
};

// External Facing API to retrieve melodies by ID
//
const MusicalMotorMelody *MusicalMotorSongs_getById(MusicalMotorSongId songId)
{
    switch(songId)
    {
        case MUSICAL_MOTOR_SONG_MEGALOVANIA:
            return &megalovaniaMelody;

        case MUSICAL_MOTOR_SONG_JINGLE_BELLS:
            return &jingleBellsMelody;

        case MUSICAL_MOTOR_SONG_MARIO:
        default:
            return &marioMelody;
    }
}

// #undef ARRAY_LEN
// #undef NOTE_REST
// #undef NOTE_D3
// #undef NOTE_E3
// #undef NOTE_F3
// #undef NOTE_G3
// #undef NOTE_A3
// #undef NOTE_Bb3
// #undef NOTE_B3
// #undef NOTE_C4
// #undef NOTE_D4
// #undef NOTE_E4
// #undef NOTE_F4
// #undef NOTE_Fs4
// #undef NOTE_G4
// #undef NOTE_Ab4
// #undef NOTE_A4
// #undef NOTE_Bb4
// #undef NOTE_B4
// #undef NOTE_C5
// #undef NOTE_D5
// #undef NOTE_E5
// #undef NOTE_F5
// #undef NOTE_Fs5
// #undef NOTE_G5
// #undef NOTE_Ab5
// #undef NOTE_A5
// #undef NOTE_Bb5
// #undef NOTE_B5
// #undef NOTE_C6
// #undef W
// #undef H
// #undef Q
// #undef Q_D
// #undef E
// #undef S
