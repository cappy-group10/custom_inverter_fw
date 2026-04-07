# EnDat Position Offset Calibration

**Primary files:** `dual_axis_servo_drive.c`, `endat_ops.c`, `endat_shared.h`, `uart_link.c`  
**Target:** TMS320F28379D / `main_foc_endat_system`  
**Last reviewed:** 2026-04-07

---

## 1. Current Checked-In Behavior

The checked-in project is currently built as `FCL_LEVEL4`, not `FCL_LEVEL3`.

That matters because:

- the runtime calibration state machine is only compiled in `FCL_LEVEL3`,
- while the checked-in `FCL_LEVEL4` build applies a saved default offset during startup.

Today, the active boot path is:

```text
main()
    -> endat21_readPosition()                  // publish one valid raw sample
    -> endat21_setPositionOffset(0.677F)       // because ENDAT_APPLY_DEFAULT_OFFSET is defined
    -> normal runtime producer starts
```

So the checked-in firmware already performs offset correction, but it does so from a stored value rather than a live calibration run.

---

## 2. Where the Offset Is Applied

The runtime offset lives in:

```text
gEndatRuntimeState.rawPositionOffsetPu
gEndatRuntimeState.offsetValid
```

Every published sample flows through `endatDecodePositionSample()` in `endat_ops.c`, which:

1. reconstructs the raw position,
2. converts it to mechanical angle in per-unit,
3. subtracts `rawPositionOffsetPu` when `offsetValid != 0`,
4. then applies direction inversion if needed.

That means the saved or calibrated offset is part of the normal EnDat publish path, not a one-off correction after the fact.

---

## 3. Current Startup Offset Path

The checked-in startup behavior is:

| Setting | Value |
|---|---|
| `ENDAT_APPLY_DEFAULT_OFFSET` | defined |
| `ENDAT_POSITION_OFFSET_PU` | `0.677F` |
| Current build level | `FCL_LEVEL4` |

Practical implication:

- if the mechanical installation still matches the calibration that produced `0.677F`, the current build is ready to use that stored value on every boot,
- if the coupling or zero position changed, this saved value should be updated or temporarily replaced by the runtime calibration flow described below.

---

## 4. Optional Runtime Calibration Path (`FCL_LEVEL3` Only)

The source still contains a runtime EnDat calibration state machine in `dual_axis_servo_drive.c`, but it is compiled only when:

```text
BUILDLEVEL == FCL_LEVEL3
```

That path:

- locks the rotor with alignment current,
- collects raw EnDat samples,
- computes a circular mean of the mechanical angle,
- writes the result through `endat21_setPositionOffset()`,
- then stops the motor cleanly.

### State machine summary

| State | Description |
|---|---|
| `ENDAT_CAL_IDLE` | Waiting for a calibration request |
| `ENDAT_CAL_SETTLING` | Holding alignment current so the rotor settles |
| `ENDAT_CAL_SAMPLING` | Collecting raw position samples |
| `ENDAT_CAL_DONE` | Offset computed and applied successfully |
| `ENDAT_CAL_ABORTED` | Calibration aborted because of a fault, stall, or cancel condition |

### Timing constants in the current source

| Constant | Value | Meaning |
|---|---:|---|
| `ENDAT_CALIBRATION_SETTLE_TICKS` | `1000` | About `100 ms` at `10 kHz` ISR rate |
| `ENDAT_CALIBRATION_SAMPLE_COUNT` | `256` | Number of samples used in the circular mean |
| `ENDAT_CALIBRATION_STALL_TICKS` | `100` | Abort threshold if fresh EnDat samples stop arriving |

---

## 5. How the Runtime Calibration Works

When `BUILDLEVEL == FCL_LEVEL3`, the state machine runs inside `buildLevel3_M1()`.

High-level flow:

```text
CTRL_CALIBRATE request
    -> force ENC_ALIGNMENT
    -> apply d-axis alignment current
    -> wait for settling
    -> collect fresh raw EnDat samples
    -> circular-average the mechanical angle
    -> endat21_setPositionOffset(meanOffsetPu)
    -> finalize, reset controller, force CTRL_STOP
```

The circular mean is used so the result is robust across the `0.0 -> 1.0` wrap boundary.

---

## 6. Host / Debug Interface

### UART command

The host command frame still carries `ctrlState`, and `CTRL_CALIBRATE` is still the calibration request value.

### Important build-level detail

`runSyncControl()` only applies the special one-shot latch behavior for `CTRL_CALIBRATE` when:

```text
BUILDLEVEL == FCL_LEVEL3
```

In other words:

- in the checked-in `FCL_LEVEL4` build, `CTRL_CALIBRATE` is not the supported operator path,
- the supported offset path in the checked-in build is the saved startup offset.

### Debugger use

If you temporarily switch the project to `FCL_LEVEL3`, you can still trigger calibration from CCS by writing:

```text
ctrlState = CTRL_CALIBRATE
```

and then watching `gEndatCalibration.state`.

---

## 7. Activation Requirements for a Live Recalibration Run

To use the runtime calibration path intentionally:

1. Set `BUILDLEVEL = FCL_LEVEL3`.
2. Keep EnDat enabled.
3. Rebuild and flash.
4. Command `CTRL_CALIBRATE`.
5. After a successful run, copy the resulting offset into `ENDAT_POSITION_OFFSET_PU` if you want the boot-time default to use it later.

If you stay on the checked-in `FCL_LEVEL4` build, step 4 is not enough by itself because the dedicated calibration state machine is not compiled into that image.

---

## 8. Failure and Finalization Behavior

The runtime calibration path aborts or finishes when:

- the control state leaves `CTRL_CALIBRATE`,
- a trip/fault occurs,
- EnDat is not initialized,
- fresh samples stop arriving,
- or sample collection completes successfully.

On both success and failure, the code:

- recomputes corrected angles if possible,
- primes the speed differentiator,
- resets the FCL controller,
- forces `CTRL_STOP`.

That behavior is intentional so a calibration run does not leave the controller half-armed.

---

## 9. Useful API Points

| Function | Role |
|---|---|
| `endat21_setPositionOffset()` | Store a raw-position offset and mark it valid |
| `endat21_clearPositionOffset()` | Remove the saved offset |
| `endat21_getPositionOffset()` | Read back the saved offset and validity flag |
| `UART_Link_sendStatus()` | Exposes `rawPositionOffsetPu` on the status frame |

---

## 10. Bottom Line

There are really two offset stories in this project now:

1. **Current checked-in behavior:** boot with a saved offset (`0.677F`) in the normal `FCL_LEVEL4` runtime.
2. **Optional live recalibration path:** switch temporarily to `FCL_LEVEL3`, run `CTRL_CALIBRATE`, then save the new result back into the default offset if desired.

That distinction is the main thing older notes tended to blur, and it is the key detail to keep in mind when working on this project now.
