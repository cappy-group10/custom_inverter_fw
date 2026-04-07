# EnDat Runtime Architecture

**Primary files:** `endat_init.c`, `endat_ops.c`, `endat.c`, `endat_commands.c`, `endat_globals.c`, `endat_utils.c`, `endat_shared.h`  
**Target:** TMS320F28379D / `main_foc_endat_system`  
**Last reviewed:** 2026-04-07

---

## 1. Current Checked-In Configuration

| Setting | Value | Meaning |
|---|---|---|
| `ENCODER_TYPE` | `21` | EnDat 2.1 runtime path is active today |
| `ENDAT_INIT_FREQ_DIVIDER` | `250` | Init-time clock is about `200 kHz` |
| `ENDAT_RUNTIME_FREQ_DIVIDER` | `6` | Runtime clock is about `8.33 MHz` |
| `ENDAT_PRODUCER_RATE_RATIO` | `3` | `EPWM9` producer runs at `30 kHz` |
| `ENDAT_PRODUCER_TIMEOUT_TICKS` | `4` | Timeout recovery after four producer ticks |
| `ENDAT_APPLY_DEFAULT_OFFSET` | defined | Saved offset is applied during boot |
| `ENDAT_POSITION_OFFSET_PU` | `0.677F` | Saved raw-position offset in per-unit |

Important implication:

- The checked-in runtime is position-only EnDat 2.1.
- The EnDat 2.2 additional-data helpers are still present, but they are not active in the current build because `ENCODER_TYPE != 22`.

---

## 2. Module Structure

The EnDat stack is split into a blocking bring-up path and a fast runtime producer:

| File | Responsibility |
|---|---|
| `endat_init.c` | GPIO/XBAR/EPWM4/SPI-B setup, power-up sequence, delay compensation |
| `endat_ops.c` | Blocking reads, producer state machine, published-snapshot helpers, offset and direction handling |
| `endat.c` | `spiRxFifoIsr()` plus `endatProducerISR()` |
| `endat_commands.c` | Bring-up command-set validation helpers |
| `endat_globals.c` | Shared PM library objects and counters |
| `endat_utils.c` | CRC helper |
| `endat_shared.h` | Shared runtime snapshot types used by CPU and CLA |

---

## 3. Published Runtime State

The fast path no longer exposes a one-consumer latch. Instead it publishes decoded samples into a shared double buffer:

```text
gEndatPositionSamples[2]
|
+- rawPosition
+- mechThetaPu
+- elecThetaPu
+- sampleCounter
\- valid

gEndatRuntimeState
|
+- activeIndex
+- readPending
+- frameReady
+- timeoutTicks
+- publishCount
+- crcFailCount
+- timeoutCount
+- positionDirection
+- positionClocks
+- rawPositionScalePu
+- rawPositionOffsetPu
\- offsetValid
```

Important behavior:

- The producer writes the inactive slot first and flips `activeIndex` last.
- Both the CPU and CLA consume the same published snapshot.
- Offset subtraction happens before direction inversion in `endat_ops.c`.

---

## 4. Initialization Sequence

The effective bring-up sequence from `main()` is:

```text
main()
|
+-[1]  EnDat_Init()
|       +- enable EPWM1-4 clocks
|       +- configure EPWM4 idle clock state
|       +- build CRC table
|       +- configure GPIOs and InputXBAR
|       +- configure SPI-B and register SPI RX ISR
|       +- power-cycle and reset the encoder
|       \- read `position_clocks`
|
+-[2]  endat21_runCommandSet()
|
+-[3]  [only if ENCODER_TYPE == 22] endat22_setupAddlData()
|
+-[4]  EnDat_initDelayComp()
|
+-[5]  PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER)
|
+-[6]  endat21_initProducer(polePairs)
+-[7]  endat21_setPositionDirection(speedDirection)
+-[8]  endat21_readPosition()            // publish one valid sample immediately
+-[9]  [if enabled] endat21_setPositionOffset(ENDAT_POSITION_OFFSET_PU)
+-[10] endatInitDone = 1
\-[11] endat21_startProducer()
```

Two important notes about the current startup behavior:

1. The code performs one blocking position read before interrupts are enabled, so consumers do not start from an empty snapshot.
2. The saved offset is applied before the normal producer cadence begins.

---

## 5. Runtime Execution Model

Runtime EnDat is a producer/consumer pipeline:

- `EPWM9` drives a non-blocking producer ISR at `30 kHz`.
- `spiRxFifoIsr()` stays minimal and only captures the just-finished frame.
- `Cla1Task1()` performs the PWM-edge handoff into FCL fields.
- `motor1ControlISR()` mirrors the same published sample into CPU-visible motor variables.

### 5.1 Producer flow

```text
EPWM9 interrupt:
    endatProducerISR()
        -> endat21_runProducerTick()
             if readPending == 0:
                 arm ENCODER_SEND_POSITION_VALUES
                 PM_endat22_startOperation()
                 readPending = 1

             else if frameReady == 1:
                 PM_endat22_receiveData()
                 CRC validate
                 decode rawPosition / mechThetaPu / elecThetaPu
                 publish to inactive slot
                 flip activeIndex

             else:
                 increment timeoutTicks
                 if timeoutTicks reaches ENDAT_PRODUCER_TIMEOUT_TICKS:
                     drop stalled read
                     increment timeout counter
```

### 5.2 SPI RX ISR flow

```text
spiRxFifoIsr():
    drain SPI-B RX FIFO into endat22Data.rdata[]
    clear RX FIFO flags
    set endat22Data.dataReady = 1
    set gEndatRuntimeState.frameReady = 1
    ACK PIE group 6
```

### 5.3 Consumer flow

```text
EPWM1 -> Cla1Task1():
    consume the currently published snapshot
    copy mech/electrical theta into FCL-compatible fields
    keep the handoff PWM-synchronous

EPWM1 -> motor1ControlISR():
    updateMotorPositionFeedback()
        -> endat21_getPublishedPosition()
        -> mirror the same sample into motorVars[0]
        -> update endatPosRaw and counters
```

---

## 6. Timing Relationship

For the checked-in build:

- `M1_PWM_FREQUENCY = 10 kHz`
- `SAMPLING_METHOD = SINGLE_SAMPLING`
- `ENDAT_PRODUCER_RATE_RATIO = 3`

That yields:

- `EPWM1` / FCL path at `10 kHz`
- `EPWM9` producer path at `30 kHz`

So the producer attempts up to three EnDat transactions per PWM period, while the motor-control consumers still use exactly one PWM-synchronous sample handoff per `EPWM1` cycle.

If the PWM frequency, sampling method, or producer ratio changes, re-check:

- `ENDAT_PRODUCER_PWM_TICKS`
- `ENDAT_PRODUCER_PHASE_TICKS`
- the expected producer-to-consumer timing relationship

---

## 7. Hardware Resource Map

### SPI-B data path

| Resource | Assignment | Notes |
|---|---|---|
| SPI peripheral | `SPI-B` | RX FIFO interrupt driven |
| GPIO63 | `SPISIMOB` | Also routed through InputXBAR |
| GPIO64 | `SPISOMIB` | Encoder data in |
| GPIO65 | `SPICLKB` | SPI clock |
| GPIO66 | `SPISTEB` | Chip select |
| PIE group 6 / INT3 | `spiRxFifoIsr()` | SPI RX interrupt |

### EPWM resources

| Resource | Role | Notes |
|---|---|---|
| `EPWM4A` / GPIO6 | EnDat clock master | Driven during bring-up and runtime clock generation |
| `EPWM4B` / GPIO7 | Clock companion output | Part of the TI EnDat setup |
| `EPWM9` | Runtime producer scheduler | Internal time base only, not intended as an external output |

### Other GPIO

| GPIO | Role | Notes |
|---|---|---|
| GPIO9 | EnDat TxEN | RS-485 direction control |
| GPIO139 | EnDat 5 V enable | Powered during `EnDat_Init()` |

---

## 8. Failure Behavior

| Condition | Runtime result |
|---|---|
| `endatInitDone == 0` | CPU mirror path returns early; existing state is preserved |
| CRC failure | CRC fail counter increments; no new sample is published |
| Producer timeout | Timeout counter increments; the stalled read is dropped and the next `EPWM9` tick can re-arm a new read |
| No valid published sample yet | Consumers keep previous state until a valid sample exists |
| `position_clocks == 0` | Decode fails and publishing is skipped |

Init-time CRC failures remain fatal and still land in `ESTOP0`.

---

## 9. Integration With Motor Control

The runtime responsibilities are intentionally split:

| Context | Responsibility |
|---|---|
| `endatProducerISR()` | Acquire, CRC-check, decode, publish |
| `Cla1Task1()` | Latch the published angle into FCL-compatible fields at the PWM edge |
| `motor1ControlISR()` | Mirror the same published sample into `motorVars[0]` for speed estimation and observability |

Consequences:

- No EnDat CRC or frame decode work stays on the CPU's tight FCL timing edge.
- The FCL still sees a PWM-synchronous angle handoff.
- The CPU and CLA always observe the same published sample stream.

---

## 10. Notes and Constraints

### The checked-in build is EnDat 2.1

The 2.2 helpers are still useful for bring-up or future work, but the active runtime path is EnDat 2.1 because `ENCODER_TYPE == 21`.

### Default offset is part of normal runtime

Offset handling is not only a calibration-time concept anymore. The checked-in boot flow applies `ENDAT_POSITION_OFFSET_PU`, and `endatDecodePositionSample()` subtracts it from every published sample when `offsetValid != 0`.

### The snapshot model is multi-consumer

CPU and CLA both read the same double-buffered snapshot. Readers must treat `activeIndex` as the commit point.

### Blocking reads are startup-only

`endat21_readPosition()` and `endat22_readPositionWithAddlData()` are fine during controlled bring-up, but they are not appropriate for the normal ISR path once the producer is active.
