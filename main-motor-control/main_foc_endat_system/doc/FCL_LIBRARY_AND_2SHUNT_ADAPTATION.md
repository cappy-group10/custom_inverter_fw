# FCL Library and 2-Shunt Adaptation Notes

**Primary scope:** `main_foc_endat_system`  
**Key files:** `fcl_cpu_code_dm.c`, `fcl_cla_code_dm.cla`, `dual_axis_servo_drive_user.h`, `dual_axis_servo_drive_user.c`, `dual_axis_servo_drive_hal.c`  
**Last reviewed:** 2026-04-07

---

## 1. Current Checked-In Build Snapshot

| Setting | Value | Meaning |
|---|---|---|
| `BUILDLEVEL` | `FCL_LEVEL4` | Speed loop + current loop are active |
| `POSITION_ENCODER` | `ENDAT_POS_ENCODER` | FCL uses EnDat-published angle feedback |
| `COUNT_CURRENT_SENSORS` | `TWO_CURRENT_SENSORS` | Two measured phase currents |
| `COUNT_CURRENT_PROTECTION_CMPSS` | `2U` | Hardware OCP only on phases V and W |
| `DISABLE_ENDAT` | not defined | EnDat runtime is enabled |
| `DISABLE_MOTOR_FAULTS` | defined | Main runtime bypasses the trip-protection path |

Important takeaway:

- This is no longer only an open-loop adaptation note.
- The checked-in project is already running with the 2-shunt configuration in the `FCL_LEVEL4` control path.

---

## 2. FCL Library Overview in This Project

The TI Fast Current Loop (FCL) library still owns the inner current loop. The project adapts the surrounding hardware, feedback, and state-machine plumbing to fit:

- a single active motor path,
- EnDat position feedback,
- and two measured phase currents instead of three.

### Core FCL-related files

| File | Role |
|---|---|
| `sources/fcl_cpu_code_dm.c` | CPU-side FCL path: current sampling, transforms, d-axis PI, inverse Park, SVGEN, PWM update |
| `sources/fcl_cla_code_dm.cla` | CLA-side FCL tasks, including the PWM-edge position handoff |
| `include/fcl/fcl_cpu_cla_dm.h` | `MOTOR_Vars_t`, `FCL_Parameters_t`, FCL defaults |
| `include/fcl/cpu_cla_shared_dm.h` | Shared CLA/CPU FCL state |
| `include/sensored_foc/fcl_enum.h` | State and build-level enums |

No prebuilt FCL `.lib` is used here. The FCL sources are compiled directly into the CCS project.

---

## 3. What the 2-Shunt Adaptation Looks Like Today

### 3.1 Current-sense wiring and ADC ownership

| Signal | ADC module | Channel | Pin | CMPSS |
|---|---|---|---|---|
| Phase V current (`Iv`) | `ADCC` | `ADCIN3` | `J3-24` | `CMPSS6` |
| Phase W current (`Iw`) | `ADCB` | `ADCIN3` | `J3-25` | `CMPSS3` |
| Phase U current (`Iu`) | not measured directly | — | — | no hardware comparator |
| DC bus voltage | `ADCD` | `ADCIN15` | `J7-63` | — |

In 2-shunt mode, phase U current is reconstructed implicitly:

```text
Iu = -(Iv + Iw)
```

### 3.2 FCL binding

`initMotorParameters()` does the key FCL wiring:

- sets `pMotor->FCL_params.adcScale = -M1_FCL_ADC_SCALE`
- calls `FCL_initADC_2I()` instead of `FCL_initADC_3I()`
- binds `curA` to phase V and `curB` to phase W
- keeps the legacy QEP-shaped FCL container populated only with the fields still needed by the EnDat path

### 3.3 Clarke transform implication

The checked-in FCL path still uses the standard two-input Clarke form internally, so only `curA` and `curB` are consumed by the fast loop.

In this project:

- `curA` = measured phase V current
- `curB` = measured phase W current

That is mathematically valid for a two-sensor drive as long as the phase mapping stays self-consistent through:

- ADC wiring,
- PWM phase assignment,
- angle conventions,
- and controller sign conventions.

---

## 4. Hardware and HAL Changes That Matter

The checked-in HAL work for the 2-shunt path now includes:

- phase-U ADC setup guarded out under `IS_TWO_SHUNT_DRIVE`
- `cmpssHandle[0] = CMPSS6` for phase V
- `cmpssHandle[1] = CMPSS3` for phase W
- `cmpssHandle[2] = 0` unused
- TRIP4 rebuilt from:
  - `INPUTXBAR1` / GPIO24 gate-driver fault
  - `CMPSS3` filtered trip
  - `CMPSS6` filtered trip
- loops over comparator handles bounded by `COUNT_CURRENT_PROTECTION_CMPSS`

That means the code no longer assumes three current-protection comparators even though the original TI reference did.

---

## 5. Offset Calibration Path

The checked-in current-offset calibration path in `dual_axis_servo_drive_user.c` already respects the 2-shunt build:

- phase-U offset accumulation is skipped
- phase-V and phase-W offsets are still measured
- only the active PPB offsets are written back

So the current-sense offset bring-up path is already aligned with the two-sensor hardware.

---

## 6. Current Risks and Caveats

These are the main issues that still matter when using the checked-in 2-shunt path.

### 6.1 Current polarity must stay verified

`initMotorParameters()` still assigns:

```c
pMotor->FCL_params.adcScale = -M1_FCL_ADC_SCALE;
```

That keeps the legacy TI sign convention. If the physical current sensors produce the opposite polarity from what the FCL expects, the loop can behave like positive feedback.

Practical check:

- command a small known torque,
- inspect `clarke.As`, `clarke.Bs`, `pi_id.fbk`, and `pi_iq.fbk`,
- confirm the signs line up with the intended motor rotation and current commands.

### 6.2 `M1_CMPSS_ZERO_COUNT` deserves explicit review

The checked-in code currently uses:

```c
#define M1_CMPSS_ZERO_COUNT 2048U
```

That is a midscale comparator zero point. At the same time, the surrounding comments describe a current front end with an approximately `2.5 V` zero on a `3.0 V` ADC reference.

If the hardware really is centered near `2.5 V`, then the CMPSS thresholds should be reviewed carefully before relying on hardware over-current protection. The doc reflects the code as checked in, but the code/comments pair is a real calibration caveat.

### 6.3 Phase U still has no hardware OCP

Only phases V and W have measured currents and CMPSS coverage. Phase U has:

- no direct ADC current channel
- no direct hardware comparator trip path

Possible mitigation:

- add a software check on reconstructed `Iu`
- rely on gate-driver fault reporting if available
- keep operating currents conservative until protection strategy is finalized

### 6.4 Fault handling is currently disabled from `main()`

`DISABLE_MOTOR_FAULTS` is defined in the current build, so the checked-in firmware is convenient for bench work but not configured as a final inverter-safety build.

---

## 7. Current Scaling Summary

These values come from the checked-in headers:

| Parameter | Current checked-in value |
|---|---|
| `M1_ADC_REFERENCE_VOLTAGE` | `3.0 V` |
| `M1_CURRENT_SENSE_SENSITIVITY` | `0.09 V/A` |
| `M1_CURRENT_COUNTS_PER_AMP` | `122.88 counts/A` |
| `M1_BASE_CURRENT` | `5.0 A` |
| `M1_FCL_ADC_SCALE` | `1 / (5.0 * 122.88)` |
| `M1_MAXIMUM_CURRENT` | `5.0 A` |
| `M1_CMPSS_ZERO_COUNT` | `2048` |

The checked-in comments still warn that the positive ADC headroom is limited, so keep that constraint in mind when raising current commands.

---

## 8. Recommended Validation Before Higher-Power Testing

1. Verify current polarity with a small commanded torque and confirm the FCL feedback signs are correct.
2. Re-check phase mapping from `EPWM1/2/3` to physical U/V/W and from ADC channels to the measured phases.
3. Revisit CMPSS zero-point and threshold calibration before re-enabling motor fault protection.
4. Consider adding a software over-current check on reconstructed phase U current.
5. Remove `DISABLE_MOTOR_FAULTS` only after the above checks are complete.

---

## 9. Bottom Line

The important status change versus older notes is this:

- the 2-shunt adaptation is no longer just an experimental open-loop branch,
- it is part of the checked-in `FCL_LEVEL4` EnDat control path,
- but the safety and calibration edges around comparator thresholds and protection policy still need deliberate bench validation.
