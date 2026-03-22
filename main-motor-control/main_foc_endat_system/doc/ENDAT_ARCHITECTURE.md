# EnDat Encoder Interface — Architecture & Execution Map

**Files:** `endat_init.c`, `endat_ops.c`, `endat.c` (+ `endat_globals.c`, `endat_utils.c`, `endat_commands.c`)  
**Target:** TMS320F28379D (C2000 F2837x)  
**Protocol:** EnDat 2.1 / EnDat 2.2  
**Last reviewed:** 2025

---

## Table of Contents

1. [Module Structure](#1-module-structure)
2. [Data Flow & State Model](#2-data-flow--state-model)
3. [Initialization Sequence](#3-initialization-sequence)
4. [Runtime Execution Model](#4-runtime-execution-model)
5. [Function Reference](#5-function-reference)
6. [Dependency Graph](#6-dependency-graph)
7. [Hardware Resource Map](#7-hardware-resource-map)
8. [Command Sequence Reference](#8-command-sequence-reference)
9. [Integration with Motor Control ISR](#9-integration-with-motor-control-isr)
10. [Known Issues / Notes](#10-known-issues--notes)

---

## 1. Module Structure

The EnDat driver is split across six source files. Each file has a single responsibility:

| File | Responsibility | Public API |
|---|---|---|
| `endat_init.c` | Hardware init, GPIO/XBAR/EPWM config, power-on sequence, delay compensation | `EnDat_Init()`, `EnDat_initDelayComp()` |
| `endat_ops.c` | Position read operations (blocking, scheduled, feedback retrieval) | `endat21_readPosition()`, `endat21_schedulePositionRead()`, `endat21_servicePositionRead()`, `endat21_getPositionFeedback()`, `endat22_setupAddlData()`, `endat22_readPositionWithAddlData()` |
| `endat.c` | SPI-B RX FIFO interrupt service routine | `spiRxFifoIsr()` (registered, not called directly) |
| `endat_globals.c` | Shared variable definitions (`endat22Data`, `endat22CRCtable`, `gEndatCrcFailCount`) | — |
| `endat_utils.c` | CRC comparison helper (`CheckCRC`) | `CheckCRC()` |
| `endat_commands.c` | EnDat 2.1/2.2 command set templates | `endat21_runCommandSet()` |

> Static helpers in `endat_init.c` (`EPWM4_Config`, `Endat_setup_GPIO`, `Endat_config_XBAR`) are private to that file and are not part of the public API.

---

## 2. Data Flow & State Model

### Shared Data Structures

```
endat22Data  (global struct, defined in endat_globals.c)
│
├── spi              → pointer to SpibRegs
├── dataReady        → set to 1 by spiRxFifoIsr, consumed by service/receive functions
├── fifo_level       → number of SPI words to drain from FIFO
├── rdata[]          → raw SPI RX buffer (filled by ISR)
├── position_lo/hi   → raw position word (unpacked from rdata by PM_endat22_receiveData)
├── position_clocks  → encoder bit-width, read from encoder address 0xD during init
├── error1 / error2  → encoder error/warning bits
├── data_crc         → received CRC from position frame
├── address / data   → parameter read results (used during init)
├── additional_data1/2       → additional data channels (EnDat 2.2 only)
├── additional_data1/2_crc   → CRC for additional data channels
└── delay_comp       → propagation delay (set by EnDat_initDelayComp)

endat22CRCtable[32]  → CRC-5 lookup table (POLY1, built during EnDat_Init)
gEndatCrcFailCount   → rolling CRC failure counter (global, readable by application)
```

### File-Private State (`endat_ops.c`)

```
sRawPosition    (uint32_t)   → most recent valid raw position count
sMechThetaPu    (float32_t)  → mechanical angle in per-unit [0, 1)
sElecThetaPu    (float32_t)  → electrical angle in per-unit [0, 1)
sDataValid      (uint16_t)   → 1 = fresh data available; cleared on read by getPositionFeedback()
sReadPending    (uint16_t)   → 1 = a scheduled transaction is in flight
```

> `sDataValid` acts as a single-sample latch — it is set after a successful CRC and cleared when `endat21_getPositionFeedback()` consumes the data. Back-to-back calls to `getPositionFeedback()` without a new read will return `false`.

---

## 3. Initialization Sequence

Called once from `main()` before interrupts are enabled. The sequence has hard timing constraints specified by the EnDat protocol.

```
main()
│
├─[1]  EnDat_Init()
│       │
│       ├── Enable EPWM1–4 clocks (CpuSysRegs.PCLKCR2)
│       │
│       ├── EPWM4_Config()
│       │     └── Force EPWM4A/B outputs high (idle CLK state for EnDat)
│       │
│       ├── PM_endat22_generateCRCTable(NBITS_POLY1, POLY1, endat22CRCtable)
│       │     └── Builds 32-entry CRC-5 lookup table
│       │
│       ├── Endat_setup_GPIO()
│       │     ├── GPIO6  → EPWM4A (EnDat CLK master)
│       │     ├── GPIO7  → EPWM4B (SPI CLK slave)
│       │     ├── GPIO63 → SPISIMOB  (GMUX=3 + MUX=3, async qual)
│       │     ├── GPIO64 → SPISOMIB  (GMUX=3 + MUX=3, async qual)
│       │     ├── GPIO65 → SPICLKB   (GMUX=3 + MUX=3, async qual)
│       │     ├── GPIO66 → SPISTEB   (GMUX=3 + MUX=3, async qual)
│       │     ├── GPIO9  → EnDat TxEN (MUX=3)
│       │     └── GPIO139→ EnDat 5V power control (output)
│       │
│       ├── Endat_config_XBAR()
│       │     └── InputXBAR Input1 ← GPIO63 (SPISIMOB → GPTRIP TRIP1)
│       │
│       ├── endat22Data.spi = &SpibRegs
│       ├── PM_endat22_setupPeriph()          ← configure SPI-B peripheral
│       │
│       ├── Register spiRxFifoIsr in PIE vector table
│       │     ├── PieVectTable.SPIB_RX_INT = &spiRxFifoIsr
│       │     ├── Enable PIE group 6 INT3 (SPI-B RX)
│       │     └── Enable CPU INT6
│       │
│       ├── ── Power-on timing sequence ──────────────────────────────
│       ├── GPIO139 = 1  (5V supply ON)
│       ├── DELAY_US(10000)                   10 ms  — supply ramp
│       │
│       ├── GPIO6 → plain output, set high    (CLK idle high)
│       ├── DELAY_US(100000)                  100 ms
│       ├── GPIO6 = 0                         (CLK pulse low, >125 ns)
│       ├── DELAY_US(425000)                  425 ms — encoder reset
│       ├── GPIO6 → restore EPWM4A mux
│       ├── DELAY_US(425000)                  425 ms — encoder ready (>381 ms)
│       │
│       ├── Assert GPIO63 (EncData) == 0      ← ESTOP0 if still high
│       │
│       ├── PM_endat22_setFreq(ENDAT_INIT_FREQ_DIVIDER)
│       │
│       ├── ── EnDat command sequence ───────────────────────────────
│       ├── ENCODER_RECEIVE_RESET (×2)        per EnDat spec requirement
│       │     DELAY_US(1000000) after 1st     1 s
│       │     DELAY_US(2000) after 2nd
│       │
│       ├── SELECTION_OF_MEMORY_AREA (MRS=0xA1)
│       │     └── CRC check → ESTOP0 on fail
│       │     DELAY_US(200)
│       │
│       └── ENCODER_SEND_PARAMETER (addr=0xD) — read position bit-width
│             └── endat22Data.position_clocks = endat22Data.data & 0xFF
│             DELAY_US(200)
│
├─[2]  endat21_runCommandSet()
│       └── Exercises basic EnDat 2.1 command set (diagnostics/validation)
│
├─[3]  endat22_setupAddlData()
│       ├── SEND_POSITION + SELECT_MEMORY_AREA  (MRS=0xA1, addl=0)  → CRC check
│       ├── SEND_POSITION + SEND_PARAMETER      (addr=0xD, addl=0)  → CRC check
│       ├── SEND_POSITION + SELECT_MEMORY_AREA  (MRS=0x45, addl=0)  → CRC check (enable addl data 1)
│       └── SEND_POSITION + SELECT_MEMORY_AREA  (MRS=0x59, addl=1)  → CRC check (enable addl data 2)
│             DELAY_US(2000) final
│
├─[4]  EnDat_initDelayComp()               ← must run at low freq (~200 kHz)
│       ├── Two blocking ENCODER_SEND_POSITION_VALUES reads
│       ├── CRC check each → ESTOP0 on fail
│       └── endat22Data.delay_comp = (delay1 + delay2) >> 1
│
├─[5]  PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER)
│             ← switch to higher runtime frequency after delay cal
│
├─[6]  DELAY_US(800)
│
├─[7]  endatInitDone = 1                   ← gate flag released
│
└─[8]  endat21_schedulePositionRead()      ← prime first non-blocking read
```

**Total blocking time at startup:** approximately 2 × 1000 ms reset + 2 × 425 ms power-on = ~2.85 s minimum, before any control loop runs.

---

## 4. Runtime Execution Model

EnDat uses a **split-phase, non-blocking** model at runtime. The transaction is split across two motor ISR cycles to avoid blocking the control loop:

```
PWM ISR cycle N:
    serviceEndatPositionAcquisition()
        └── endat21_schedulePositionRead()
              ├── Guard: return early if sReadPending != 0
              ├── endat22Data.dataReady = 0
              ├── PM_endat22_setupCommand(ENCODER_SEND_POSITION_VALUES, ...)
              └── PM_endat22_startOperation()   ← kicks off SPI-B DMA/FIFO TX
                        sReadPending = 1

        [SPI transaction in progress — SPI-B clocks out request to encoder]

SPI-B RX FIFO fills → spiRxFifoIsr() fires (PIE group 6, INT3):
    ├── Drain FIFO: rdata[0..fifo_level] = SPIRXBUF
    ├── Clear RXFFOVF + RXFFINT flags
    ├── endat22Data.dataReady = 1
    └── PIE ACK group 6

PWM ISR cycle N+1:
    updateMotorPositionFeedback()
        └── endat21_servicePositionRead()
              ├── Guard: return if sReadPending==0 or dataReady!=1
              ├── PM_endat22_receiveData(ENCODER_SEND_POSITION_VALUES, 0)
              │     └── Unpacks rdata[] → position_lo/hi, error1/2, data_crc
              ├── endatValidatePositionCrc()
              │     └── PM_endat22_getCrcPos() → CheckCRC()
              │           on fail: gEndatCrcFailCount++, sReadPending=0, return
              ├── endatGetPositionRaw()
              │     └── raw = (position_hi<<16 | position_lo) & bitmask
              ├── sMechThetaPu = raw / (1 << position_clocks)
              ├── sDataValid = 1
              └── sReadPending = 0

        endat21_getPositionFeedback(mechTheta, elecTheta, rawPos, polePairs)
              ├── Guard: return false if sDataValid==0 or position_clocks==0
              ├── *mechThetaPu = sMechThetaPu
              ├── *rawPosition = sRawPosition
              ├── sElecThetaPu = fmodf(sMechThetaPu * polePairs)
              ├── *elecThetaPu = sElecThetaPu
              ├── sDataValid = 0                ← latch consumed
              └── return true

        → writes to pMotor->posMechTheta, posElecTheta, ptrFCL->qep.*, ptrFCL->pangle
```

### Timing Diagram

```
ISR cycle:  |    N    |   N+1   |   N+2   |   N+3   |
            |---------|---------|---------|---------|
Schedule:   | ▶ TX    |         | ▶ TX    |         |
SPI ISR:    |    [RX] |         |    [RX] |         |
Service:    |         | ▶ unpack|         | ▶ unpack|
Feedback:   |         | ▶ valid |         | ▶ valid |
```

One complete position update is available every **2 PWM ISR cycles**. If a CRC failure occurs in cycle N+1, the previous valid position is retained (motor ISR returns `false`, prior `pMotor` values unchanged).

---

## 5. Function Reference

### Public API

| Function | File | Blocking? | Purpose |
|---|---|---|---|
| `EnDat_Init()` | `endat_init.c` | Yes (~2.85 s) | Full hardware init, power-on sequence, encoder reset, read position bit-width |
| `EnDat_initDelayComp()` | `endat_init.c` | Yes | 2-sample propagation delay measurement; sets `endat22Data.delay_comp` |
| `endat21_runCommandSet()` | `endat_commands.c` | Yes | Exercises EnDat 2.1 command set for validation |
| `endat22_setupAddlData()` | `endat_ops.c` | Yes | Configures encoder to send 2 additional data words with each position frame |
| `endat22_readPositionWithAddlData()` | `endat_ops.c` | Yes | Single blocking read of position + 2 additional data channels with CRC check |
| `endat21_readPosition()` | `endat_ops.c` | Yes | Single blocking position read, updates `sRawPosition` / `sMechThetaPu` |
| `endat21_schedulePositionRead()` | `endat_ops.c` | No | Starts SPI transaction; returns immediately. Re-entrant guard via `sReadPending` |
| `endat21_servicePositionRead()` | `endat_ops.c` | No | If `dataReady`, unpacks and CRC-validates the pending frame |
| `endat21_getPositionFeedback()` | `endat_ops.c` | No | Returns latest valid `mechTheta`, `elecTheta`, `rawPosition`; clears `sDataValid` |
| `spiRxFifoIsr()` | `endat.c` | ISR | Drains SPI-B RX FIFO, sets `dataReady = 1` |

### Private Helpers (`endat_init.c`)

| Function | Purpose |
|---|---|
| `EPWM4_Config()` | Forces EPWM4A/B high via TZ OST — idle CLK state |
| `Endat_setup_GPIO()` | GPIO mux assignments for CLK, SPI-B, TxEN, power control |
| `Endat_config_XBAR()` | Routes GPIO63 (SPISIMOB) to InputXBAR Input1 |

### Private Helpers (`endat_ops.c`)

| Function | Purpose |
|---|---|
| `endatGetType()` | Returns `ENDAT22` or `ENDAT21` based on `ENCODER_TYPE` define |
| `endatGetPositionRaw()` | Reconstructs and masks raw position from `position_lo/hi` |
| `endatValidatePositionCrc()` | Calls `PM_endat22_getCrcPos()` + `CheckCRC()`; returns 1 on pass |

---

## 6. Dependency Graph

```
EnDat_Init()
    ├── EPWM4_Config()
    ├── PM_endat22_generateCRCTable()     ← must precede any CRC operations
    ├── Endat_setup_GPIO()
    ├── Endat_config_XBAR()
    └── PM_endat22_setupPeriph()          ← SPI-B peripheral config

        ──── EnDat_Init() must complete before any ops ────

endat21_runCommandSet()                   depends on: EnDat_Init()
endat22_setupAddlData()                   depends on: EnDat_Init()
EnDat_initDelayComp()                     depends on: EnDat_Init(), position_clocks set
PM_endat22_setFreq(RUNTIME)               depends on: EnDat_initDelayComp()

        ──── Runtime (ISR context) ────────────────────────

endat21_schedulePositionRead()            depends on: endatInitDone==1, PM_endat22_setFreq(RUNTIME)
    └── [SPI transaction] ──► spiRxFifoIsr()
endat21_servicePositionRead()             depends on: dataReady==1 (set by ISR)
    └── endatValidatePositionCrc()
          └── endatGetPositionRaw()
endat21_getPositionFeedback()             depends on: sDataValid==1 (set by servicePositionRead)
```

---

## 7. Hardware Resource Map

### SPI-B (EnDat data interface)

| Resource | Assignment | Notes |
|---|---|---|
| Peripheral | SPI-B (`SpibRegs`) | Full-duplex; RX FIFO interrupt driven |
| MOSI (TX) | GPIO63 | SPISIMOB, GMUX=3/MUX=3, async qual |
| MISO (RX) | GPIO64 | SPISOMIB, async qual |
| CLK | GPIO65 | SPICLKB, async qual |
| CS | GPIO66 | SPISTEB, async qual |
| PIE interrupt | Group 6, INT3 | `SPIB_RX_INT` → `spiRxFifoIsr` |
| CPU interrupt | INT6 (`IER |= 0x20`) | Enabled in `EnDat_Init()` |

### EPWM4 (EnDat CLK generation)

| Resource | Assignment | Notes |
|---|---|---|
| EPWM4A | GPIO6 | EnDat CLK master output |
| EPWM4B | GPIO7 | SPI CLK slave output |
| TZ action | OST → force high | Idle CLK state; configured in `EPWM4_Config()` |

> EPWM4 is also used as the PWM basis for Motor 2 inverter control (GPIO6–7 = UH/UL_M2 in the HAL). At runtime the EPWM mux takes over from GPIO mode. See HAL_ARCHITECTURE.md for the full PWM assignment table.

### Other GPIO

| GPIO | Signal | Direction | Notes |
|---|---|---|---|
| GPIO9 | EnDat TxEN | OUT | Enables RS-485 driver transmit direction |
| GPIO63 | EncData / SPISIMOB | IN | Also routed to InputXBAR Input1 for TRIP logic |
| GPIO139 | EnDat 5V power | OUT | Set high in `EnDat_Init()` to power encoder supply |

### InputXBAR

| XBAR Input | Source | Used for |
|---|---|---|
| Input1 | GPIO63 (SPISIMOB) | GPTRIP XBAR TRIP1 (encoder data line monitoring) |

### Frequency Settings

| Constant | Usage | Notes |
|---|---|---|
| `ENDAT_INIT_FREQ_DIVIDER` | Set before reset/init commands | Low frequency for reliable init |
| `ENDAT_RUNTIME_FREQ_DIVIDER` | Set after `EnDat_initDelayComp()` | Higher frequency for control loop |

Both constants are defined in `endat.h`. The switch from init to runtime frequency must happen **after** delay compensation, since `delay_comp` is measured at the low init frequency.

---

## 8. Command Sequence Reference

### Initialization Commands (in order, all blocking)

| Step | Command | MRS/Addr | Purpose | Delay after |
|---|---|---|---|---|
| 1 | `ENCODER_RECEIVE_RESET` | 0xAA | Reset encoder (1st) | 1 s |
| 2 | `ENCODER_RECEIVE_RESET` | 0xAA | Reset encoder (2nd, per spec) | 2 ms |
| 3 | `SELECTION_OF_MEMORY_AREA` | 0xA1 | Select encoder manufacturer params | 200 µs |
| 4 | `ENCODER_SEND_PARAMETER` | 0xD | Read position bit-width → `position_clocks` | 200 µs |

### Additional Data Setup Commands (EnDat 2.2, `endat22_setupAddlData`)

| Step | Command | MRS | Purpose | addl count | Delay after |
|---|---|---|---|---|---|
| 1 | `SEND_POSITION + SELECT_MEM` | 0xA1 | Manufacturer params | 0 | 200 µs |
| 2 | `SEND_POSITION + SEND_PARAM` | 0xD | Re-read position clocks | 0 | 200 µs |
| 3 | `SEND_POSITION + SELECT_MEM` | 0x45 | Acknowledge memory LSB (addl data 1) | 0 | 200 µs |
| 4 | `SEND_POSITION + SELECT_MEM` | 0x59 | Operating status (addl data 2) | 1→2 | 2 ms |

### Runtime Command

| Command | addl count | Used by |
|---|---|---|
| `ENCODER_SEND_POSITION_VALUES` | 0 | All runtime reads (`schedulePositionRead`, `readPosition`, `initDelayComp`) |
| `ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA` | 2 | `endat22_readPositionWithAddlData()` |

---

## 9. Integration with Motor Control ISR

The following shows how the EnDat driver integrates with the motor control ISR in `dual_axis_servo_drive.c`:

```
motor1ControlISR()  (every PWM period)
│
├── serviceEndatPositionAcquisition()
│     ├── Guard: return if endatInitDone == 0
│     └── endat21_schedulePositionRead()
│           └── starts SPI-B transaction (non-blocking)
│
├── [FCL current control runs here]
│
└── updateMotorPositionFeedback(MTR_1)
      ├── Guard: return false if endatInitDone == 0
      ├── endat21_servicePositionRead()
      │     └── unpacks + CRC validates completed SPI frame (if ready)
      └── endat21_getPositionFeedback(&mechTheta, &elecTheta, &raw, polePairs)
            └── on success, writes to:
                  pMotor->posMechTheta
                  pMotor->posElecTheta
                  pMotor->speed.ElecTheta
                  pMotor->ptrFCL->qep.MechTheta
                  pMotor->ptrFCL->qep.ElecTheta
                  pMotor->ptrFCL->pangle
                  endatPosRaw  (global, for debug)
```

### Position Calculation

```
raw = (position_hi << 16 | position_lo) & ((1 << position_clocks) - 1)

mechThetaPu = raw / (1 << position_clocks)        // [0.0, 1.0)

elecThetaPu = fmod(mechThetaPu * polePairs, 1.0)  // wraps each electrical cycle
```

### Failure Behaviour

| Condition | Result |
|---|---|
| `endatInitDone == 0` | `updateMotorPositionFeedback()` returns `false`; previous `pMotor` values held |
| CRC failure | `gEndatCrcFailCount++`, `sReadPending` cleared, `sDataValid` not set, previous values held |
| `sReadPending != 0` when scheduling | New schedule silently skipped; in-flight transaction completes normally |
| `dataReady != 1` when servicing | `servicePositionRead()` exits immediately; position not updated this cycle |
| `position_clocks == 0` | `getPositionFeedback()` returns `false` — encoder bit-width was never read |

---

## 10. Known Issues / Notes

### GPIO139 conflict with HAL
`endat_init.c` configures GPIO139 as an output for EnDat 5V power control. The HAL (`dual_axis_servo_drive_hal.c`) also configures GPIO139 as `nFault_M2` input. These are mutually exclusive functions on the same pin. Review hardware board revision to confirm which function is wired, and ensure only one driver configures this pin.

### `ESTOP0` on CRC failure during init
All blocking init and setup functions (`EnDat_Init`, `endat22_setupAddlData`, `EnDat_initDelayComp`) call `ESTOP0` on CRC failure. At runtime, `endat21_servicePositionRead()` increments `gEndatCrcFailCount` instead and allows the motor control loop to continue with the previous valid position. This asymmetry is intentional — init failures are fatal, runtime failures are recoverable.

### `endatInitDone` gate flag
The flag is set to `1` in `main()` after the full init sequence, and checked at the top of both `serviceEndatPositionAcquisition()` and `updateMotorPositionFeedback()`. Any reordering of the init steps in `main()` that sets this flag early will allow the ISR to run EnDat reads before the encoder is ready.

### `ENDAT_HACK` bypasses calibration wait
When `ENDAT_HACK` is defined, the `while(enableFlag == false)` spin-wait in `main()` is skipped. This also skips the full blocking init delay. Do not use in production.

### Blocking reads are interrupt-unsafe
`endat21_readPosition()` and `endat22_readPositionWithAddlData()` use `while(dataReady != 1)` spin-waits. They must not be called from ISR context or after the motor control ISR has been enabled, as they will compete with `spiRxFifoIsr()` for the `dataReady` flag. These functions are intended for use during startup only.

### Single-consumer design
`sDataValid` is a single-latch flag — `getPositionFeedback()` clears it on read. Only one consumer (the motor ISR) should call `getPositionFeedback()`. Calling it from a background task as well will cause one caller to always see stale or invalid data.

### EnDat 2.2 additional data not used in runtime loop
`endat22_setupAddlData()` and `endat22_readPositionWithAddlData()` configure and exercise the two additional data channels, but the runtime non-blocking path (`schedulePositionRead` / `servicePositionRead`) uses `ENCODER_SEND_POSITION_VALUES` only (no additional data). The additional data setup is currently init-time validation only.