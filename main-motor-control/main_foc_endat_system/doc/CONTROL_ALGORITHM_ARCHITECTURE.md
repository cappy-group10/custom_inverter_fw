# `main_foc_endat_system` Control Algorithm Architecture

**Primary file:** `dual_axis_servo_drive.c`  
**Supporting files:** `dual_axis_servo_drive_user.c`, `dual_axis_servo_drive_settings.h`, `dual_axis_servo_drive_hal.c`, `fcl_cla_code_dm.cla`, `endat/*.c`  
**Target:** TI F28379D / `main_foc_endat_system`  
**Last reviewed:** 2026-04-07

---

## 1. Big Picture

`main()` is the orchestration layer for the checked-in FOC + EnDat build. It does not implement the control law directly. Instead it:

1. initializes the device, HAL, motor parameters, and runtime defaults,
2. calibrates current offsets,
3. brings up the EnDat stack and publishes an initial valid position sample,
4. enables the interrupt-driven runtime,
5. then stays in a foreground loop that services the A/B/C background framework and synchronizes host/debug commands.

At runtime, the control work is split across several timing domains:

| Timing domain | Rate in current build | Main responsibility |
|---|---:|---|
| `EPWM1` + `Cla1Task1()` + `motor1ControlISR()` | `10 kHz` | Fast current loop, PWM-synchronous angle handoff, speed-loop supervision |
| `EPWM9` + `endatProducerISR()` | `30 kHz` | Independent EnDat acquisition and publish |
| CPU Timer 0 / A branch | `50 us` base event | `runMotorControl()` plus spare A tasks |
| CPU Timer 1 / B branch | `100 us` base event | SFRA background work in level 6, otherwise mostly spare |
| CPU Timer 2 / C branch | `150 us` base event | LEDs, UART status cadence, spare C tasks |
| Foreground `for(;;)` loop | best effort | A/B/C dispatch plus `runSyncControl()` |

Although the naming still comes from TI's dual-axis example, the checked-in project initializes and runs only `MTR_1`.

---

## 2. Current Checked-In Build Snapshot

These values are the ones currently compiled in the repository on 2026-04-07:

| Setting | Value | Meaning |
|---|---|---|
| `BUILDLEVEL` | `FCL_LEVEL4` | Speed loop on top of the fast current loop |
| `SAMPLING_METHOD` | `SINGLE_SAMPLING` | One control ISR per PWM period |
| `FCL_CNTLR` | `PI_CNTLR` | PI-based FCL path |
| `POSITION_ENCODER` | `ENDAT_POS_ENCODER` | Position feedback comes from EnDat |
| `ENCODER_TYPE` | `21` | EnDat 2.1 runtime path is active |
| `M1_PWM_FREQUENCY` | `10 kHz` | Inverter carrier frequency |
| `M1_ISR_FREQUENCY` | `10 kHz` | `EPWM1` ISR rate in this build |
| `speedLoopPrescaler` | `10` | Speed PID runs at `1 kHz` |
| `ENDAT_PRODUCER_RATE_RATIO` | `3` | EnDat producer runs at `30 kHz` |
| `ENDAT_RUNTIME_FREQ_DIVIDER` | `6` | EnDat runtime clock is about `8.33 MHz` at `200 MHz` SYSCLK |
| `ENDAT_POSITION_OFFSET_PU` | `0.677F` | Saved default raw-position offset |

Important compile-time flags in the current bench configuration:

- `ENDAT_HACK` is defined, so `main()` does not block waiting for `enableFlag`.
- `DISABLE_MOTOR_FAULTS` is defined, so trip-protection logic is bypassed from `main()`.
- `ENDAT_APPLY_DEFAULT_OFFSET` is defined, so the saved EnDat position offset is applied during startup.
- `DACOUT_EN` is defined, so the EPWM7/8 debug DAC mux is enabled.

---

## 3. Initialization Sequence From `main()`

This is the effective checked-in startup order:

```text
main()
|
+-[1]  Device_init()
|
+-[2]  HAL_init()
+-[3]  HAL_MTR_init(MTR_1)
|
+-[4]  Disable TBCLKSYNC
|
+-[5]  HAL_setParams()
+-[6]  HAL_setMotorParams(MTR_1)
|
+-[7]  Enable TBCLKSYNC
|
+-[8]  initMotorParameters()
+-[9]  initControlVars()
+-[10] [unless DISABLE_MOTOR_FAULTS] HAL_setupMotorFaultProtection()
+-[11] resetControlVars()
+-[12] HAL_clearTZFlag()
|
+-[13] Initialize A/B/C task pointers
+-[14] Optional SFRA setup for level 6
+-[15] Wait for FCL software-version check
|
+-[16] HAL_setupInterrupts()
+-[17] runOffsetsCalculation()
|
+-[18] EnDat bring-up
|       +- EnDat_Init()
|       +- endat21_runCommandSet()
|       +- [only if ENCODER_TYPE == 22] endat22_setupAddlData()
|       +- EnDat_initDelayComp()
|       +- PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER)
|       +- endat21_initProducer(polePairs)
|       +- endat21_setPositionDirection(speedDirection)
|       +- endat21_readPosition()       // publish one valid sample immediately
|       +- [if ENDAT_APPLY_DEFAULT_OFFSET] endat21_setPositionOffset(0.677F)
|       +- endatInitDone = 1
|       \- endat21_startProducer()
|
+-[19] HAL_enableInterrupts()
+-[20] Disable gate driver, init UART, enable global interrupts
|
\-[21] Forever:
        +- dispatch A/B/C background tasks
        \- runSyncControl()
```

Why this order matters:

- Current-offset calibration happens before the fast current loop trusts ADC data.
- One blocking EnDat read is forced before interrupts start so the control side already has a valid published sample.
- The saved position offset is applied before normal runtime consumption begins.
- The gate driver stays off until the state machine moves into a run-capable state.

---

## 4. Current Runtime Scheduling Model

### 4.1 Foreground and background framework

After initialization, the foreground loop repeatedly does two things:

1. dispatches the `A0 -> B0 -> C0` background framework, and
2. calls `runSyncControl()` to mirror host/debug-owned globals into `motorVars[0]`.

The timers behind the A/B/C framework are configured in `HAL_setParams()`:

| Branch | Timer period | Current use |
|---|---:|---|
| A | `50 us` | `A1 = runMotorControl()` |
| B | `100 us` | `B1 = SFRA background work` in level 6 |
| C | `150 us` | status/LED housekeeping |

### 4.2 Fast control path

The fast control path is ISR-driven:

```text
EPWM1 event
|
+- Cla1Task1()
|   \- latch the latest EnDat-published angle into FCL-compatible fields
|
\- motor1ControlISR()
    +- updateMotorPositionFeedback()
    +- buildLevel46_M1() in the checked-in build
    +- DLOG / optional DAC bookkeeping
    \- interrupt acknowledge + heartbeat update
```

### 4.3 EnDat runtime path

The encoder runtime is intentionally decoupled from the PWM ISR:

```text
EPWM9 @ 30 kHz
    -> endatProducerISR()
       -> endat21_runProducerTick()
          -> schedule or service one non-blocking position read
          -> CRC-check, decode, publish into the double buffer

EPWM1 edge
    -> Cla1Task1()
       -> consume the currently published sample for FCL timing

EPWM1 ISR
    -> updateMotorPositionFeedback()
       -> mirror the same published sample into CPU-visible motor variables
```

That split keeps CRC/decode work off the tight FCL timing edge while preserving a PWM-synchronous angle handoff.

---

## 5. Current Build Control Flow (`FCL_LEVEL4`)

This is the most relevant execution path for the checked-in firmware.

### 5.1 ISR-level flow

```text
motor1ControlISR()
|
+-[1] updateMotorPositionFeedback()
|       \- copy the latest published EnDat sample into `motorVars[0]`
|
+-[2] buildLevel46_M1()
|       |
|       +- run FCL current-controller math
|       +- update DC bus measurement
|       +- handle alignment / post-alignment logic
|       +- run the speed estimator
|       +- every 10th ISR, run the speed PID
|       \- feed `pi_iq.ref` from the speed PID output
|
+-[3] update DLOG / optional DAC channels
|
\-[4] increment `isrTicker` and acknowledge interrupts
```

### 5.2 Reference and feedback chain

```text
Host/debug speed command
    -> runSyncControl()
    -> motorVars[0].speedRef
    -> ramp control (`rc`)
    -> speed PID reference
    -> speed PID output
    -> FCL q-axis current reference (`pi_iq.ref`)
    -> fast current loop
    -> PWM duty update

Id command
    -> IdRef / IdRef_run
    -> ramper()
    -> `pi_id.ref`
    -> fast current loop

EnDat position
    -> producer double buffer
    -> Cla1Task1() PWM-edge latch
    -> `fclVars[0].qep` / `pangle`
    -> speed estimator + FCL angle input
```

### 5.3 Alignment and transition logic

At level 4, the CPU-side supervisor behaves as follows:

- If `runMotor == MOTOR_STOP`, the code resets back toward alignment and clears controller state.
- On the first run transition, `ENC_ALIGNMENT` is collapsed directly into `ENC_CALIBRATION_DONE` for the EnDat build.
- Once `lsw == ENC_CALIBRATION_DONE`, `IdRef_run` and the speed loop become active.
- Because `speedLoopPrescaler = 10`, the speed loop runs at `1 kHz` while the current loop stays at `10 kHz`.

Important contrast with the calibration doc:

- The runtime EnDat offset calibration state machine exists only in `FCL_LEVEL3`.
- The checked-in `FCL_LEVEL4` build relies on the saved default offset applied at startup.

---

## 6. Build-Level Guide

The file still supports TI's incremental build-level model:

| Build level | Primary goal | Closed loops | Notes |
|---|---|---|---|
| `FCL_LEVEL1` | Verify PWM generation and SVPWM path | none | Open-loop angle generation only |
| `FCL_LEVEL2` | Verify ADC/current sensing and transforms | none | Current feedback and transforms visible, still open-loop |
| `FCL_LEVEL3` | Verify d/q current regulation | current loop | Also contains the runtime EnDat offset-calibration state machine |
| `FCL_LEVEL4` | Verify speed regulation | speed + current loops | Current checked-in build |
| `FCL_LEVEL5` | Verify position regulation | position + speed + current loops | Host speed command path changes here |
| `FCL_LEVEL6` | Verify bandwidth with SFRA | speed + current loops | Level 4 structure plus SFRA injection/collection |

---

## 7. Support Functions That Matter Most

| Function | Role |
|---|---|
| `initMotorParameters()` | Binds motor constants, scaling, and FCL data structures |
| `initControlVars()` | Initializes PI/PID gains, ramps, and default references |
| `runOffsetsCalculation()` | Calibrates current-sense offsets before enabling control |
| `runMotorControl()` | Applies run/stop/fault/gate-driver state |
| `runSyncControl()` | Mirrors host/debug-owned globals into `motorVars[0]` |
| `updateMotorPositionFeedback()` | Mirrors the latest published EnDat sample into CPU-side variables |
| `buildLevel46_M1()` | Checked-in fast-loop supervisor for speed + current control |

---

## 8. Notes and Constraints

### Single-axis runtime on top of dual-axis naming

The file, objects, and many helper names still come from TI's dual-axis reference, but only motor 1 is actively initialized and controlled in this project.

### Bench-oriented safety configuration

`DISABLE_MOTOR_FAULTS` is currently defined. That keeps the checked-in build convenient for bench bring-up, but it also means the normal trip-protection path is not active from `main()`.

### `enableFlag` wait is bypassed

`ENDAT_HACK` is defined, so startup does not pause waiting for an external enable handshake.

### Saved EnDat offset is part of the current runtime

The checked-in build applies `ENDAT_POSITION_OFFSET_PU` during boot. If that value is no longer correct for the mechanical installation, update it or switch temporarily to the `FCL_LEVEL3` calibration flow documented in `OFFSET_CALIBRATION.md`.

---

## 9. Quick Reading Guide For Future Changes

When changing the control behavior, start here:

1. `include/boostxl_3phganinv/dual_axis_servo_drive_settings.h`
2. `include/boostxl_3phganinv/dual_axis_servo_drive_user.h`
3. `sources/dual_axis_servo_drive_user.c`
4. `sources/dual_axis_servo_drive.c`
5. `doc/ENDAT_ARCHITECTURE.md`
6. `doc/HAL_ARCHITECTURE.md`
