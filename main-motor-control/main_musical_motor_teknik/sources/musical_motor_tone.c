//#############################################################################
//
// FILE:   musical_motor_tone.c
//
// TITLE:  Musical Motor tone generation and melody sequencing
//
//#############################################################################

#include "device.h"
#include "driverlib.h"

#include "ipark.h"
#include "rampgen.h"
#include "svgen.h"

#include "musical_motor_hw.h"
#include "musical_motor_tone.h"

static RAMPGEN rampGen = RAMPGEN_DEFAULTS;
static IPARK iPark = IPARK_DEFAULTS;
static SVGEN svGen = SVGEN_DEFAULTS;

static const MusicalMotorMelody *activeMelody = (const MusicalMotorMelody *)0;

static float32_t toneAmplitude = 0.0f;
static uint16_t toneActive = 0U;

static uint16_t noteIndex = 0U;
static uint32_t noteTicksRemaining = 0U;

static void MusicalMotorTone_setNote(float32_t freqHz);
static void MusicalMotorTone_loadCurrentNote(void);
static void MusicalMotorTone_update(void);

void MusicalMotorTone_init(const MusicalMotorMelody *melody,
                           float32_t amplitudePu)
{
    activeMelody = melody;
    toneAmplitude = amplitudePu;
    toneActive = 0U;
    noteIndex = 0U;
    noteTicksRemaining = 0U;

    rampGen.Freq = 0.0f;
    rampGen.StepAngleMax = MUSICAL_MOTOR_HW_ISR_PERIOD_S;
    rampGen.Angle = 0.0f;
    rampGen.Gain = 1.0f;
    rampGen.Out = 0.0f;
    rampGen.Offset = 0.0f;

    if((activeMelody != (const MusicalMotorMelody *)0) &&
       (activeMelody->length > 0U))
    {
        MusicalMotorTone_loadCurrentNote();
    }
    else
    {
        MusicalMotorTone_setNote(0.0f);
    }
}

void MusicalMotorTone_setAmplitude(float32_t amplitudePu)
{
    toneAmplitude = amplitudePu;
}

__interrupt void MusicalMotorTone_epwm1ISR(void)
{
    MusicalMotorHw_setDebug1(1U);

    if(toneActive != 0U)
    {
        MusicalMotorTone_update();
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

__interrupt void MusicalMotorTone_cpuTimer0ISR(void)
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

            MusicalMotorTone_loadCurrentNote();
        }
    }

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

static void MusicalMotorTone_setNote(float32_t freqHz)
{
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

static void MusicalMotorTone_loadCurrentNote(void)
{
    MusicalMotorTone_setNote(activeMelody->notes[noteIndex].freqHz);
    noteTicksRemaining = activeMelody->notes[noteIndex].durationMs;
}

static void MusicalMotorTone_update(void)
{
    float32_t sinVal;
    float32_t cosVal;

    fclRampGen(&rampGen);

    sinVal = __sinpuf32(rampGen.Out);
    cosVal = __cospuf32(rampGen.Out);

    iPark.Ds = 0.0f;
    iPark.Qs = toneAmplitude;
    iPark.Sine = sinVal;
    iPark.Cosine = cosVal;
    runIPark(&iPark);

    svGen.Ualpha = iPark.Alpha;
    svGen.Ubeta = iPark.Beta;
    runSVGenDQ(&svGen);
}
