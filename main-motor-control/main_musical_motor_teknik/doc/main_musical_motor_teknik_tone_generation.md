# Tone Generation in `main_musical_motor_teknik.c`

This document explains how `sources/main_musical_motor_teknik.c` turns note frequencies into audible sound from the motor.

## Big Picture

The firmware does not generate sound by changing the PWM carrier frequency. Instead, it keeps the inverter switching at a fixed high rate and rotates a voltage vector in the motor at the desired audio frequency.

Signal chain in the fast interrupt:

```text
note frequency
-> ramp angle generator
-> sin/cos
-> inverse Park transform
-> space-vector PWM
-> EPWM compare values for the 3 motor phases
```

The rotating stator field makes the motor structure vibrate, and those vibrations become the audible tone.

## The Two Time Bases

There are two independent loops in this file:

1. Fast loop: `epwm1ISR()`
   - Runs once per PWM cycle.
   - Updates the instantaneous three-phase voltages used to create the tone.

2. Slow loop: `cpuTimer0ISR()`
   - Runs every 1 ms.
   - Advances through the melody table and changes the active note.

This separation is important:

- The fast loop creates waveform shape.
- The slow loop decides which note should be playing.

## Important Constants

From the current code:

- `SYSCLK_FREQ = 200 MHz`
- `PWM_CARRIER_FREQ = 20000U`
- `ISR_PERIOD_S = 1 / PWM_CARRIER_FREQ = 50 us`
- `TONE_VQ_DEFAULT = 0.2f`

With up-down counting, the code computes:

```text
PWM_TBPRD = SYSCLK / (2 * PWM_CARRIER_FREQ)
          = 200,000,000 / (2 * 20,000)
          = 5000

PWM_HALF_TBPRD = 2500
```

Note: some comments in the C file still say "10 kHz" and "100 us", but the active macro values currently produce a 20 kHz fast loop and a 50 us ramp step.

## Step 1: Melody Sequencer Chooses a Note

The melody is stored as an array of:

```c
typedef struct {
    float32_t freqHz;
    uint32_t durationMs;
} NoteEntry;
```

Each entry contains:

- `freqHz`: the target musical pitch
- `durationMs`: how long to hold that pitch

`cpuTimer0ISR()` decrements `noteTicksRemaining` once per millisecond. When it reaches zero, the ISR advances `noteIndex`, loads the next note, and calls `toneSetNote()`.

If `freqHz` is `0.0f`, the note is treated as a rest.

## Step 2: `toneSetNote()` Arms or Mutes the Tone Path

`toneSetNote()` does not directly write PWM outputs. It only updates the tone state shared with the fast ISR:

- `toneFreqHz`
- `rampGen.Freq`
- `toneActive`

Behavior:

- `freqHz > 0`: enable tone generation and set the ramp frequency
- `freqHz == 0`: disable tone generation and mark the output silent

One subtle detail: `toneSetNote()` does not reset `rampGen.Angle`. That means note changes are phase-continuous, and after a rest the waveform resumes from the last stored angle instead of restarting at zero.

## Step 3: Ramp Generator Converts Frequency Into Electrical Angle

The fast ISR calls `toneUpdate()`, which starts with:

```c
fclRampGen(&rampGen);
```

The TI ramp block updates angle as:

```text
Angle += StepAngleMax * Freq
```

This file sets:

```text
StepAngleMax = ISR_PERIOD_S
```

so the effective update is:

```text
Angle += ISR_PERIOD_S * toneFreqHz
```

Because angle is stored in per-unit form, `0.0 -> 1.0` represents one full electrical revolution. So:

- `440 Hz` means the angle completes 440 full turns per second
- the fast loop just advances the angle a small amount every 50 us

## Step 4: Per-Unit Angle Becomes Sine and Cosine

After the ramp update, the code computes:

```c
float32_t sinVal = __sinpuf32(rampGen.Out);
float32_t cosVal = __cospuf32(rampGen.Out);
```

These TI intrinsics interpret the input as per-unit angle:

- `0.0` = `0`
- `0.25` = `pi/2`
- `0.5` = `pi`
- `1.0` wraps back to `2*pi`

This creates the rotating sinusoidal reference used for the motor phases.

## Step 5: Inverse Park Builds a Rotating Alpha-Beta Voltage Vector

The code sets:

```c
iPark.Ds = 0.0f;
iPark.Qs = toneAmplitude;
```

Then `runIPark()` computes:

```text
Alpha = Ds * cos(theta) - Qs * sin(theta)
Beta  = Qs * cos(theta) + Ds * sin(theta)
```

Since `Ds = 0`, this simplifies to:

```text
Alpha = -toneAmplitude * sin(theta)
Beta  =  toneAmplitude * cos(theta)
```

So the firmware is creating a constant-magnitude rotating vector in the stationary alpha-beta plane.

What the parameters mean here:

- `toneFreqHz` controls how fast the vector rotates, which sets pitch
- `toneAmplitude` controls vector magnitude, which sets loudness/mechanical excitation

## Step 6: SVPWM Converts the Rotating Vector Into Three Phase Duties

The alpha-beta vector is passed into `runSVGenDQ()`:

```c
svGen.Ualpha = iPark.Alpha;
svGen.Ubeta  = iPark.Beta;
runSVGenDQ(&svGen);
```

The SVPWM block computes three normalized switching functions:

- `Ta`
- `Tb`
- `Tc`

These values represent the commanded phase duties for the inverter.

## Step 7: Duty Commands Are Mapped Into EPWM Compare Registers

`toneWritePWM()` converts each normalized SVPWM output into a hardware compare value:

```text
CMPA = (PWM_HALF_TBPRD * Tphase) + PWM_HALF_TBPRD
```

This maps:

- `-1` -> `0`
- `0` -> `PWM_HALF_TBPRD`
- `+1` -> `PWM_TBPRD`

The current phase mapping is:

- `EPWM1 <- Tc`
- `EPWM2 <- Ta`
- `EPWM3 <- Tb`

That mapping matches the comment in the source and is intentional for this hardware setup.

## What Happens During a Rest

When `toneActive == 0`, `epwm1ISR()` does not run the tone math. Instead it forces all three PWM channels to `PWM_HALF_TBPRD`, which is 50% duty on every phase.

That produces:

- zero line-to-line voltage
- no rotating excitation
- silence from the motor

This is better than disabling PWM outright because it keeps the inverter timing centered and stable.

## Why the Motor Produces Audible Sound

The inverter is still switching at a high carrier rate, but the average three-phase voltage vector is rotating at an audio frequency such as 262 Hz, 440 Hz, or 784 Hz. That rotating field creates alternating electromagnetic forces in the stator and rotor structure. The motor body responds mechanically, and those vibrations radiate as sound.

In other words:

- PWM carrier frequency shapes the switching
- rotating vector frequency sets the musical note

## Practical Tuning Knobs

The main controls for tone behavior are:

- `TONE_VQ_DEFAULT`
  - Raises or lowers loudness by changing vector magnitude
  - Increase carefully to avoid excessive current or torque

- `SONG_SELECT`
  - Selects which melody table is compiled in

- melody note frequency and duration
  - Change pitch and note length

- `PWM_CARRIER_FREQ`
  - Changes the fast-loop update rate and PWM carrier
  - If changed, the ramp math also changes because `ISR_PERIOD_S` depends on it

## Summary

`main_musical_motor_teknik.c` generates tones by:

1. Picking a note frequency from the melody table.
2. Converting that frequency into a continuously advancing electrical angle.
3. Turning the angle into a rotating alpha-beta voltage vector with constant magnitude.
4. Using SVPWM to synthesize three phase duty cycles from that vector.
5. Writing those duty cycles into `EPWM1`, `EPWM2`, and `EPWM3` every fast interrupt.

The result is an open-loop rotating field at audio frequency, which makes the motor act like a speaker.
