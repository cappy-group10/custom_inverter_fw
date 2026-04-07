# HAL Architecture

**Primary file:** `dual_axis_servo_drive_hal.c`  
**Target:** TI F28379D / `main_foc_endat_system`  
**Last reviewed:** 2026-04-07

---

## 1. Current Checked-In HAL Snapshot

| Setting | Value | Meaning |
|---|---|---|
| `SAMPLING_METHOD` | `SINGLE_SAMPLING` | `EPWM1` ISR runs at `10 kHz` |
| `COUNT_CURRENT_SENSORS` | `2` | Two-shunt current measurement |
| `COUNT_CURRENT_PROTECTION_CMPSS` | `2` | CMPSS only on phases V and W |
| `ENDAT_PRODUCER_RATE_RATIO` | `3` | `EPWM9` producer runs at `30 kHz` |
| `DACOUT_EN` | defined | EPWM7/8 debug DAC mux is enabled |
| `DISABLE_MOTOR_FAULTS` | defined | `main()` does not call `HAL_setupMotorFaultProtection()` in the checked-in build |

---

## 2. HAL Object Hierarchy

```text
HAL_Obj (halHandle / hal)
|
+- dacHandle[3]       -> DACA, DACB, DACC
+- claHandle          -> CLA1
+- sciHandle[2]       -> SCIA, SCIB
+- timerHandle[3]     -> CPUTIMER0, CPUTIMER1, CPUTIMER2
+- adcHandle[4]       -> ADCA, ADCB, ADCC, ADCD
\- adcResult[4]       -> ADCARESULT .. ADCDRESULT

HAL_MTR_Obj (halMtrHandle[0])
|
+- spiHandle          -> M1_SPI_BASE
+- pwmHandle[3]       -> motor phase U / V / W
+- cmpssHandle[3]     -> 2 active handles in 2-shunt mode
\- qepHandle          -> 0 in the checked-in EnDat build
```

For the current 2-shunt build:

- `cmpssHandle[0] = CMPSS6` for phase V
- `cmpssHandle[1] = CMPSS3` for phase W
- `cmpssHandle[2] = 0`

---

## 3. Initialization Sequence

This is the effective HAL-related startup order from `main()`:

```text
main()
|
+-[1]  Device_init()
|
+-[2]  HAL_init()
|       \- populate system-level handles
|
+-[3]  HAL_MTR_init(MTR_1)
|       \- populate motor-1 peripheral handles
|
+-[4]  disable TBCLKSYNC
|
+-[5]  HAL_setParams()
|       |
|       +- set LSPCLK
|       +- init GPIO
|       +- setup CLA memory and vectors
|       +- init interrupt module and vector table
|       +- setup CPU timers
|       +- setup GPIO muxing
|       +- setup DACs when enabled
|       \- setup ADCs and PPBs
|
+-[6]  HAL_setMotorParams(MTR_1)
|       |
|       +- HAL_setupMotorPWMs()
|       |    +- configure EPWM1/2/3 for inverter switching
|       |    \- configure EPWM9 as the EnDat producer scheduler
|       \- HAL_setupCMPSS()
|
+-[7]  enable TBCLKSYNC
|
+-[8]  [unless DISABLE_MOTOR_FAULTS] HAL_setupMotorFaultProtection()
+-[9]  HAL_setupInterrupts()
+-[10] HAL_enableInterrupts()
|
\-[11] runtime
```

Two important conditionals:

- `HAL_setupMotorFaultProtection()` exists and is documented here, but the checked-in `main()` skips it because `DISABLE_MOTOR_FAULTS` is defined.
- `HAL_setupQEP()` is not part of the active path because the checked-in build uses EnDat and explicitly leaves `qepHandle = 0`.

---

## 4. Function Map

| Function | Purpose |
|---|---|
| `HAL_init()` | Populate the system HAL object |
| `HAL_MTR_init()` | Populate the motor HAL object |
| `HAL_setParams()` | Configure CLA, GPIO, ADC, timers, UART-related SCI handles, and optional DAC |
| `HAL_setMotorParams()` | Configure motor PWMs and CMPSS |
| `HAL_setupCLA()` | Configure CLA memory ownership, vectors, and triggers |
| `HAL_setupADCs()` | Configure current and voltage ADC modules, SOCs, and PPBs |
| `HAL_setupGPIOs()` | Configure board-level pin muxing and directions |
| `HAL_setupDACs()` | Configure DAC outputs when `DACOUT_EN` is defined |
| `HAL_setupMotorPWMs()` | Configure EPWM1/2/3 plus the EnDat producer scheduler |
| `HAL_setupEndatProducerPWM()` | Configure `EPWM9` as the internal `30 kHz` EnDat scheduler |
| `HAL_setupCMPSS()` | Configure current-protection comparators |
| `HAL_setupMotorFaultProtection()` | Build the TRIP4 path and configure trip-zone behavior |
| `HAL_setupInterrupts()` | Register `motor1ControlISR()` and `endatProducerISR()` |
| `HAL_enableInterrupts()` | Enable PIE/CPU interrupt routing for the active PWM paths |

---

## 5. Hardware Resource Map

### Motor PWM resources

| EPWM | Role | Notes |
|---|---|---|
| `EPWM1` | Motor phase U + control ISR source | Also drives `Cla1Task1()` timing |
| `EPWM2` | Motor phase V | Standard inverter PWM |
| `EPWM3` | Motor phase W | Standard inverter PWM |
| `EPWM4` | EnDat clock-generation path | Used by `endat_init.c` |
| `EPWM9` | EnDat producer scheduler | Internal only, about `30 kHz` in the checked-in build |

### CPU timers

| Timer | Period | Use |
|---|---:|---|
| `CPUTIMER0` | `50 us` | A branch |
| `CPUTIMER1` | `100 us` | B branch |
| `CPUTIMER2` | `150 us` | C branch |

### ADC map for the current 2-shunt build

| Signal | ADC module | Channel | Trigger |
|---|---|---|---|
| Phase V current (`Iv`) | `ADCC` | `ADCIN3` | `EPWM1 SOCA` |
| Phase W current (`Iw`) | `ADCB` | `ADCIN3` | `EPWM1 SOCA` |
| DC bus voltage | `ADCD` | `ADCIN15` | `EPWM1 SOCA` |

Phase U current is not measured directly in the checked-in hardware build.

### Interrupt ownership

| Source | PIE group | Consumer |
|---|---|---|
| `EPWM1 INT` | Group 3 / INT1 | `motor1ControlISR()` and CLA task timing |
| `EPWM9 INT` | Group 3 / INT9 | `endatProducerISR()` |
| `SPI-B RX INT` | Group 6 / INT3 | `spiRxFifoIsr()` |

---

## 6. GPIO Notes

| GPIO | Function | Notes |
|---|---|---|
| `0-5` | EPWM1/2/3 outputs | Motor 1 inverter phases |
| `6-7` | EPWM4A/B | EnDat clock path |
| `24` | Gate-driver fault input | Routed through `INPUTXBAR1` into TRIP4 |
| `42-43` | SCI-A TX/RX | Host UART link |
| `63-66` | SPI-B | EnDat serial interface |
| `139` | Conflicting ownership | Reclaimed by `EnDat_Init()` as EnDat 5 V enable |
| `157-160` | EPWM7/8 debug outputs | Enabled because `DACOUT_EN` is currently defined |

---

## 7. Important Constraints and Known HAL Caveats

### `EPWM9` is now owned by EnDat scheduling

The current build reserves `EPWM9` for the independent EnDat producer scheduler. It is not intended as a normal application PWM output.

### GPIO139 is still a real collision point

`HAL_setupGPIOs()` still contains legacy configuration that treats GPIO139 as a generic input, while `endat_init.c` later reclaims GPIO139 as the EnDat 5 V power-enable output.

This means:

- the checked-in runtime still works because `EnDat_Init()` runs after the HAL GPIO setup and reprograms the pin,
- but GPIO139 ownership is not cleanly expressed in one place yet.

### The old GPIO156/GPIO139 typo is still present

The `HAL_setupGPIOs()` section labeled for GPIO156 still configures GPIO139. That is a documentation-worthy code caveat because it helps explain why GPIO139 behavior looks inconsistent unless you read both HAL and EnDat init paths together.

### Fault-protection helpers are present but not active by default

The CMPSS, XBAR, and trip-zone helper code still exists and still documents the intended protection design, but the checked-in `main()` currently skips `HAL_setupMotorFaultProtection()` because `DISABLE_MOTOR_FAULTS` is defined.

---

## 8. Bottom Line

The HAL has already been reshaped around the current project realities:

- one actively controlled motor,
- EnDat-driven angle feedback,
- a dedicated `EPWM9` producer scheduler,
- and a two-shunt current-sense path.

The main HAL cleanup still left is ownership cleanup around GPIO139/GPIO156 and the deliberate re-enabling of fault-protection logic once bench validation is complete.
