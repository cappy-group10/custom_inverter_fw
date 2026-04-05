# EnDat Position Offset Calibration - Architecture & Interface

**Primary file:** `dual_axis_servo_drive.c`  
**Supporting files:** `endat_ops.c`, `endat_shared.h`, `fcl_enum.h`, `uart_link.c`, `commands.py`  
**Target:** TMS320F28379D (C2000 F2837x)  
**Build level:** `FCL_LEVEL3` (current-loop with manual Id/Iq control)  
**Last reviewed:** 2026

---

## 1. Purpose

The offset calibration routine measures and removes the mechanical zero-point
error of the EnDat absolute encoder.  When a motor is first installed (or the
encoder coupling shifts), the raw position reported by the encoder does not
correspond to the true mechanical zero of the stator/rotor alignment.

The calibration holds the rotor stationary with a known d-axis alignment
current, collects a burst of raw encoder readings, and computes the mean
angular offset using circular averaging.  That offset is then stored in
`gEndatRuntimeState.rawPositionOffsetPu` and subtracted from every subsequent
position sample, so downstream control code (speed loop, position loop) sees a
corrected angle.

---

## 2. State Machine

The calibration is driven by `runEndatCalibrationStateMachine()`, a static
inline function called once per PWM ISR tick inside `buildLevel3_M1()`.

```text
              ctrlState = CTRL_CALIBRATE
              runMotor  = MOTOR_RUN
                      |
                      v
               +-----------+
               |   IDLE    |
               +-----------+
                      |
      resetEndatCalibrationContext(SETTLING)
                      |
                      v
               +-----------+    settleTicks >= 1000
               | SETTLING  | -------------------------+
               +-----------+                          |
                      |                               v
                      |                      +-----------+
                      |                      | SAMPLING  |---+
                      |                      +-----------+   |
                      |                            |         |
                      |             sampleCount >= 256       |
                      |                            |    stall >= 100 ticks
                      |                            v         | (no fresh sample)
                      |                      +-----------+   |
                      |                      |   DONE    |   |
                      |                      +-----------+   |
                      |                                      v
                      |                              +-----------+
                      +---(abort on fault/stall)---->| ABORTED   |
                                                     +-----------+
```

### States

| State | Value | Description |
|---|:---:|---|
| `ENDAT_CAL_IDLE` | 0 | Inactive, waiting for `CTRL_CALIBRATE` command |
| `ENDAT_CAL_SETTLING` | 1 | Alignment current applied, waiting for rotor to settle (1000 ISR ticks) |
| `ENDAT_CAL_SAMPLING` | 2 | Collecting 256 raw encoder samples for circular mean |
| `ENDAT_CAL_DONE` | 3 | Offset computed and applied, terminal success state |
| `ENDAT_CAL_ABORTED` | 4 | Calibration aborted (fault, stall, or user cancel), terminal failure state |

### Timing constants

| Constant | Value | Meaning |
|---|---|---|
| `ENDAT_CALIBRATION_SETTLE_TICKS` | 1000 | ISR ticks to wait for alignment current to stabilize (~100 ms at 10 kHz) |
| `ENDAT_CALIBRATION_SAMPLE_COUNT` | 256 | Number of raw encoder samples to average |
| `ENDAT_CALIBRATION_STALL_TICKS` | 100 | Consecutive ISR ticks in SAMPLING without a fresh EnDat sample before abort |

---

## 3. Detailed Flow

### 3.1 Entry: IDLE to SETTLING

When `pMotor->ctrlState == CTRL_CALIBRATE`, `runMotor == MOTOR_RUN`, and the
state is IDLE (or DONE/ABORTED from a previous run), `resetEndatCalibrationContext()`
initializes all accumulators and transitions to `ENDAT_CAL_SETTLING`.

During SETTLING the ISR alignment logic in `buildLevel3_M1()` forces:

```c
motorVars[0].alignCntr = 0;
motorVars[0].ptrFCL->lsw = ENC_ALIGNMENT;
motorVars[0].IdRef = motorVars[0].IdRef_start;
```

This applies a d-axis alignment current (`IdRef_start`) with zero q-axis
current, locking the rotor in a known electrical position.  The speed ramp
target and Iq reference are both zeroed:

```c
ptrFCL->pi_iq.ref = (lsw == ENC_ALIGNMENT) ? 0 : IqRef;
rc.TargetValue = 0;
```

Stall detection is **not active** during SETTLING.  The system only needs to
wait for the alignment current to reach steady state; fresh encoder samples are
not required.

### 3.2 SETTLING to SAMPLING

After `ENDAT_CALIBRATION_SETTLE_TICKS` (1000) ISR ticks, the state transitions
to `ENDAT_CAL_SAMPLING`.  The sample accumulators (`sumSin`, `sumCos`,
`sampleCount`) and the stall counter are reset.

### 3.3 SAMPLING

On each ISR tick with a fresh encoder sample (`endatSampleCounter` changed
since last capture):

1. Compute `rawMechThetaPu = wrapThetaPu(endatPosRaw * rawPositionScalePu)`
2. Accumulate `sumSin += __sinpuf32(rawMechThetaPu)` and
   `sumCos += __cospuf32(rawMechThetaPu)`
3. Increment `sampleCount`

If no fresh sample arrives for `ENDAT_CALIBRATION_STALL_TICKS` (100)
consecutive ISR ticks, the calibration aborts.  This guards against encoder
communication failure during active sampling.

### 3.4 Offset computation

When `sampleCount` reaches `ENDAT_CALIBRATION_SAMPLE_COUNT` (256):

```c
meanOffsetPu = atan2f(sumSin, sumCos) / (2.0F * PI);
meanOffsetPu = wrapThetaPu(meanOffsetPu);
endat21_setPositionOffset(meanOffsetPu);
```

The circular mean avoids the 0/1 wraparound problem that a naive arithmetic
average would suffer from.  The result is stored via `endat21_setPositionOffset()`:

```c
gEndatRuntimeState.rawPositionOffsetPu = endatWrapThetaPu(rawOffsetPu);
gEndatRuntimeState.offsetValid = 1U;
```

### 3.5 Finalization

`finalizeEndatCalibration()` is called on both success (`ENDAT_CAL_DONE`) and
failure (`ENDAT_CAL_ABORTED`).  It:

1. Computes corrected angles from the current raw position (if offset is now valid)
2. Primes the speed differentiator with the corrected angle to avoid a startup spike
3. Resets the FCL controller
4. Sets `pMotor->ctrlState = CTRL_STOP` and `ctrlState = CTRL_STOP`

---

## 4. Command Interface

### 4.1 UART (host to MCU)

The host sends a 16-byte motor command frame:

```
[0xAA] [0x01] [ctrlState:1] [speedRef:4] [idRef:4] [iqRef:4] [checksum:1]
```

Setting `ctrlState = 5` (`CTRL_CALIBRATE`) triggers calibration.
`applyMotorCmd()` in `uart_link.c` writes the global `ctrlState` variable.

### 4.2 Python side

```python
from commands import CtrlState, MotorCommand

cmd = MotorCommand(ctrl_state=CtrlState.CALIBRATE,
                   speed_ref=0.0, id_ref=0.0, iq_ref=0.0)
```

### 4.3 runSyncControl() latch logic

Because `runSyncControl()` runs on every main-loop iteration (much faster than
UART frames arrive), a latch mechanism prevents repeated CTRL_CALIBRATE frames
from re-triggering calibration while one is already in progress:

```text
ctrlState != CALIBRATE        -> clear latch, pass ctrlState through
ctrlState == CALIBRATE, unlatch or already running -> set latch, apply CALIBRATE
ctrlState == CALIBRATE, latched, motor not CALIBRATE -> force STOP (prevent re-trigger)
```

This means: to restart calibration after completion, the host must first send a
non-CALIBRATE state (e.g., `CTRL_STOP`) to clear the latch, then send
`CTRL_CALIBRATE` again.

### 4.4 CCS / debugger

Write `ctrlState = 5` in the Expressions window.  The same `runSyncControl()`
path propagates it to `motorVars[0].ctrlState`.  The calibration state can be
observed via `gEndatCalibration.state` (0-4).

---

## 5. How the Offset is Applied at Runtime

Once `offsetValid == 1`, every position sample published by the EnDat producer
is corrected in `computeCorrectedEndatAngles()`:

```c
mechTheta = wrapThetaPu(rawPosition * rawPositionScalePu);
mechTheta = wrapThetaPu(mechTheta - rawOffsetPu);          // subtract offset
```

The corrected mechanical angle is then multiplied by `PolePairs` to produce the
electrical angle.  Direction inversion (`positionDirection`) is also applied.

This correction runs in the ISR path and adds approximately 1 call to
`wrapThetaPu()` (a `floorf` + conditional add) when offset is valid.

---

## 6. Abort Conditions

| Condition | Where checked | Result |
|---|---|---|
| `pMotor->ctrlState != CTRL_CALIBRATE` | Guard at top of SM | Abort if in SETTLING or SAMPLING |
| `tripFlagDMC != 0` (overcurrent fault) | Guard after ctrlState check | Abort unconditionally |
| `endatInitDone == 0` (encoder not ready) | Guard after ctrlState check | Abort unconditionally |
| Stall: no fresh EnDat sample for 100 ticks | Inside SAMPLING case only | Abort (encoder comm failure) |
| `rawPositionScalePu <= 0` | Inside SAMPLING on fresh sample | Abort (invalid encoder config) |

---

## 7. ISR Cycle Cost

The calibration state machine is designed to add minimal overhead to the 10 kHz
PWM ISR:

| Operation | When | Approximate cost |
|---|---|---|
| `freshSample` comparison | Every ISR tick during cal | ~1 cycle |
| `settleTicks++` and compare | SETTLING phase | ~2 cycles |
| `__sinpuf32` / `__cospuf32` | SAMPLING, fresh sample only | ~1-2 cycles each (TMU) |
| `wrapThetaPu` (`floorf`) | SAMPLING, fresh sample only | ~10-30 cycles |
| `atan2f` + division | Once at completion | ~50-200 cycles (software) |

The `atan2f` call could be replaced with the TMU intrinsic `__atan2puf32()`
which returns directly in per-unit and avoids the `/ (2*PI)` division, but
since it fires only once per calibration run, the cost is negligible.

---

## 8. Key Source Locations

| Item | File | Line(s) |
|---|---|---|
| State enum and context struct | `dual_axis_servo_drive.c` | 236-257 |
| Timing constants | `dual_axis_servo_drive.c` | 232-234 |
| `resetEndatCalibrationContext()` | `dual_axis_servo_drive.c` | 718-727 |
| `computeCorrectedEndatAngles()` | `dual_axis_servo_drive.c` | 729-769 |
| `finalizeEndatCalibration()` | `dual_axis_servo_drive.c` | 772-799 |
| `runEndatCalibrationStateMachine()` | `dual_axis_servo_drive.c` | 802-916 |
| ISR alignment logic for CALIBRATE | `dual_axis_servo_drive.c` | 1284-1289 |
| `runSyncControl()` latch logic | `dual_axis_servo_drive.c` | 2156-2174 |
| `endat21_setPositionOffset()` | `endat_ops.c` | 480-484 |
| `endat21_clearPositionOffset()` | `endat_ops.c` | 486-490 |
| `endat21_getPositionOffset()` | `endat_ops.c` | 492-503 |
| `CtrlState.CALIBRATE` (Python) | `commands.py` | 15 |
| UART frame parsing | `uart_link.c` | 309-315 |
