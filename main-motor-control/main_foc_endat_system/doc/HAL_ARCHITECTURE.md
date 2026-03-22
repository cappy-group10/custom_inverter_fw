# Hardware Abstraction Layer (HAL) — Architecture & Execution Map

**File:** `dual_axis_servo_drive_hal.c`  
**Target:** TI F2837x (C2000) — Dual-Axis Servo Drive  
**Last reviewed:** 2025

---

## Table of Contents

1. [Object Hierarchy](#1-object-hierarchy)
2. [Initialization Sequence (Execution Order)](#2-initialization-sequence-execution-order)
3. [Function Reference](#3-function-reference)
4. [Dependency Graph](#4-dependency-graph)
5. [Hardware Resource Map](#5-hardware-resource-map)
6. [ISR & CLA Task Map](#6-isr--cla-task-map)
7. [GPIO Assignment Table](#7-gpio-assignment-table)
8. [Known Issues / Notes](#8-known-issues--notes)

---

## 1. Object Hierarchy

```
HAL_Obj  (halHandle / hal)
│
├── dacHandle[3]       → DACA, DACB, DACC
├── claHandle          → CLA1
├── sciHandle[2]       → SCIA, SCIB
├── timerHandle[3]     → CPUTIMER0, CPUTIMER1, CPUTIMER2
├── adcHandle[4]       → ADCA, ADCB, ADCC, ADCD
└── adcResult[4]       → ADCARESULT .. ADCDRESULT

HAL_MTR_Obj  (halMtrHandle[2] / halMtr[2])
│
├── spiHandle          → M1_SPI_BASE  (MTR_1 only)
├── pwmHandle[3]       → U/V/W phase PWM bases
├── cmpssHandle[3]     → U/V/W phase CMPSS bases
└── qepHandle          → M1_QEP_BASE  (only when POSITION_ENCODER_IS_QEP)
```

> `HAL_Obj` is system-wide. `HAL_MTR_Obj` is instantiated per motor axis (index 0 = MTR_1, index 1 = MTR_2).

---

## 2. Initialization Sequence (Execution Order)

The following is the call order as executed from `main()` in `dual_axis_servo_drive.c`.

```
main()
│
├─[1]  Device_init()                         ← TI driverlib: clocks, flash, PLL
│
├─[2]  HAL_init(&hal, sizeof(hal))
│       └── SysCtl_disableWatchdog()
│       └── Assigns: DAC, CLA, SCI, Timer, ADC handles
│
├─[3]  HAL_MTR_init(&halMtr[MTR_1], ...)
│       └── Assigns: SPI, PWM, CMPSS, QEP handles (MTR_1)
│
├─[4]  SysCtl_disablePeripheral(TBCLKSYNC)   ← freeze PWM clocks before config
│
├─[5]  HAL_setParams(halHandle)
│       ├── SysCtl_setLowSpeedClock(DIV_4)   → 50 MHz LSPCLK
│       ├── Device_initGPIO()
│       ├── HAL_setupCLA()
│       │     ├── memcpy CLA code/const (flash builds only)
│       │     ├── SysCtl_selectSecMaster(CLA)
│       │     ├── MemCfg_initSections() — CLA msg RAMs
│       │     ├── MemCfg set LS4/LS5 → CLA program RAM
│       │     ├── MemCfg set LS2/LS3 → CLA data RAM
│       │     ├── CLA_mapTaskVector() × 8  (Tasks 1–8)
│       │     ├── CLA_enableIACK() + CLA_enableTasks(ALL)
│       │     ├── CLA_setTriggerSource(TASK_1, EPWM1INT)
│       │     └── CLA_setTriggerSource(TASK_5, EPWM4INT)
│       ├── Interrupt_initModule()
│       ├── Interrupt_initVectorTable()
│       ├── HAL_setupCpuTimer(TIMER0, 50µs)  → A-tasks
│       ├── HAL_setupCpuTimer(TIMER1, 100µs) → B-tasks
│       ├── HAL_setupCpuTimer(TIMER2, 150µs) → C-tasks
│       ├── HAL_setupGPIOs()
│       ├── HAL_setupDACs()                  [#ifdef DACOUT_EN]
│       └── HAL_setupADCs()
│             ├── ADC config: 12-bit, single-ended, 50 MHz, all 4 modules
│             └── SOC + PPB setup for M1: Iu(C2), Iv(B2), Iw(A2), Vdc(D14)
│
├─[6]  HAL_setMotorParams(halMtrHandle[MTR_1])
│       ├── HAL_setupMotorPWMs()
│       │     ├── EPWM1/2/3 — up/down count, complementary deadband
│       │     ├── Phase shifts: PWM1=0, PWM2=2, PWM3=4
│       │     ├── Sync: EPWM1 → EPWM4 via SYSCTL_SYNC_IN_EPWM4
│       │     └── ADC SOC trigger from EPWM1 SOCA (zero or period)
│       ├── HAL_setupCMPSS()
│       │     ├── CMPSS 1/2/3 — DAC-driven comparators, digital filters
│       │     ├── DAC-H = 1024 (initial, overridden by fault setup)
│       │     └── DAC-L = 1024 (initial)
│       └── HAL_setupQEP()                   [#if POSITION_ENCODER_IS_QEP]
│             ├── 2x quadrature resolution, run-free emulation
│             ├── Position reset at max (4 × M1_ENCODER_LINES − 1)
│             ├── Unit timer @ M1_QEP_UNIT_TIMER_TICKS
│             └── Capture: CLK/128, unit-pos/32
│
├─[7]  SysCtl_enablePeripheral(TBCLKSYNC)    ← unfreeze PWM clocks
│
├─[8]  initMotorParameters(&motorVars[0], ...)
├─[9]  initControlVars(&motorVars[0])
│
├─[10] HAL_setupMotorFaultProtection(MTR_1, currentLimit)
│       ├── HAL_setupCMPSS_DACValue() — sets actual OCP trip levels
│       ├── XBAR: TRIP4 ← CMPSS1_H_OR_L | CMPSS3_H_OR_L |
│       │                  CMPSS6_H_OR_L | INPUTXBAR1 (GPIO24)
│       └── EPWM TZ: DCAEVT1 + CBC6 → force A/B low on fault
│
├─[11] resetControlVars(&motorVars[0])
├─[12] HAL_clearTZFlag(halMtrHandle[MTR_1])
│
├─[13] HAL_setupInterrupts(halMtrHandle[MTR_1])
│       ├── EPWM1 INT source: zero (single) or zero+period (double sampling)
│       ├── Interrupt_register(M1_INT_PWM, motor1ControlISR)
│       └── ADC INT1 on M1_IW (ADCA SOC): continuous mode
│
├─[14] runOffsetsCalculation(&motorVars[0])  ← current sensor offset cal
│
├─[15] EnDat encoder init sequence
│       ├── EnDat_Init()
│       ├── endat21_runCommandSet()
│       ├── endat22_setupAddlData()
│       ├── EnDat_initDelayComp()
│       ├── PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER)
│       └── endat21_schedulePositionRead()
│
├─[16] HAL_enableInterrupts(halMtrHandle[MTR_1])
│       ├── EPWM_clearEventTriggerInterruptFlag(PWM1)
│       ├── Interrupt_enable(M1_INT_PWM)
│       └── Interrupt_enableInCPU(INT3)
│
└─[17] EINT / ERTM  ← global interrupt enable → enter main loop
```

---

## 3. Function Reference

| Function | Owner Object | Called From | Purpose |
|---|---|---|---|
| `HAL_init()` | `HAL_Obj` | `main()` | Allocates & populates system-level handle |
| `HAL_MTR_init()` | `HAL_MTR_Obj` | `main()` | Allocates & populates per-motor handle |
| `HAL_setParams()` | `HAL_Obj` | `main()` | System-wide peripheral config (CLA, GPIO, ADC, DAC, timers) |
| `HAL_setMotorParams()` | `HAL_MTR_Obj` | `main()` | Motor-specific peripheral config (PWM, CMPSS, QEP) |
| `HAL_setupCLA()` | `HAL_Obj` | `HAL_setParams()` | CLA RAM, task vectors, triggers |
| `HAL_setupADCs()` | `HAL_Obj` | `HAL_setParams()` | 4× ADC modules, SOC, PPB for current/voltage |
| `HAL_setupDACs()` | `HAL_Obj` | `HAL_setParams()` | 3× DAC outputs (resolver + debug); `#ifdef DACOUT_EN` |
| `HAL_setupGPIOs()` | `HAL_Obj` | `HAL_setParams()` | All GPIO mux/direction/pad assignments |
| `HAL_setupCpuTimer()` | static | `HAL_setParams()` | Configure one CPU timer with period count |
| `HAL_setupMotorPWMs()` | `HAL_MTR_Obj` | `HAL_setMotorParams()` | EPWM 1-6 (M1: 1-3, M2: 4-6), deadband, sync, ADC SOC trigger |
| `HAL_setupCMPSS()` | `HAL_MTR_Obj` | `HAL_setMotorParams()` | CMPSS comparators, filters, initial DAC values (1024) |
| `HAL_setupCMPSS_DACValue()` | `HAL_MTR_Obj` | `HAL_setupMotorFaultProtection()` | Update OCP DAC levels at runtime |
| `HAL_setupQEP()` | `HAL_MTR_Obj` | `HAL_setMotorParams()` | QEP decoder, unit timer, capture unit |
| `HAL_setupMotorFaultProtection()` | `HAL_MTR_Obj` | `main()` | XBAR/TRIP4 routing, TZ actions for OCP |
| `HAL_setupInterrupts()` | `HAL_MTR_Obj` | `main()` | Register ISR, configure EPWM INT and ADC INT1 |
| `HAL_enableInterrupts()` | `HAL_MTR_Obj` | `main()` | Unmask PWM interrupt in PIE + CPU |

---

## 4. Dependency Graph

```
HAL_init()
    │
    └──► HAL_setParams()
              ├──► HAL_setupCLA()          depends on: linker symbols (CLA RAM)
              ├──► HAL_setupCpuTimer() ×3
              ├──► HAL_setupGPIOs()
              ├──► HAL_setupDACs()         [DACOUT_EN only]
              └──► HAL_setupADCs()

HAL_MTR_init()
    │
    └──► HAL_setMotorParams()
              ├──► HAL_setupMotorPWMs()    depends on: HAL_setupCLA() (EPWM triggers CLA)
              ├──► HAL_setupCMPSS()
              └──► HAL_setupQEP()          [POSITION_ENCODER_IS_QEP only]

─── After both init chains complete ───────────────────────────────

HAL_setupMotorFaultProtection()
    └──► HAL_setupCMPSS_DACValue()         depends on: HAL_setupCMPSS() already run

HAL_setupInterrupts()                      depends on: HAL_setupMotorPWMs() already run
HAL_enableInterrupts()                     depends on: HAL_setupInterrupts() already run

─── Runtime (ISR) ─────────────────────────────────────────────────

motor1ControlISR()    ← triggered by EPWM1 INT (PIE Group 3)
    └── FCL Tasks 1–4 run on CLA (triggered by EPWM1INT)

(MTR_2 ISR)           ← triggered by EPWM4 INT
    └── FCL Tasks 5–8 run on CLA (triggered by EPWM4INT)
```

---

## 5. Hardware Resource Map

### ADC Channels — Motor 1

| Signal | ADC Module | SOC # | PPB # | GPIO/Pin | Trigger |
|---|---|---|---|---|---|
| M1 Phase U current (Iu) | ADCC | `M1_IU_ADC_SOC_NUM` | `M1_IU_ADC_PPB_NUM` | C2 | EPWM1 SOCA |
| M1 Phase V current (Iv) | ADCB | `M1_IV_ADC_SOC_NUM` | `M1_IV_ADC_PPB_NUM` | B2 | EPWM1 SOCA |
| M1 Phase W current (Iw) | ADCA | `M1_IW_ADC_SOC_NUM` | `M1_IW_ADC_PPB_NUM` | A2 | EPWM1 SOCA |
| M1 DC Bus voltage (Vdc) | ADCD | `M1_VDC_ADC_SOC_NUM` | `M1_VDC_ADC_PPB_NUM` | D14 | EPWM1 SOCA |

All PPB calibration offsets initialised to 0; corrected by `runOffsetsCalculation()`.

### PWM Assignments

| EPWM Module | Motor | Phase | A output | B output |
|---|---|---|---|---|
| EPWM1 | MTR_1 | U | UH | UL |
| EPWM2 | MTR_1 | V | VH | VL |
| EPWM3 | MTR_1 | W | WH | WL |
| EPWM4 | MTR_2 | U | UH | UL |
| EPWM5 | MTR_2 | V | VH | VL |
| EPWM6 | MTR_2 | W | WH | WL |

EPWM1 is the sync master → EPWM4 via `SYSCTL_SYNC_IN_EPWM4`.

### CMPSS Assignments

| CMPSS Module | Motor | Phase | XBAR Trip |
|---|---|---|---|
| CMPSS1 | MTR_1 | U | TRIP4 MUX00 |
| CMPSS3 | MTR_1 | V | TRIP4 MUX04 |
| CMPSS6 | MTR_1 | W | TRIP4 MUX10 |

INPUTXBAR1 (GPIO24, OT_M1) is also OR'd into TRIP4 via MUX01.

### CPU Timers

| Timer | Period | Purpose |
|---|---|---|
| CPUTIMER0 | 50 µs | A-task state machine |
| CPUTIMER1 | 100 µs | B-task state machine |
| CPUTIMER2 | 150 µs | C-task state machine |

---

## 6. ISR & CLA Task Map

### CPU ISR

| ISR | PIE Group | Trigger | Owner |
|---|---|---|---|
| `motor1ControlISR` | Group 3 (`M1_INT_PWM`) | EPWM1 INT (zero or zero+period) | CPU |

### CLA Tasks

| Task | Trigger | Assigned To |
|---|---|---|
| `Cla1Task1` | EPWM1 INT | FCL Motor 1 (step 1 of 4) |
| `Cla1Task2` | (software IACK) | FCL Motor 1 (step 2 of 4) |
| `Cla1Task3` | (software IACK) | FCL Motor 1 (step 3 of 4) |
| `Cla1Task4` | (software IACK) | FCL Motor 1 (step 4 of 4) |
| `Cla1Task5` | EPWM4 INT | FCL Motor 2 (step 1 of 4) |
| `Cla1Task6` | (software IACK) | FCL Motor 2 (step 2 of 4) |
| `Cla1Task7` | (software IACK) | FCL Motor 2 (step 3 of 4) |
| `Cla1Task8` | (software IACK) | FCL Motor 2 (step 4 of 4) |

CLA program RAM: LS4 + LS5. CLA data RAM: LS2 + LS3.

---

## 7. GPIO Assignment Table

| GPIO | Signal | Direction | Function |
|---|---|---|---|
| 0 | UH_M1 | OUT | EPWM1A |
| 1 | UL_M1 | OUT | EPWM1B |
| 2 | VH_M1 | OUT | EPWM2A |
| 3 | VL_M1 | OUT | EPWM2B |
| 4 | WH_M1 | OUT | EPWM3A |
| 5 | WL_M1 | OUT | EPWM3B |
| 6 | UH_M2 | OUT | EPWM4A |
| 7 | UL_M2 | OUT | EPWM4B |
| 8 | VH_M2 | OUT | EPWM5A |
| 9 | VL_M2 | OUT | EPWM5B |
| 10 | WH_M2 | OUT | EPWM6A |
| 11 | WL_M2 | OUT | EPWM6B |
| 12 | CANTXB | IN | CAN-B TX |
| 14 | OT_M2 | IN (inverted) | Over-temp fault M2 |
| 17 | CANRXB | IN | CAN-B RX |
| 18 | Debug | OUT | GPIO |
| 19 | nFault_M1 | IN | DRV fault input M1 |
| 20 | QEP1A_M1 | IN | EQEP1A (3-sample qual) |
| 21 | QEP1B_M1 | IN | EQEP1B (3-sample qual) |
| 24 | OT_M1 | IN (inverted) | Over-temp fault M1 / INPUTXBAR1 |
| 26 | EN_GATE_M2 | OUT (pullup, init=1) | Gate driver enable M2 |
| 31 | LED1 | OUT (init=1) | Status LED |
| 34 | LED2 | OUT (init=1) | Status LED |
| 40 | SDAB | IN | I²C data |
| 41 | SCLB | IN | I²C clock |
| 42 | SCITXDA | IN | SCI-A TX |
| 43 | SCIRXDA | IN | SCI-A RX |
| 54 | QEP2A_M2 | IN | EQEP2A (3-sample qual) |
| 55 | QEP2B_M2 | IN | EQEP2B (3-sample qual) |
| 56 | SCITXDC | OUT | SCI-C TX |
| 57 | QEP2I_M2 | IN | EQEP2I (3-sample qual) |
| 58 | SPISIMOA | OUT | SPI-A MOSI (M1) |
| 59 | SPISOMIA | IN | SPI-A MISO (M1) |
| 60 | SPICLKA | OUT | SPI-A CLK (M1) |
| 61 | SPISTEA | OUT | SPI-A CS (M1) |
| 63 | SPISIMOB | OUT | SPI-B MOSI (M2) |
| 64 | SPISOMIB | IN | SPI-B MISO (M2) |
| 65 | SPICLKB | OUT | SPI-B CLK (M2) |
| 66 | SPISTEB | OUT | SPI-B CS (M2) |
| 99 | QEP1I | IN | EQEP1I index (3-sample qual) |
| 124 | EN_GATE_M1 | OUT (pullup, init=1) | Gate driver enable M1 |
| 125 | WAKE_M1 | IN | DRV wake M1 |
| 139 | nFault_M2 | IN | DRV fault input M2 |
| 157 | EPWM7A | OUT | DAC1 (PWM-based) |
| 158 | EPWM7B | OUT | DAC2 |
| 159 | EPWM8A | OUT | DAC3 |
| 160 | EPWM8B | OUT | DAC4 |

> **Note:** GPIO156 block in source incorrectly re-configures GPIO139. This appears to be a copy-paste bug — GPIO156 is set up as GPIO139. Review before adding Motor 2 fault logic.

---

## 8. Known Issues / Notes

### Bug: GPIO156 configures GPIO139 (duplicate)
In `HAL_setupGPIOs()`, the block labelled `// GPIO156->GPIO` calls:
```c
GPIO_setMasterCore(139, ...);   // ← should be 156
GPIO_setPinConfig(GPIO_139_GPIO139, ...);
```
This silently reconfigures GPIO139 (nFault_M2) a second time. GPIO156 is never actually configured.

### Motor 2 HAL stubs
`HAL_MTR_init()` and `HAL_setMotorParams()` only fully configure MTR_1. MTR_2 PWM/CMPSS/QEP handles are declared in the object but the init block for `halMtr[MTR_2]` is absent. The CLA tasks 5–8 and EPWM4–6 GPIO assignments exist; motor 2 HAL init requires completion.

### `DISABLE_MOTOR_FAULTS` guard
Fault protection (`HAL_setupMotorFaultProtection`) is wrapped in `#ifndef DISABLE_MOTOR_FAULTS`. Ensure this is **not** defined in production builds.

### `ENDAT_HACK` guard
The startup `while(enableFlag == false)` spin-wait is bypassed when `ENDAT_HACK` is defined. Used during encoder bringup; must not be left enabled in production.

### Flash vs. RAM builds
CLA code copy (`memcpy` of `Cla1funcsRunStart`) only runs in `_FLASH` builds. In RAM debug sessions, CCS loads CLA program RAM directly and the copy is skipped.

### Sampling method
`SAMPLING_METHOD` (defined in settings) controls whether ADC SOC and EPWM INT fire at counter zero only (`SINGLE_SAMPLING`) or at both zero and period (`DOUBLE_SAMPLING`). The ISR and CLA task rate doubles in double-sampling mode.