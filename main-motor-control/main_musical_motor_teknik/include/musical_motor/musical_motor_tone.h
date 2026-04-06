
//#############################################################################
//
// FILE:   musical_motor_tone.h
//
// TITLE:  Musical Motor tone generation, melody sequencing, and manual-tone
//         queue for UART-driven playback.
//
//#############################################################################

#ifndef MUSICAL_MOTOR_TONE_H
#define MUSICAL_MOTOR_TONE_H

#include "driverlib.h"
#include "musical_motor_songs.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Manual-tone ring buffer
//
//  The host sends Tone CMD frames at a fast rate (e.g. every 50 ms) with a
//  short fixed duration (e.g. 100 ms) while the user holds a key.  The MCU
//  enqueues each tone; the cpuTimer0 ISR dequeues and plays them one by
//  one.  When the queue empties the motor falls silent (key released).
// ═══════════════════════════════════════════════════════════════════════════

#define TONE_QUEUE_CAPACITY     4U      // power-of-2 not required

typedef struct {
    float32_t freqHz;
    float32_t amplitude;        // Vq per-unit
    uint16_t  durationMs;
} ToneQueueEntry;

// ═══════════════════════════════════════════════════════════════════════════
//  Playback mode / state  (uint16_t so they're safe for ISR access on C28x)
// ═══════════════════════════════════════════════════════════════════════════

#define TONE_MODE_SONG      0U
#define TONE_MODE_MANUAL    1U

#define TONE_STATE_IDLE     0U
#define TONE_STATE_PLAYING  1U
#define TONE_STATE_PAUSED   2U

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — initialisation
// ═══════════════════════════════════════════════════════════════════════════

//! One-time hardware-level init (ramp generator, SVG defaults).
//! Does NOT start playback — call playSong() or enqueueTone() afterwards.
void MusicalMotorTone_init(const MusicalMotorMelody *melody,
                           float32_t amplitudePu);

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — playback control  (called from main-loop context)
// ═══════════════════════════════════════════════════════════════════════════

//! Load a predefined melody and begin playing it.
//! Switches to SONG mode; any queued manual tones are discarded.
void MusicalMotorTone_playSong(const MusicalMotorMelody *melody,
                               float32_t amplitudePu);

//! Enqueue a single tone for manual playback.
//! Switches to MANUAL mode on the first call (discards any active song).
//! Returns true if the entry was enqueued, false if the queue was full.
bool MusicalMotorTone_enqueueTone(float32_t freqHz,
                                  float32_t amplitudePu,
                                  uint16_t  durationMs);

//! Change the Vq amplitude without affecting the current note or mode.
void MusicalMotorTone_setAmplitude(float32_t amplitudePu);

//! Stop all playback, silence the motor, return to IDLE.
void MusicalMotorTone_stop(void);

//! Pause the current playback (song or manual).
void MusicalMotorTone_pause(void);

//! Resume from a paused state.
void MusicalMotorTone_resume(void);

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — status getters  (safe to call from main loop)
// ═══════════════════════════════════════════════════════════════════════════

uint16_t  MusicalMotorTone_getState(void);       // TONE_STATE_*
uint16_t  MusicalMotorTone_getMode(void);        // TONE_MODE_*
float32_t MusicalMotorTone_getCurrentFreq(void);
float32_t MusicalMotorTone_getAmplitude(void);
uint16_t  MusicalMotorTone_getNoteIndex(void);   // song mode only
uint16_t  MusicalMotorTone_getNoteTotal(void);   // song mode only
uint32_t  MusicalMotorTone_getIsrTicker(void);   // free-running counter

// ═══════════════════════════════════════════════════════════════════════════
//  ISR entry points  (registered in main.c)
// ═══════════════════════════════════════════════════════════════════════════

__interrupt void MusicalMotorTone_epwm1ISR(void);
__interrupt void MusicalMotorTone_cpuTimer0ISR(void);

#endif // MUSICAL_MOTOR_TONE_H
