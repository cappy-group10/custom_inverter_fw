# Dual-Axis Servo Drive Control Algorithm - Architecture & Execution Map

**Primary file:** `dual_axis_servo_drive.c`  
**Supporting files:** `dual_axis_servo_drive_user.c`, `dual_axis_servo_drive.h`, `dual_axis_servo_drive_user.h`, `dual_axis_servo_drive_settings.h`, `dual_axis_servo_drive_hal.h`, `fcl_cla_code_dm.cla`  
**Target:** TI F2837x (C2000) - `main_foc_endat_system`  
**Last reviewed:** 2026

---

## 1. Big Picture

`main()` is the orchestration point for the servo-drive application. It does not implement the control laws directly; instead it:

1. initializes the device, HAL, motor data, and protection,
2. calibrates current sensing,
3. brings up the EnDat encoder pipeline,
4. enables the interrupt-driven runtime,
5. then stays in a foreground loop that services slower background tasks and synchronizes command variables.

At runtime, the control algorithm is split across several timing domains:

| Timing domain | Typical rate in this build | Main responsibility |
|---|---:|---|
| `EPWM1` + CLA Task 1 + `motor1ControlISR()` | `10 kHz` | Fast current-loop execution, position/speed feedback update, build-level logic |
| `EPWM9` EnDat producer ISR | `40 kHz` | Independent EnDat position acquisition and publish |
| CPU Timer 0 A-branch | `50 us` base event | Motor run/stop and fault/gate logic (`A1`) plus spare slots |
| CPU Timer 1 B-branch | `100 us` base event | SFRA background communications in level 6 |
| CPU Timer 2 C-branch | `150 us` base event | LEDs and spare slots |
| Foreground `for(;;)` loop | as fast as possible | Dispatch A/B/C framework and synchronize user/system references |

The control structure is layered:

- Fast current control is handled by the FCL path on each PWM interrupt.
- Speed control is a slower outer loop that runs every `speedLoopPrescaler` ISR cycles.
- Position control exists only in build level 5 and wraps the speed loop.
- EnDat feedback is produced independently, then consumed synchronously at the PWM edge.

Although the project name says "dual axis", the current `main()` path initializes and runs only `MTR_1`.

---

## 2. Current Configuration Snapshot

These settings define the active behavior of the checked-in build:

| Setting | Value | Meaning |
|---|---|---|
| `BUILDLEVEL` | `FCL_LEVEL4` | Speed loop enabled on top of current loop |
| `SAMPLING_METHOD` | `SINGLE_SAMPLING` | One ISR/current-loop execution per PWM period |
| `FCL_CNTLR` | `PI_CNTLR` | PI-based fast current loop |
| `POSITION_ENCODER` | `ENDAT_POS_ENCODER` | Absolute position feedback from EnDat |
| `M1_PWM_FREQUENCY` | `10 kHz` | Inverter PWM frequency |
| `M1_ISR_FREQUENCY` | `10 kHz` | `EPWM1` control ISR rate in single sampling |
| `ENDAT_PRODUCER_RATE_RATIO` | `4` | EnDat runtime producer runs at `4x` PWM |
| `speedLoopPrescaler` | `10` | Speed loop runs at `1 kHz` in this build |

Important compile-time notes for the present bench configuration:

- `ENDAT_HACK` is defined, so `main()` does not wait on `enableFlag`.
- `DISABLE_MOTOR_FAULTS` is defined, so fault-protection configuration and trip handling are bypassed in the current test build.

---

## 3. Initialization Sequence From `main()`

This is the effective control-oriented startup order:

```text
main()
|
+-[1]  Device_init()
|
+-[2]  HAL_init()
+-[3]  HAL_MTR_init(MTR_1)
|
+-[4]  Disable TBCLKSYNC
|       \- freeze PWM time bases during configuration
|
+-[5]  HAL_setParams()
+-[6]  HAL_setMotorParams(MTR_1)
|
+-[7]  Enable TBCLKSYNC
|
+-[8]  initMotorParameters()
|       \- bind FCL object, motor constants, scaling, PWM/ADC/QEP hooks
|
+-[9]  initControlVars()
|       \- initialize PI/PID controllers and default references
|
+-[10] Set runtime limits / defaults
|       +- current limit override
|       +- alignment and startup references
|       \- datalog configuration
|
+-[11] resetControlVars()
+-[12] HAL_clearTZFlag()
|
+-[13] [Level 6 only] configureSFRA()
|
+-[14] Initialize background task pointers A/B/C
|
+-[15] Wait for FCL library version check
|
+-[16] HAL_setupInterrupts()
|       +- register `motor1ControlISR()` on `EPWM1`
|       \- register EnDat producer ISR on `EPWM9`
|
+-[17] runOffsetsCalculation()
|       \- calibrate current ADC offsets before enabling control
|
+-[18] EnDat bring-up
|       +- `EnDat_Init()`
|       +- `endat21_runCommandSet()`
|       +- `[EnDat 2.2] endat22_setupAddlData()`
|       +- `EnDat_initDelayComp()`
|       +- `PM_endat22_setFreq()`
|       +- `endat21_initProducer(polePairs)`
|       \- `endat21_startProducer()`
|
+-[19] HAL_enableInterrupts()
|
+-[20] Force gate disabled, then enable global interrupts
|       +- `GPIO_writePin(..., 1)` disables the driver gate
|       +- `EINT`
|       \- `ERTM`
|
\-[21] Forever:
        +- run A/B/C background task framework
        \- run `runSyncControl()`
```

### Why this order matters

- PWM clocks stay frozen during setup so no partial PWM configuration reaches the inverter.
- Current-sense offsets are calibrated before the fast current loop uses ADC data.
- EnDat runtime acquisition is started before normal control begins, so valid position can be published before the drive is commanded to run.
- The driver gate remains disabled until the runtime state machine explicitly moves the motor into `CTRL_RUN`.

---

## 4. Runtime Scheduling Model

### 4.1 Foreground/background framework

After initialization, `main()` does not perform control math directly. Its infinite loop does two things:

1. dispatches the A/B/C background task framework through `Alpha_State_Ptr`, and
2. calls `runSyncControl()` to copy external/global commands into `motorVars[0]`.

The A/B/C scheduler is a round-robin state machine:

```text
A0 -> B0 -> C0 -> A0 -> ...
```

Each branch is armed by a different CPU timer:

| Branch | Timer base period | Implemented tasks |
|---|---:|---|
| A branch | `50 us` | `A1` = `runMotorControl()`, `A2/A3` spare, LED toggle in `A3` |
| B branch | `100 us` | `B1` = SFRA background comms in level 6, `B2/B3` spare |
| C branch | `150 us` | `C1` = LED toggle, `C2/C3` spare |

Each branch rotates among three task slots. For example, the A branch receives a `50 us` event, but `A1`, `A2`, and `A3` execute on successive A-branch opportunities.

### 4.2 Fast control path

The main control loop is interrupt-driven:

```text
EPWM1 event
|
+- CLA Task 1
|   \- latch PWM-synchronous position into FCL state
|
\- motor1ControlISR()
    +- mirror latest published EnDat sample into CPU-side motorVars
    +- execute build-level-specific control logic
    +- update datalog / optional DAC outputs
    \- acknowledge PWM/ADC/CLA interrupts
```

### 4.3 EnDat runtime path

The encoder runtime is intentionally decoupled from the PWM ISR:

```text
EPWM9 @ 40 kHz
    -> endatProducerISR()
       -> non-blocking EnDat producer tick
          -> publish latest valid sample into shared double buffer

EPWM1 edge
    -> Cla1Task1()
       -> read the active published EnDat slot
       -> copy mech/electrical angle into `fclVars[0].qep`
       -> set `pangle` for FCL use

EPWM1 ISR
    -> updateMotorPositionFeedback()
       -> mirror the same published sample into `motorVars[0]`
       -> update CPU-visible position/speed variables and diagnostics
```

This split keeps EnDat I/O asynchronous while preserving PWM-synchronous angle consumption by the FCL.

---

## 5. Control-State Layers

Two state machines are important when reading the control logic.

### 5.1 Drive state (`ctrlState` / `runMotor`)

The foreground path writes command variables into `motorVars[0]` through `runSyncControl()`. The background A-task then applies them in `runMotorControl()`:

```text
Global refs (`ctrlState`, `speedRef`, `IdRef`, `IqRef`)
    -> runSyncControl()
    -> motorVars[0].ctrlState / speedRef / IdRef_run / IqRef
    -> runMotorControl()
    -> motorVars[0].runMotor + gate enable/disable
```

`runMotorControl()` is responsible for:

- honoring `CTRL_RUN` versus stop states,
- enabling or disabling the gate driver,
- handling trip flags and reset behavior,
- forcing `runMotor = MOTOR_STOP` on faults.

### 5.2 Encoder/FCL state (`lsw`)

The fast loop uses the encoder/FCL local state machine:

| State | Meaning | Typical behavior |
|---|---|---|
| `ENC_ALIGNMENT` | Rotor alignment phase | Apply d-axis current and hold angle at zero |
| `ENC_WAIT_FOR_INDEX` | Incremental encoder only | Spin slowly until index is found |
| `ENC_CALIBRATION_DONE` | Closed-loop-ready state | Use real position feedback for closed control |

For the current EnDat build, `getPostAlignmentEncoderState()` returns `ENC_CALIBRATION_DONE`, so the `ENC_WAIT_FOR_INDEX` state is effectively skipped after alignment. That path remains in the code because the file still supports generic QEP-based build levels.

---

## 6. Build-Level Guide

The file uses incremental build levels to stage functionality from open-loop bring-up to full cascaded control.

| Build level | Primary goal | Closed loops | Main algorithm summary |
|---|---|---|---|
| `FCL_LEVEL1` | Verify PWM generation and SVPWM path | none | Ramp control generates an angle, inverse Park creates alpha/beta voltage commands, SVGEN writes duty cycles directly |
| `FCL_LEVEL2` | Verify ADC/current sensing and transforms | none | Adds current sampling, Clarke/Park transforms, encoder/speed measurement, but still drives voltage commands open-loop |
| `FCL_LEVEL3` | Verify d/q current regulation | current loop | FCL closes the fast current loop; CPU logic still mainly supervises alignment and provides fixed `IqRef` / `IdRef` references |
| `FCL_LEVEL4` | Verify speed regulation | speed + current loops | Speed PID becomes the outer loop; its output becomes `Iq` reference for the FCL current loop |
| `FCL_LEVEL5` | Verify position regulation | position + speed + current loops | Position PI generates a speed command, speed PID generates `Iq`, and FCL closes the current loop |
| `FCL_LEVEL6` | Verify bandwidth with SFRA | same as level 4 | Reuses the level 4 speed/current structure and injects SFRA perturbations into d-axis, q-axis, or speed loop |

### Big-picture loop nesting by build level

```text
Level 1:
    angle generator -> inverse Park -> SVPWM

Level 2:
    angle generator + current feedback verification

Level 3:
    Iq/Id references -> fast current loop (FCL)

Level 4:
    speed reference -> speed PID -> Iq reference -> fast current loop

Level 5:
    position reference -> position PI -> speed PID -> Iq reference -> fast current loop

Level 6:
    level 4 structure + SFRA injection/collection
```

---

## 7. Current Build Control Flow (`FCL_LEVEL4`, EnDat, Single Sampling)

This is the most relevant execution path for the checked-in project.

### 7.1 ISR-level flow

```text
motor1ControlISR()
|
+-[1] updateMotorPositionFeedback()
|       \- copy latest published EnDat sample into `motorVars[0]`
|
+-[2] buildLevel46_M1()
|       |
|       +- run FCL current-controller math and wrapper
|       +- measure DC bus voltage
|       +- execute alignment / post-alignment supervisor logic
|       +- run speed estimator from electrical angle
|       +- every 10th ISR, run speed PID
|       +- set `pi_iq.ref` from speed PID output
|       \- ramp `IdRef` into `pi_id.ref`
|
+-[3] write DLOG / optional DAC channels
|
+-[4] acknowledge interrupt group(s)
|
\-[5] increment `isrTicker`
```

### 7.2 Reference and feedback chain

```text
User/global speed command
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
    -> CLA Task 1 PWM-edge latch
    -> `fclVars[0].qep` / `pangle`
    -> speed estimator + FCL angle input
```

### 7.3 Alignment and transition logic

At level 4, the CPU-side supervisor around the FCL behaves as follows:

- If `runMotor == MOTOR_STOP`, the code resets back to `ENC_ALIGNMENT`, clears the controller state, and commands zero speed/current.
- During `ENC_ALIGNMENT`, the code applies `IdRef_start` and waits for the alignment counter to expire.
- After alignment, the EnDat build transitions directly to `ENC_CALIBRATION_DONE`.
- In `ENC_CALIBRATION_DONE`, `IdRef` becomes `IdRef_run`, the speed reference becomes active, and the speed PID closes the outer loop.

Because `speedLoopPrescaler = 10`, the speed PID executes once every 10 current-loop passes:

- Current loop: `10 kHz`
- Speed loop: `1 kHz`

That is the key loop-separation used in the current project.

---

## 8. Support Functions That Matter To The Algorithm

| Function | Role in overall control behavior |
|---|---|
| `initMotorParameters()` | Binds hardware, scaling, motor constants, and FCL data structures |
| `initControlVars()` | Initializes PI/PID gains, references, and ramp generator state |
| `resetControlVars()` | Resets FCL/current-loop state when stopping or restarting |
| `runOffsetsCalculation()` | Calibrates ADC current offsets before control is enabled |
| `runMotorControl()` | Applies run/stop/fault state and controls gate-driver enable |
| `runSyncControl()` | Copies external/global commands into the active motor control structure |
| `updateMotorPositionFeedback()` | Mirrors the latest valid EnDat sample into CPU-side variables |
| `ramper()` | Slew-limits `IdRef` and position reference changes |
| `refPosGen()` | Generates the local stepped position command sequence for level 5 |

---

## 9. Notes And Constraints

### Present build is effectively single-axis

The file and object naming still reflect the TI dual-axis example, but `main()` only initializes one motor object and one ISR path in this project.

### Fault handling is currently bench-oriented

Because `DISABLE_MOTOR_FAULTS` is defined in `dual_axis_servo_drive_user.h`, the checked-in build bypasses the normal trip-protection setup and recovery path. That is fine for bench EnDat bring-up, but it should be revisited before inverter-powered testing.

### Enable gating is bypassed in the current test build

Because `ENDAT_HACK` is defined, the startup wait on `enableFlag` is skipped. The runtime still uses `ctrlState` and `runMotor` to decide whether to enable the gate, but the explicit wait-for-enable step in `main()` is disabled.

### Absolute and incremental encoders share one framework

The alignment and `lsw` logic is generic. With EnDat, post-alignment goes straight to `ENC_CALIBRATION_DONE`; with QEP, the same framework would insert `ENC_WAIT_FOR_INDEX`.

---

## 10. Quick Reading Guide For Future Changes

When changing the control algorithm, these are the fastest places to inspect:

1. `dual_axis_servo_drive_settings.h` for build-level and feedback configuration.
2. `dual_axis_servo_drive_user.c` for controller gains, scaling, and loop prescalers.
3. `dual_axis_servo_drive.c` for startup order, background state machine, and build-level supervisor logic.
4. `fcl_cla_code_dm.cla` for PWM-synchronous position handoff and CLA-side fast-loop behavior.
5. `ENDAT_ARCHITECTURE.md` and `HAL_ARCHITECTURE.md` for the lower-level encoder and hardware execution maps.
