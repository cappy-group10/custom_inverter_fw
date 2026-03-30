# FCL Library Usage & Two-Current-Sensor Adaptation Report

**Date:** 2026-03-30
**Branch:** `dev/currentsense`
**Build level at time of review:** `FCL_LEVEL2` (open-loop, ADC/Clarke verification)

---

## 1. FCL Library Overview

The Fast Current Loop (FCL) library is TI's latency-optimized inner current controller for FOC drives on the C2000 (F2837x) platform. It splits work between the CPU and the Control Law Accelerator (CLA) to achieve the tightest possible current-loop bandwidth at 10 kHz PWM.

### 1.1 Source Files

| File | Role |
|------|------|
| `sources/fcl_cpu_code_dm.c` | CPU-side FCL: Clarke, Park, d-axis PI, inverse Park, SVGEN, PWM update |
| `sources/fcl_cla_code_dm.cla` | CLA-side: position latch (Task 1), q-axis PI (Task 2), complex ctrl (Task 3), QEP housekeeping (Task 4) |
| `include/fcl/fcl_cpu_cla_dm.h` | `MOTOR_Vars_t` and `FCL_Parameters_t` structs, default initializers |
| `include/fcl/fcl_foc_cpu_dm.h` | Extended FOC helpers (inverse Park, SVGEN macros) |
| `include/fcl/fcl_pi.h` | `FCL_PIController_t` and `FCL_PI_MACRO` |
| `include/fcl/fcl_cmplx_ctrl.h` | Complex-controller struct (alternative to PI) |
| `include/fcl/cpu_cla_shared_dm.h` | Shared CPU/CLA data (`FCL_Vars_t`, `fclVars[]`) |
| `include/sensored_foc/fcl_enum.h` | Enumerations (`ENC_ALIGNMENT`, `MOTOR_RUN`, build levels, etc.) |

No pre-built `.lib` files -- everything is source-compiled.

### 1.2 Data Structures

```
FCL_Parameters_t  // (per motor)
  carrierMid      // PWM half-period in Q16 (for duty scaling)
  adcScale        // PPB counts -> per-unit current (-1/(Ibase * counts_per_amp))
  cmidsqrt3       // carrierMid * sqrt(3)
  tSamp           // 1 / sampling_freq
  Rd, Rq          // stator resistance (d/q)
  Ld, Lq          // stator inductance (d/q)
  Vdcbus          // DC bus voltage (updated every ISR)
  BemfK           // back-EMF constant
  Ibase, Wbase    // per-unit bases
  wccD, wccQ      // current-controller bandwidth (rad/s)

MOTOR_Vars_t  // (per motor)
  FCL_params      // embedded FCL_Parameters_t
  pi_id           // d-axis PI (runs on CPU)
  ptrFCL->pi_iq   // q-axis PI (runs on CLA Task 2)
  curA_PPBRESULT  // hardware pointer to 1st current ADC PPB result
  curB_PPBRESULT  // hardware pointer to 2nd current ADC PPB result
  curC_PPBRESULT  // hardware pointer to 3rd current ADC PPB result (unused in 2-shunt)
  pwmCompA/B/C    // direct pointers to CMPA registers for phase U/V/W
  AdcIntFlag      // pointer to ADC interrupt flag register

FCL_Vars_t fclVars[2] // (CLA-shared, per motor)
  pangle          // electrical angle for current ISR
  qep.*           // position/encoder state
  pi_iq           // q-axis PI struct (CLA-writable)
  rg              // ramp generator
  lsw             // encoder state machine (alignment, index, calibrated)
  speedWePrev     // previous speed for BEMF feedforward
```

### 1.3 Initialization Sequence (in `initMotorParameters()`)

1. **`FCL_initADC_2I()` or `FCL_initADC_3I()`** -- binds ADC PPB result register addresses to `curA/B/C_PPBRESULT` pointers in the motor struct. In 2-shunt mode, only `curA` (phase V) and `curB` (phase W) are set.

2. **`FCL_initPWM()`** -- disables CMPA shadow mode on all three phase PWMs and stores direct register pointers (`pwmCompA/B/C`) for lowest-latency duty updates.

3. **`FCL_initQEP()`** -- (QEP only) stores QEP register base in `fclVars[].ptrQEP`. Set to NULL for EnDat.

4. **`FCL_resetController()`** -- zeros all PI integrators, carry-over terms, and complex-controller state.

### 1.4 Runtime Execution (10 kHz ISR Path)

```bash
EPWM1 edge
  |
  +-- CLA Task 1: latch encoder position into fclVars[0].pangle
  |
  +-- motor1ControlISR() [CPU, in RAM]
       |
       +-- updateMotorPositionFeedback() -- mirror EnDat sample to motorVars
       |
       +-- buildLevel46_M1() [FCL_LEVEL 3/4/6]
            |
            +-- FCL_runPICtrl_M1()
            |     1. Wait for CLA Task 1 (position ready)
            |     2. Compute sin/cos of electrical angle
            |     3. Wait for ADC EOC
            |     4. Clarke transform (read curA, curB PPB results)
            |     5. Park transform -> d/q current errors
            |     6. Force CLA Task 2 (q-axis PI on CLA)
            |     7. Run d-axis PI on CPU
            |     8. Wait for CLA Task 2 completion
            |     9. Inverse Park -> alpha/beta voltages
            |    10. SVGEN + PWM CMPA update
            |
            +-- FCL_runPICtrlWrap_M1()
            |     - Update PI gains from motor params + Vdcbus
            |     - BEMF feedforward carry-over
            |     - Wait for CLA Task 4, clear CLA flags
            |
            +-- Speed loop (every 10th ISR = 1 kHz)
            +-- Reference management (IdRef, IqRef ramps)
```

### 1.5 Clarke Transform in the FCL

The FCL uses a 2-input Clarke transform regardless of whether 2 or 3 sensors are installed. Only `curA_PPBRESULT` and `curB_PPBRESULT` are read:

```c
// FCL_CLARKE_STYLE_1 macro (fcl_cpu_code_dm.c:85-90)
clarke1Alpha = (int16_t)HWREGH(pMotor->curA_PPBRESULT) * adcScale;
clarke1Beta  = (clarke1Alpha
              + 2.0 * (int16_t)HWREGH(pMotor->curB_PPBRESULT) * adcScale)
              * ONEbySQRT3;
```

This is mathematically equivalent to the standard Clarke for two measured phases A,B where the third is implied by Kirchhoff's current law (Ia + Ib + Ic = 0).

**In this codebase, "curA" = phase V and "curB" = phase W.** The mapping is set in `FCL_initADC_2I()` / `FCL_initADC_3I()`. The 3-shunt init also sets `curC` (phase U), but the Clarke macro never reads it -- it was only used for diagnostics / datalog.

---

## 2. Current State of the 2-Shunt Adaptation

### 2.1 Changes Already Made (branch `dev/currentsense`)

The following files have been modified to support the custom inverter's 2-current-sensor hardware:

#### `include/boostxl_3phganinv/dual_axis_servo_drive_user.h`

| Change | Detail |
|--------|--------|
| Sensor count config | Added `TWO_CURRENT_SENSORS` / `THREE_CURRENT_SENSORS` defines; set `COUNT_CURRENT_SENSORS = TWO_CURRENT_SENSORS` |
| `IS_TWO_SHUNT_DRIVE` | Defined when `COUNT_CURRENT_SENSORS == 2`; used as compile-time guard throughout |
| Phase U guarded | `M1_IU_*` ADC defines and `M1_IFB_U` / `M1_IFB_U_PPB` macros wrapped in `#ifndef IS_TWO_SHUNT_DRIVE` |
| Phase V ADC channel | Mapped to `ADCC_BASE`, `ADC_CH_ADCIN3` (J3 pin24, CMPIN6N/CMPSS6) |
| Phase W ADC channel | Mapped to `ADCB_BASE`, `ADC_CH_ADCIN3` (J3 pin25, CMPIN3N/CMPSS3) |
| Sensor analog front end | Documented: 90 mV/A sensitivity, 2.5 V zero, 3.0 V ADC ref -> +5.56 A positive headroom |
| `M1_FCL_ADC_SCALE` | Recomputed: `1/(Ibase * counts_per_amp) = 1/(5.0 * 122.88)` instead of old `1/2048` |
| CMPSS protection count | `COUNT_CURRENT_PROTECTION_CMPSS = 2U` (only V and W channels have comparator-capable inputs) |
| Current limits | `M1_BASE_CURRENT = 5.0`, `M1_MAXIMUM_CURRENT = 5.0` (below +5.56 A ADC headroom) |
| Voltage scale | Calibrated from hardware measurement: 600 V / 2.92 V -> 616.4 V full scale |

#### `include/boostxl_3phganinv/dual_axis_servo_drive_settings.h`

| Change | Detail |
|--------|--------|
| `BUILDLEVEL` | Set to `FCL_LEVEL2` for ADC/Clarke verification before closing the current loop |
| `DISABLE_MOTOR_FAULTS` | Defined (bench testing mode) |
| `DISABLE_ENDAT` | Defined (can be removed once encoder is wired) |

#### `sources/dual_axis_servo_drive_hal.c`

| Change | Detail |
|--------|--------|
| ADC setup | Phase U SOC/PPB setup guarded by `#ifndef IS_TWO_SHUNT_DRIVE`; V and W always configured |
| CMPSS handle init | 2-shunt path: `cmpssHandle[0] = M1_IV_CMPSS_BASE` (CMPSS6), `cmpssHandle[1] = M1_IW_CMPSS_BASE` (CMPSS3), `cmpssHandle[2] = 0` |
| CMPSS setup loop | Iterates over `COUNT_CURRENT_PROTECTION_CMPSS` (2) instead of hardcoded 3 |
| Fault protection | TRIP4 XBAR rebuilt with only MUX01 (gate-driver fault), MUX04 (CMPSS3/W), MUX10 (CMPSS6/V) |
| CMPSS DAC value loop | Bounded by `COUNT_CURRENT_PROTECTION_CMPSS` |
| Trip flag clearing | CMPSS latch clears bounded by `cnt < COUNT_CURRENT_PROTECTION_CMPSS` |

#### `sources/dual_axis_servo_drive_user.c`

| Change | Detail |
|--------|--------|
| FCL ADC init | Calls `FCL_initADC_2I()` when `IS_TWO_SHUNT_DRIVE` defined, else `FCL_initADC_3I()` |
| Offset calibration | Phase U offset calc guarded by `#ifndef IS_TWO_SHUNT_DRIVE`; V and W always calibrated |
| PPB offset write | Phase U PPB offset write guarded; V and W always written |

### 2.2 What the 2-Shunt Path Does

1. **ADC:** Only two SOCs are configured (ADCC ch3 for Iv, ADCB ch3 for Iw). Phase U has no ADC channel.

2. **`FCL_initADC_2I()`:** Sets `curA_PPBRESULT` -> Iv PPB result, `curB_PPBRESULT` -> Iw PPB result. The ADC interrupt flag pointer comes from the Iw ADC base (last channel to convert).

3. **Clarke Transform:** `FCL_CLARKE_STYLE_1()` reads only `curA` and `curB`. This naturally implements the 2-sensor Clarke:
   - `Alpha = Iv * adcScale`
   - `Beta = (Iv + 2*Iw) * adcScale / sqrt(3)`
   - Phase U current is implicitly reconstructed: `Iu = -(Iv + Iw)`

4. **Offset Calibration:** Only Iv and Iw offsets are measured and written to PPB reference registers.

5. **Over-Current Protection:** Two CMPSS modules (CMPSS6 for Iv, CMPSS3 for Iw) compare against DAC thresholds. Phase U is unprotected by hardware (no sensor).

---

## 3. Remaining Work for Full 2-Shunt Operation

### 3.1 Items Already Working (No Changes Needed)

| Component | Status | Notes |
|-----------|--------|-------|
| ADC channel configuration (Iv, Iw) | Done | Correct bases, channels, SOC, PPB |
| `FCL_initADC_2I()` call | Done | curA=Iv, curB=Iw correctly mapped |
| `FCL_CLARKE_STYLE_1()` macro | Works as-is | Only reads curA and curB; 2-sensor Clarke is mathematically valid |
| `FCL_runPICtrl_M1()` / `FCL_runComplexCtrl_M1()` | Works as-is | Uses the same Clarke macro; no 3rd sensor dependency |
| ADC offset calibration | Done | Iv and Iw offsets calibrated; Iu skipped |
| CMPSS over-current (Iv, Iw) | Done | 2 CMPSS modules configured, TRIP4 XBAR correct |
| HAL_MTR_init CMPSS handles | Done | 2-shunt path assigns CMPSS6 and CMPSS3 |
| `buildLevel2_M1()` current read | Done | Reads `M1_IFB_V_PPB` and `M1_IFB_W_PPB` into `clarke.As` and `clarke.Bs` |
| Fault protection XBAR | Done | TRIP4 uses MUX01 + MUX04 + MUX10 |

### 3.2 Items Requiring Attention Before Closing the Current Loop (FCL_LEVEL3/4)

#### 3.2.1 ADC Scale Sign Convention

**File:** `dual_axis_servo_drive_user.c:91`

```c
pMotor->FCL_params.adcScale = -M1_FCL_ADC_SCALE;
```

`M1_FCL_ADC_SCALE` is defined as `1/(Ibase * counts_per_amp)` = positive. The negative sign (`-`) is applied in `initMotorParameters()`. This produces a negative `adcScale`, which the original TI code expects because its sensors had inverted polarity (low-side shunts where positive current produces negative voltage swing from the midpoint).

**Action needed:** Verify the polarity of your current sensors. If your sensors produce a voltage *above* 2.5 V for positive current (typical for hall-effect current sensors like ACS712), the sign may need to flip. Connect one phase at a time, command a known positive torque, and confirm:
- `clarke.As` (Iv) and `clarke.Bs` (Iw) have correct signs
- Park transform outputs `pi_id.fbk` and `pi_iq.fbk` track their references with correct sign

If signs are inverted, change to `pMotor->FCL_params.adcScale = +M1_FCL_ADC_SCALE;`

#### 3.2.2 Phase-to-PWM Mapping Consistency

The Clarke transform assigns:
- `curA` (Alpha) = Phase V measurement
- `curB` (Beta input) = Phase W measurement

The SVGEN output maps:
- `svgen.Tc` -> `pwmHandle[0]` (EPWM1) = Phase U
- `svgen.Ta` -> `pwmHandle[1]` (EPWM2) = Phase V
- `svgen.Tb` -> `pwmHandle[2]` (EPWM3) = Phase W

**Action needed:** Verify that the physical wiring matches this mapping:
1. EPWM1 gate output drives the U-phase half-bridge
2. EPWM2 gate output drives the V-phase half-bridge (same leg as Iv sensor)
3. EPWM3 gate output drives the W-phase half-bridge (same leg as Iw sensor)

If the wiring is different, either swap the `pwmHandle[]` assignment in `HAL_MTR_init()` or swap the ADC channel assignments.

#### 3.2.3 Phase U Over-Current Protection Gap

With only 2 current sensors, phase U has no hardware over-current protection. The CMPSS trip path only monitors Iv and Iw.

**Risk:** A short or overcurrent event on the U-phase leg will not trigger a hardware trip until the current redistributes to V or W phases.

**Mitigation options:**
1. **Software OCP:** Add a software check in the ISR that reconstructs `Iu = -(Iv + Iw)` and compares against a threshold. Latency is one ISR period (100 us) -- not as fast as hardware, but catches sustained faults.
2. **Gate driver fault:** If the U-phase gate driver has its own DESAT or OCP output connected to GPIO24 (already on TRIP4 MUX01), that provides partial protection.
3. **Accept the risk:** For bench testing at low currents (<5 A) with `DISABLE_MOTOR_FAULTS` defined, this is acceptable.

#### 3.2.4 ADC Conflict Check: ADCC Used for Both Iv (SOC0, ch3) and Potentially Iu (SOC0, ch2)

In 2-shunt mode, ADCC is used for Iv on channel ADCIN3 via SOC0. In 3-shunt mode, ADCC is *also* used for Iu on channel ADCIN2 via SOC0. Since both use `ADC_SOC_NUMBER0` on the same ADC module, they would conflict if both were configured simultaneously.

**Status:** This is correctly handled by the `#ifndef IS_TWO_SHUNT_DRIVE` guard -- the Iu SOC is never configured in 2-shunt mode. No action needed, but be aware that switching back to 3-shunt will require verifying SOC number assignments if Iu and Iv share the same ADC module.

#### 3.2.5 Build Level Transition

The current build level is `FCL_LEVEL2` (open-loop). To close the current loop:

1. Set `BUILDLEVEL = FCL_LEVEL3` in `dual_axis_servo_drive_settings.h`
2. Verify `DISABLE_ENDAT` is removed (or that the encoder is providing valid position)
3. Start with conservative PI gains and low `IqRef` (0.01-0.03 pu)
4. Monitor `pi_id.fbk`, `pi_iq.fbk`, and `FCL_cycleCount[0]` to confirm loop stability

#### 3.2.6 Datalog Channel Verification

In `buildLevel2_M1()`, the datalog channels should capture the two measured currents for validation. Verify that `dlog_4ch1` input pointers include `clarke.As` (Iv) and `clarke.Bs` (Iw) so you can confirm ADC readings during open-loop operation.

---

## 4. Current Sensor Analog Front End Summary

| Parameter | Value | Derivation |
|-----------|-------|------------|
| Sensitivity | 90 mV/A | Sensor datasheet |
| Zero-current voltage | 2.5 V | Sensor bias |
| ADC reference | 3.0 V | F2837x specification |
| ADC resolution | 12-bit (4096 counts) | -- |
| Counts per amp | 122.88 | (0.09 V/A * 4096) / 3.0 V |
| Max positive current | +5.56 A | (3.0 - 2.5) / 0.09 |
| Max negative current | -27.78 A | 2.5 / 0.09 |
| CMPSS zero count | 3413 | (2.5 / 3.0) * 4096 |
| `M1_BASE_CURRENT` | 5.0 A | Conservative limit below +5.56 A |
| `M1_FCL_ADC_SCALE` | 0.001628 | 1 / (5.0 * 122.88) |

**Asymmetric headroom warning:** The 2.5 V bias with a 3.0 V ADC reference means only 0.5 V (5.56 A) of positive headroom but 2.5 V (27.78 A) of negative headroom. If the motor can draw more than 5.56 A peak in the positive direction, the ADC will saturate. Hardware mitigation (level-shifting or attenuation) would be needed for higher currents.

---

## 5. ADC Channel Map (2-Shunt Configuration)

| Signal | ADC Module | Channel | SOC | PPB | Pin | CMPSS |
|--------|-----------|---------|-----|-----|-----|-------|
| Phase V (Iv) | ADCC | ADCIN3 | SOC0 | PPB1 | J3-24 | CMPSS6 (CMPIN6N) |
| Phase W (Iw) | ADCB | ADCIN3 | SOC0 | PPB1 | J3-25 | CMPSS3 (CMPIN3N) |
| DC Bus (Vdc) | ADCD | ADCIN15 | SOC0 | PPB1 | J7-63 | -- |
| Phase U (Iu) | *not connected* | -- | -- | -- | -- | -- |

All SOCs are triggered by `EPWM1_SOCA` (synchronized to PWM counter zero in single-sampling mode).

---

## 6. FCL Function Reference (Quick Map)

| Function | File:Line | Called From | Purpose |
|----------|-----------|-------------|---------|
| `FCL_initPWM()` | fcl_cpu_code_dm.c:254 | initMotorParameters | Disable CMPA shadow, store PWM pointers |
| `FCL_initADC_2I()` | fcl_cpu_code_dm.c:276 | initMotorParameters | Bind 2 ADC PPB results to curA/curB |
| `FCL_initADC_3I()` | fcl_cpu_code_dm.c:296 | initMotorParameters | Bind 3 ADC PPB results (3-shunt only) |
| `FCL_initQEP()` | fcl_cpu_code_dm.c:319 | initMotorParameters | Store QEP base (QEP encoder only) |
| `FCL_resetController()` | fcl_cpu_code_dm.c:330 | buildLevel*, resetControlVars | Zero PI state |
| `FCL_runPICtrl_M1()` | fcl_cpu_code_dm.c:411 | buildLevel3/46_M1 | Full current loop: Clarke->Park->PI->iPark->SVGEN->PWM |
| `FCL_runPICtrlWrap_M1()` | fcl_cpu_code_dm.c:485 | buildLevel3/46_M1 | Update PI gains, BEMF feedforward, clear CLA flags |
| `FCL_runComplexCtrl_M1()` | fcl_cpu_code_dm.c:570 | buildLevel3/46_M1 | Alternative complex controller path |
| `FCL_runComplexCtrlWrap_M1()` | fcl_cpu_code_dm.c:648 | buildLevel3/46_M1 | Complex controller gain update |

---

## 7. Recommendations

### Immediate (before closing the current loop)

1. **Validate ADC readings at `FCL_LEVEL2`.** With the motor disconnected from load, run a small open-loop voltage (VqTesting = 0.05) and confirm that `clarke.As` and `clarke.Bs` show sinusoidal currents 120 degrees apart, with correct amplitude and sign.

2. **Confirm sensor polarity.** If the sign of `adcScale` is wrong, the current loop will have positive feedback and will immediately saturate or oscillate. Check at Level 2 before moving to Level 3.

3. **Remove `DISABLE_MOTOR_FAULTS` only after verifying CMPSS thresholds.** With `M1_MAXIMUM_CURRENT = 5.0 A` and `M1_CMPSS_ZERO_COUNT = 3413`, the CMPSS DAC-H = 3413 + 614 = 4027 and DAC-L = 3413 - 614 = 2799. Verify these are within the 12-bit range (0-4095). DAC-H = 4027 is close to saturation -- consider reducing `M1_MAXIMUM_CURRENT` to 4.5 A for margin.

### Medium-term

4. **Add software OCP for phase U** in the ISR (reconstruct Iu from Iv + Iw and check against a threshold). This closes the protection gap from having no sensor on phase U.

5. **Tune PI gains using SFRA at `FCL_LEVEL6`** once the current loop is stable at Level 3/4. The auto-computed gains from motor parameters (`wccD`, `wccQ`) may need adjustment for your specific hardware parasitics.

### Phase mapping note

The TI convention used in this code treats Phase V as the "first" Clarke input and Phase W as the "second." This is a 120-degree rotated frame compared to the textbook convention (Phase U first). The control algorithm is self-consistent because the encoder angle, Park transform, and PWM mapping are all aligned to this convention. **Do not** try to "fix" this to put Phase U first -- it would break the existing working relationships.
