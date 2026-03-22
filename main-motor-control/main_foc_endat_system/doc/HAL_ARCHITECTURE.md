# Hardware Abstraction Layer (HAL) - Architecture & Execution Map

**File:** `dual_axis_servo_drive_hal.c`  
**Target:** TI F2837x (C2000) - Dual-Axis Servo Drive
**Last reviewed:** 2026

---

## 1. Object Hierarchy

```text
HAL_Obj  (halHandle / hal)
|
+- dacHandle[3]       -> DACA, DACB, DACC
+- claHandle          -> CLA1
+- sciHandle[2]       -> SCIA, SCIB
+- timerHandle[3]     -> CPUTIMER0, CPUTIMER1, CPUTIMER2
+- adcHandle[4]       -> ADCA, ADCB, ADCC, ADCD
\- adcResult[4]       -> ADCARESULT .. ADCDRESULT

HAL_MTR_Obj  (halMtrHandle[2] / halMtr[2])
|
+- spiHandle          -> M1_SPI_BASE  (MTR_1 only)
+- pwmHandle[3]       -> U/V/W phase PWM bases
+- cmpssHandle[3]     -> U/V/W phase CMPSS bases
\- qepHandle          -> M1_QEP_BASE when QEP feedback is used
```

---

## 2. Initialization Sequence

This is the effective initialization order as used by `main()`:

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
+-[4]  SysCtl_disablePeripheral(TBCLKSYNC)
|
+-[5]  HAL_setParams()
|       |
|       +- set LSPCLK
|       +- init GPIO
|       +- setup CLA memory, vectors, and triggers
|       +- init interrupt module and vector table
|       +- setup CPU timers
|       +- setup GPIOs
|       +- setup DACs when enabled
|       \- setup ADCs and PPBs
|
+-[6]  HAL_setMotorParams(MTR_1)
|       |
|       +- HAL_setupMotorPWMs()
|       |    +- configure EPWM1/2/3 for motor phase switching
|       |    \- configure EPWM9 as the EnDat producer scheduler
|       +- HAL_setupCMPSS()
|       \- HAL_setupQEP() when QEP feedback is selected
|
+-[7]  SysCtl_enablePeripheral(TBCLKSYNC)
|
+-[8]  initMotorParameters()
+-[9]  initControlVars()
+-[10] HAL_setupMotorFaultProtection()
+-[11] resetControlVars()
+-[12] HAL_clearTZFlag()
|
+-[13] HAL_setupInterrupts()
|       |
|       +- register motor1ControlISR on EPWM1 INT
|       +- register endatProducerISR on EPWM9 INT
|       +- configure EPWM1 interrupt cadence for current control
|       \- configure EPWM7 interrupt cadence for EnDat runtime scheduling
|
+-[14] runOffsetsCalculation()
|
+-[15] EnDat init sequence
|       \- documented in ENDAT_ARCHITECTURE.md
|
+-[16] HAL_enableInterrupts()
|       |
|       +- clear EPWM1 interrupt flag
|       +- clear EPWM9 interrupt flag
|       +- enable EPWM1 INT in PIE group 3
|       +- enable EPWM9 INT in PIE group 3
|       \- enable CPU INT3
|
\-[17] EINT / ERTM
```

---

## 3. Function Reference

| Function | Called From | Purpose |
|---|---|---|
| `HAL_init()` | `main()` | Populate the system HAL object |
| `HAL_MTR_init()` | `main()` | Populate the per-motor HAL object |
| `HAL_setParams()` | `main()` | Configure CLA, GPIO, ADC, timers, and optional DAC |
| `HAL_setMotorParams()` | `main()` | Configure motor-specific PWM, CMPSS, and optional QEP |
| `HAL_setupCLA()` | `HAL_setParams()` | Configure CLA RAM ownership, vectors, and task triggers |
| `HAL_setupADCs()` | `HAL_setParams()` | Configure ADC modules, SOCs, and PPBs |
| `HAL_setupGPIOs()` | `HAL_setParams()` | Configure board-level pin muxing and directions |
| `HAL_setupDACs()` | `HAL_setParams()` | Configure DAC outputs when enabled |
| `HAL_setupMotorPWMs()` | `HAL_setMotorParams()` | Configure EPWM1/2/3 for motor drive and EPWM9 for EnDat scheduling |
| `HAL_setupEndatProducerPWM()` | `HAL_setupMotorPWMs()` | Configure EPWM9 as an internal `40 kHz` EnDat scheduler |
| `HAL_setupCMPSS()` | `HAL_setMotorParams()` | Configure current protection comparators |
| `HAL_setupQEP()` | `HAL_setMotorParams()` | Configure QEP hardware when selected |
| `HAL_setupMotorFaultProtection()` | `main()` | Configure XBAR and trip-zone motor fault handling |
| `HAL_setupInterrupts()` | `main()` | Register ISR vectors and configure EPWM interrupt sources |
| `HAL_enableInterrupts()` | `main()` | Enable EPWM interrupts and CPU interrupt group 3 |

---

## 4. Dependency Graph

```text
HAL_init()
    -> HAL_setParams()
         +- HAL_setupCLA()
         +- HAL_setupCpuTimer() x3
         +- HAL_setupGPIOs()
         +- HAL_setupDACs() when enabled
         \- HAL_setupADCs()

HAL_MTR_init()
    -> HAL_setMotorParams()
         +- HAL_setupMotorPWMs()
         |    \- HAL_setupEndatProducerPWM()
         +- HAL_setupCMPSS()
         \- HAL_setupQEP() when POSITION_ENCODER == QEP_POS_ENCODER

After base init:
    HAL_setupMotorFaultProtection()
    HAL_setupInterrupts()
    HAL_enableInterrupts()

Runtime:
    EPWM1 INT -> motor1ControlISR()
    EPWM1 INT -> Cla1Task1()
    EPWM9 INT -> endatProducerISR()
```

---

## 5. Hardware Resource Map

### Motor PWM resources

| EPWM | Role | Notes |
|---|---|---|
| EPWM1 | Motor 1 phase U + control-loop interrupt source | Also triggers `Cla1Task1` |
| EPWM2 | Motor 1 phase V | Standard inverter PWM |
| EPWM3 | Motor 1 phase W | Standard inverter PWM |
| EPWM4 | Motor 2 / EnDat clock generation path | Board-specific reuse; see EnDat doc |
| EPWM5 | Motor 2 phase V | Reserved in this codebase |
| EPWM6 | Motor 2 phase W | Reserved in this codebase |
| EPWM9 | EnDat runtime scheduler | Internal `40 kHz` producer time base |

### ADC channels for Motor 1

| Signal | ADC module | Trigger |
|---|---|---|
| Phase U current | ADCC | EPWM1 SOCA |
| Phase V current | ADCB | EPWM1 SOCA |
| Phase W current | ADCA | EPWM1 SOCA |
| DC bus voltage | ADCD | EPWM1 SOCA |

### Interrupt ownership

| Source | PIE group | Consumer |
|---|---|---|
| EPWM1 INT | Group 3 / INT1 | `motor1ControlISR()` and CLA Task 1 trigger |
| EPWM9 INT | Group 3 / INT9 | `endatProducerISR()` |
| SPIB RX INT | Group 6 / INT3 | `spiRxFifoIsr()` |

---

## 6. ISR & CLA Task Map

### CPU ISRs

| ISR | Trigger | Responsibility |
|---|---|---|
| `motor1ControlISR()` | EPWM1 INT | Current-loop CPU work, speed estimator input mirror, build-level flow |
| `endatProducerISR()` | EPWM9 INT | Independent EnDat runtime producer scheduler |
| `spiRxFifoIsr()` | SPI-B RX FIFO | Capture the completed EnDat frame and flag it ready |

### CLA tasks

| Task | Trigger | Responsibility |
|---|---|---|
| `Cla1Task1` | EPWM1 INT | Latch QEP or published EnDat position into FCL state |
| `Cla1Task2` | Software IACK | FCL motor-1 step 2 |
| `Cla1Task3` | Software IACK | FCL motor-1 step 3 |
| `Cla1Task4` | Software IACK | FCL motor-1 step 4 |
| `Cla1Task5` | EPWM4 INT | FCL motor-2 step 1 |
| `Cla1Task6` | Software IACK | FCL motor-2 step 2 |
| `Cla1Task7` | Software IACK | FCL motor-2 step 3 |
| `Cla1Task8` | Software IACK | FCL motor-2 step 4 |

For EnDat builds, `Cla1Task1` takes ownership of the PWM-edge position handoff when `ptrQEP == 0`. That keeps the FCL dependency PWM-synchronous even though the encoder producer is running independently.

---

## 7. GPIO Notes

| GPIO | Function | Notes |
|---|---|---|
| 0-5 | Motor 1 inverter PWM | EPWM1/2/3 outputs |
| 6-7 | EPWM4A/B | Used by the EnDat clock-generation path |
| 20/21/99 | QEP1 | Only relevant for QEP feedback builds |
| 63-66 | SPI-B | EnDat serial interface |
| 139 | EnDat 5V enable / conflicting board function | Needs board-level confirmation |
| 157-160 | EPWM7/8 debug outputs | Only muxed when `DACOUT_EN` is enabled |

`EPWM7` and `EPWM8` remain available for DAC debug outputs because the EnDat producer scheduler now uses `EPWM9`.

---

## 8. Notes & Constraints

### FCL dependency

The FCL still depends on a PWM-edge position update, but it no longer depends on CPU-side EnDat decode timing. The handoff now happens inside `Cla1Task1`, which is already synchronized to `EPWM1INT`.

### EPWM9 ownership

`HAL_setupEndatProducerPWM()` configures EPWM9 as an internal time base only. It is not intended to drive external pins in the EnDat configuration.

### Single-sampling assumption

The current build uses `SAMPLING_METHOD = SINGLE_SAMPLING`, so EPWM1 INT, `Cla1Task1`, and `motor1ControlISR()` all run at `10 kHz`, while EPWM9 runs at `40 kHz`.

### Existing board-level conflicts still apply

GPIO139 still has conflicting meanings across parts of the codebase, and the old GPIO156/GPIO139 setup issue remains in `HAL_setupGPIOs()`. Those are pre-existing HAL concerns and were not changed by the EnDat producer work.
