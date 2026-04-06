//#############################################################################
//
// FILE:   musical_motor_tone.c
//
// TITLE:  Musical Motor tone generation, melody sequencing, and manual-tone
//         queue for UART-driven playback.
//
// DESCRIPTION:
//   Two playback modes:
//
//   SONG mode  — The cpuTimer0 ISR (1 kHz) walks through a MusicalMotorMelody
//                array, loading each note's frequency and duration.
//
//   MANUAL mode — The host pushes short-duration tones via UART.  Each tone
//                 is enqueued into a 4-entry ring buffer.  The cpuTimer0 ISR
//                 dequeues and plays them sequentially.  When the queue
//                 empties the motor falls silent (user released the key).
//
//   In both modes the epwm1 ISR (20 kHz) runs the phase-ramp → inverse-Park
//   → SVPWM pipeline to produce the three-phase output.
//
//#############################################################################

#include "device.h"
#include "driverlib.h"

#include "ipark.h"
#include "rampgen.h"
#include "svgen.h"

#include "musical_motor_hw.h"
#include "musical_motor_tone.h"

// ═══════════════════════════════════════════════════════════════════════════
//  SVPWM pipeline state
// ═══════════════════════════════════════════════════════════════════════════
static RAMPGEN rampGen = RAMPGEN_DEFAULTS;
static IPARK   iPark   = IPARK_DEFAULTS;
static SVGEN   svGen   = SVGEN_DEFAULTS;

// ═══════════════════════════════════════════════════════════════════════════
//  Playback state  (all accessed by the cpuTimer0 ISR at 1 kHz)
// ═══════════════════════════════════════════════════════════════════════════
static volatile uint16_t toneMode  = TONE_MODE_SONG;
static volatile uint16_t toneState = TONE_STATE_IDLE;

static const MusicalMotorMelody *activeMelody = (const MusicalMotorMelody *)0;

static volatile float32_t toneAmplitude      = 0.0f;
static volatile float32_t currentFreqHz      = 0.0f;
static volatile uint16_t  toneActive         = 0U;
static volatile uint16_t  noteIndex          = 0U;
static volatile uint32_t  noteTicksRemaining = 0U;

// Free-running counter incremented in cpuTimer0 ISR (1 kHz → ms resolution)
static volatile uint32_t isrTicker = 0U;

// ═══════════════════════════════════════════════════════════════════════════
//  Manual-tone ring buffer  (SPSC: main-loop writes head, ISR reads tail)
// ═══════════════════════════════════════════════════════════════════════════
static ToneQueueEntry toneQueue[TONE_QUEUE_CAPACITY];
static volatile uint16_t toneQueueHead = 0U;   // next write slot  (main loop)
static volatile uint16_t toneQueueTail = 0U;   // next read slot   (ISR)

static inline uint16_t toneQueueCount(void)
{
    // Safe: both head and tail are uint16_t (atomic read on C28x)
    uint16_t h = toneQueueHead;
    uint16_t t = toneQueueTail;
    return (h >= t) ? (h - t) : (TONE_QUEUE_CAPACITY - t + h);
}

static inline bool toneQueueFull(void)
{
    return (((toneQueueHead + 1U) % TONE_QUEUE_CAPACITY) == toneQueueTail);
}

static inline bool toneQueueEmpty(void)
{
    return (toneQueueHead == toneQueueTail);
}

// Called from ISR context only
static inline bool toneQueueDequeue(ToneQueueEntry *out)
{
    if(toneQueueHead == toneQueueTail)
    {
        return false;
    }

    *out = toneQueue[toneQueueTail];
    toneQueueTail = (toneQueueTail + 1U) % TONE_QUEUE_CAPACITY;
    return true;
}

static inline void toneQueueFlush(void)
{
    toneQueueHead = 0U;
    toneQueueTail = 0U;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

static void setNote(float32_t freqHz)
{
    currentFreqHz = freqHz;

    if(freqHz > 0.0f)
    {
        rampGen.Freq = freqHz;
        toneActive = 1U;
    }
    else
    {
        rampGen.Freq = 0.0f;
        toneActive = 0U;
    }
}

static void loadCurrentMelodyNote(void)
{
    setNote(activeMelody->notes[noteIndex].freqHz);
    noteTicksRemaining = activeMelody->notes[noteIndex].durationMs;
}

static void toneUpdate(void)
{
    float32_t sinVal;
    float32_t cosVal;

    fclRampGen(&rampGen);

    sinVal = __sinpuf32(rampGen.Out);
    cosVal = __cospuf32(rampGen.Out);

    iPark.Ds     = 0.0f;
    iPark.Qs     = toneAmplitude;
    iPark.Sine   = sinVal;
    iPark.Cosine = cosVal;
    runIPark(&iPark);

    svGen.Ualpha = iPark.Alpha;
    svGen.Ubeta  = iPark.Beta;
    runSVGenDQ(&svGen);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — initialisation
// ═══════════════════════════════════════════════════════════════════════════

void MusicalMotorTone_init(const MusicalMotorMelody *melody,
                           float32_t amplitudePu)
{
    activeMelody  = melody;
    toneAmplitude = amplitudePu;
    toneActive    = 0U;
    toneState     = TONE_STATE_IDLE;
    toneMode      = TONE_MODE_SONG;
    noteIndex     = 0U;
    noteTicksRemaining = 0U;
    isrTicker     = 0U;

    toneQueueFlush();

    rampGen.Freq         = 0.0f;
    rampGen.StepAngleMax = MUSICAL_MOTOR_HW_ISR_PERIOD_S;
    rampGen.Angle        = 0.0f;
    rampGen.Gain         = 1.0f;
    rampGen.Out          = 0.0f;
    rampGen.Offset       = 0.0f;

    if((activeMelody != (const MusicalMotorMelody *)0) &&
       (activeMelody->length > 0U))
    {
        loadCurrentMelodyNote();
        toneState = TONE_STATE_PLAYING;
    }
    else
    {
        setNote(0.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — playback control
// ═══════════════════════════════════════════════════════════════════════════

void MusicalMotorTone_playSong(const MusicalMotorMelody *melody,
                               float32_t amplitudePu)
{
    toneQueueFlush();

    activeMelody  = melody;
    toneAmplitude = amplitudePu;
    toneMode      = TONE_MODE_SONG;
    noteIndex     = 0U;

    if((activeMelody != (const MusicalMotorMelody *)0) &&
       (activeMelody->length > 0U))
    {
        loadCurrentMelodyNote();
        toneState = TONE_STATE_PLAYING;
    }
    else
    {
        setNote(0.0f);
        toneState = TONE_STATE_IDLE;
    }
}

bool MusicalMotorTone_enqueueTone(float32_t freqHz,
                                  float32_t amplitudePu,
                                  uint16_t  durationMs)
{
    if(toneQueueFull())
    {
        return false;
    }

    //
    // Switch to manual mode on first enqueue (discard any active song)
    //
    if(toneMode != TONE_MODE_MANUAL)
    {
        activeMelody = (const MusicalMotorMelody *)0;
        toneMode     = TONE_MODE_MANUAL;
    }

    //
    // Enqueue the tone
    //
    uint16_t slot = toneQueueHead;
    toneQueue[slot].freqHz    = freqHz;
    toneQueue[slot].amplitude = amplitudePu;
    toneQueue[slot].durationMs = durationMs;
    toneQueueHead = (slot + 1U) % TONE_QUEUE_CAPACITY;

    //
    // If we were idle, mark as playing so the ISR starts consuming
    //
    if(toneState != TONE_STATE_PLAYING)
    {
        toneState = TONE_STATE_PLAYING;
    }

    return true;
}

void MusicalMotorTone_setAmplitude(float32_t amplitudePu)
{
    toneAmplitude = amplitudePu;
}

void MusicalMotorTone_stop(void)
{
    toneQueueFlush();
    activeMelody = (const MusicalMotorMelody *)0;
    noteTicksRemaining = 0U;
    setNote(0.0f);
    toneState = TONE_STATE_IDLE;
}

void MusicalMotorTone_pause(void)
{
    if(toneState == TONE_STATE_PLAYING)
    {
        toneActive = 0U;
        toneState  = TONE_STATE_PAUSED;
    }
}

void MusicalMotorTone_resume(void)
{
    if(toneState == TONE_STATE_PAUSED)
    {
        //
        // Restore toneActive based on whether we had a non-zero frequency
        //
        if(currentFreqHz > 0.0f)
        {
            rampGen.Freq = currentFreqHz;
            toneActive = 1U;
        }
        toneState = TONE_STATE_PLAYING;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public API — status getters
// ═══════════════════════════════════════════════════════════════════════════

uint16_t MusicalMotorTone_getState(void)
{
    return toneState;
}

uint16_t MusicalMotorTone_getMode(void)
{
    return toneMode;
}

float32_t MusicalMotorTone_getCurrentFreq(void)
{
    return currentFreqHz;
}

float32_t MusicalMotorTone_getAmplitude(void)
{
    return toneAmplitude;
}

uint16_t MusicalMotorTone_getNoteIndex(void)
{
    return noteIndex;
}

uint16_t MusicalMotorTone_getNoteTotal(void)
{
    if(activeMelody != (const MusicalMotorMelody *)0)
    {
        return activeMelody->length;
    }
    return 0U;
}

uint32_t MusicalMotorTone_getIsrTicker(void)
{
    return isrTicker;
}

// ═══════════════════════════════════════════════════════════════════════════
//  ISR — EPWM1 (20 kHz)  PWM waveform generation
// ═══════════════════════════════════════════════════════════════════════════

__interrupt void MusicalMotorTone_epwm1ISR(void)
{
    MusicalMotorHw_setDebug1(1U);

    if(toneActive != 0U)
    {
        toneUpdate();
        MusicalMotorHw_writeTonePwm(svGen.Tc, svGen.Ta, svGen.Tb);
    }
    else
    {
        MusicalMotorHw_writeSilentPwm();
    }

    MusicalMotorHw_setDebug1(0U);

    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

// ═══════════════════════════════════════════════════════════════════════════
//  ISR — cpuTimer0 (1 kHz)  Note sequencing / queue consumption
// ═══════════════════════════════════════════════════════════════════════════

__interrupt void MusicalMotorTone_cpuTimer0ISR(void)
{
    isrTicker++;

    if(toneState != TONE_STATE_PLAYING)
    {
        Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
        return;
    }

    // -----------------------------------------------------------------
    //  SONG mode — walk through the melody array
    // -----------------------------------------------------------------
    if(toneMode == TONE_MODE_SONG)
    {
        if((activeMelody != (const MusicalMotorMelody *)0) &&
           (activeMelody->length > 0U))
        {
            if(noteTicksRemaining > 0U)
            {
                noteTicksRemaining--;
            }

            if(noteTicksRemaining == 0U)
            {
                noteIndex++;
                if(noteIndex >= activeMelody->length)
                {
                    noteIndex = 0U;
                }
                loadCurrentMelodyNote();
            }
        }
    }
    // -----------------------------------------------------------------
    //  MANUAL mode — consume from the tone queue
    // -----------------------------------------------------------------
    else
    {
        if(noteTicksRemaining > 0U)
        {
            noteTicksRemaining--;
        }

        if(noteTicksRemaining == 0U)
        {
            ToneQueueEntry entry;

            if(toneQueueDequeue(&entry))
            {
                toneAmplitude      = entry.amplitude;
                noteTicksRemaining = (uint32_t)entry.durationMs;
                setNote(entry.freqHz);
            }
            else
            {
                //
                // Queue empty — user released the key.  Silence the motor.
                //
                setNote(0.0f);
            }
        }
    }

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}
