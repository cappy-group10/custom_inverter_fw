# EnDat Encoder Interface - Architecture & Execution Map

**Files:** `endat_init.c`, `endat_ops.c`, `endat.c`, `endat_globals.c`, `endat_utils.c`, `endat_commands.c`, `endat_shared.h`
**Target:** TMS320F28379D (C2000 F2837x)  
**Protocol:** EnDat 2.1 / EnDat 2.2  
**Last reviewed:** 2026

---

## 1. Module Structure

The EnDat stack is split into a blocking init path and a fast runtime producer:

| File | Responsibility | Public API |
|---|---|---|
| `endat_init.c` | Hardware init, GPIO/XBAR/EPWM config, power-on sequence, delay compensation | `EnDat_Init()`, `EnDat_initDelayComp()` |
| `endat_ops.c` | Blocking reads plus the independent producer state machine and published snapshot helpers | `endat21_readPosition()`, `endat21_initProducer()`, `endat21_startProducer()`, `endat21_runProducerTick()`, `endat21_getPublishedPosition()`, `endat21_getPositionFeedback()`, `endat22_setupAddlData()`, `endat22_readPositionWithAddlData()` |
| `endat.c` | SPI-B RX FIFO ISR plus the EPWM7-driven producer scheduler ISR | `spiRxFifoIsr()`, `endatProducerISR()` |
| `endat_globals.c` | Shared variable definitions, runtime buffers, CRC and timeout counters | — |
| `endat_utils.c` | CRC comparison helper | `CheckCRC()` |
| `endat_commands.c` | EnDat 2.1/2.2 command set validation helpers | `endat21_runCommandSet()` |
| `endat_shared.h` | CLA-visible runtime snapshot types | `EndatPositionSample`, `EndatRuntimeState` |

---

## 2. Data Flow & State Model

### Library-owned state

`endat22Data` is still the raw exchange buffer shared with the TI PM EnDat library. It carries:

- SPI handle, FIFO level, raw `rdata[]`
- unpacked `position_lo` / `position_hi`
- `position_clocks`, `error1`, `error2`, `data_crc`
- parameter-read state and optional EnDat 2.2 additional data fields
- `delay_comp`

### Published runtime state

The fast path no longer exposes a single-consumer latch. Instead it publishes decoded samples into a CLA-visible double buffer:

```text
gEndatPositionSamples[2]   (ClaData)
|
+- rawPosition    -> decoded absolute position count
+- mechThetaPu    -> mechanical angle in per-unit [0, 1)
+- elecThetaPu    -> electrical angle in per-unit [0, 1)
+- sampleCounter  -> monotonically increasing publish counter
\- valid          -> sample contains a CRC-validated position

gEndatRuntimeState (ClaData)
|
+- activeIndex    -> currently published slot (0 or 1)
+- readPending    -> producer has a position transaction in flight
+- frameReady     -> SPI RX ISR has captured a complete frame
+- timeoutTicks   -> producer-side stall watchdog
+- publishCount   -> number of published samples
+- crcFailCount   -> mirrored runtime CRC failure counter
\- timeoutCount   -> mirrored runtime timeout recovery counter
```

The producer always writes the inactive slot first and flips `activeIndex` only after the new sample is fully valid. CPU and CLA readers treat `activeIndex` as the commit point.

---

## 3. Initialization Sequence

Called once from `main()` before the motor control loop is enabled:

```text
main()
|
+-[1]  EnDat_Init()
|       |
|       +- Enable EPWM1-4 clocks and configure EPWM4 idle clock generation
|       +- Build CRC table
|       +- Configure GPIOs and InputXBAR
|       +- Configure SPI-B and register spiRxFifoIsr()
|       +- Run the EnDat power-up and reset timing sequence
|       \- Read encoder position bit-width into endat22Data.position_clocks
|
+-[2]  endat21_runCommandSet()
|
+-[3]  [EnDat 2.2 only] endat22_setupAddlData()
|
+-[4]  EnDat_initDelayComp()
|
+-[5]  PM_endat22_setFreq(ENDAT_RUNTIME_FREQ_DIVIDER)
|
+-[6]  DELAY_US(800)
|
+-[7]  endat21_initProducer(polePairs)
|       \- clears the published buffers, counters, and runtime flags
|
+-[8]  endatInitDone = 1
|
\-[9]  endat21_startProducer()
        \- the first EPWM7 tick launches runtime acquisition
```

Notes:

- `endat22_setupAddlData()` is now conditional on `ENCODER_TYPE == 22`.
- No runtime read is primed from `motor1ControlISR()` anymore.
- Startup behavior remains conservative: FCL sees no EnDat angle until the first valid published sample exists.

---

## 4. Runtime Execution Model

Runtime EnDat is now a producer/consumer pipeline:

- `EPWM7` drives a `40 kHz` producer ISR.
- `spiRxFifoIsr()` stays lean and only captures the completed frame.
- `Cla1Task1()` performs the PWM-edge handoff into FCL state.
- `motor1ControlISR()` mirrors the already-published sample into CPU-side variables for speed estimation, debug, and UI.

### Producer flow

```text
EPWM7 interrupt:
    endatProducerISR()
        -> endat21_runProducerTick()
             if readPending == 0:
                 setup ENCODER_SEND_POSITION_VALUES
                 PM_endat22_startOperation()
                 readPending = 1

             else if frameReady == 1:
                 PM_endat22_receiveData()
                 CRC validate
                 decode rawPosition / mechThetaPu / elecThetaPu
                 write inactive ClaData slot
                 publish by flipping activeIndex

             else:
                 increment timeoutTicks
                 if timeoutTicks reaches ENDAT_PRODUCER_TIMEOUT_TICKS:
                     drop stalled read
                     increment gEndatTimeoutCount
```

### SPI RX ISR flow

```text
spiRxFifoIsr():
    drain FIFO into endat22Data.rdata[]
    clear RX FIFO flags
    endat22Data.dataReady = 1
    gEndatRuntimeState.frameReady = 1
    ACK PIE group 6
```

### Consumer flow

```text
EPWM1 -> Cla1Task1():
    if ptrQEP == 0 and lsw != ENC_ALIGNMENT:
        latch the active EnDat slot
        copy mech/electrical theta into fclVars[0].qep.*
        set pangle = qep.ElecTheta
    else if lsw == ENC_ALIGNMENT:
        force qep and pangle to zero
    else:
        keep the previous FCL angle state

EPWM1 -> motor1ControlISR():
    updateMotorPositionFeedback(MTR_1)
        -> endat21_getPublishedPosition()
        -> mirror mech/electrical theta into motorVars[0]
        -> update endatPosRaw and endatCrcFailCount
```

### Timing relationship

For the current build:

- `M1_PWM_FREQUENCY = 10 kHz`
- `SAMPLING_METHOD = SINGLE_SAMPLING`
- `ENDAT_PRODUCER_RATE_RATIO = 4`

That yields a `40 kHz` EnDat producer while FCL remains `10 kHz` and PWM-synchronous. The producer therefore attempts up to four EnDat transactions per PWM period, but FCL still consumes exactly one latched position at each PWM edge.

If `SAMPLING_METHOD` is changed to `DOUBLE_SAMPLING`, the producer will still run at `4x` the PWM carrier frequency, not `4x` the CPU ISR rate, until the scheduler constants are revisited.

---

## 5. Function Reference

### Public API

| Function | File | Blocking? | Purpose |
|---|---|---|---|
| `EnDat_Init()` | `endat_init.c` | Yes | Full hardware init, power-on sequence, encoder reset, read position bit-width |
| `EnDat_initDelayComp()` | `endat_init.c` | Yes | Two-sample propagation delay measurement |
| `endat21_runCommandSet()` | `endat_commands.c` | Yes | EnDat command-set validation during bring-up |
| `endat22_setupAddlData()` | `endat_ops.c` | Yes | Enables the two EnDat 2.2 additional data channels |
| `endat22_readPositionWithAddlData()` | `endat_ops.c` | Yes | Single blocking read of position plus additional data |
| `endat21_readPosition()` | `endat_ops.c` | Yes | Single blocking position read, then publishes one snapshot |
| `endat21_initProducer()` | `endat_ops.c` | No | Clears shared buffers, counters, and runtime state |
| `endat21_startProducer()` | `endat_ops.c` | No | Arms runtime acquisition after init |
| `endat21_runProducerTick()` | `endat_ops.c` | No | One producer scheduler step |
| `endat21_getPublishedPosition()` | `endat_ops.c` | No | Returns a stable copy of the active published sample |
| `endat21_getPositionFeedback()` | `endat_ops.c` | No | Compatibility wrapper around the published-sample API |
| `endat21_schedulePositionRead()` | `endat_ops.c` | No | Compatibility helper that directly arms one non-blocking read |
| `endat21_servicePositionRead()` | `endat_ops.c` | No | Compatibility helper that services one completed read |
| `spiRxFifoIsr()` | `endat.c` | ISR | Drains SPI-B RX FIFO and flags a completed frame |
| `endatProducerISR()` | `endat.c` | ISR | Runs the EPWM7-driven producer tick |

### Private helpers (`endat_ops.c`)

| Helper | Purpose |
|---|---|
| `endatGetType()` | Returns `ENDAT21` or `ENDAT22` from `ENCODER_TYPE` |
| `endatGetPositionRaw()` | Reconstructs and masks the raw position from `position_lo/hi` |
| `endatValidatePositionCrc()` | Computes the expected CRC for the current position frame |
| `endatDecodePositionSample()` | Converts the current library frame into a decoded runtime sample |
| `endatPublishPositionSample()` | Writes the inactive slot and flips `activeIndex` last |
| `endatHandleReadTimeout()` | Drops a stalled read and increments the timeout counter |

---

## 6. Dependency Graph

```text
EnDat_Init()
    +- EPWM4_Config()
    +- PM_endat22_generateCRCTable()
    +- Endat_setup_GPIO()
    +- Endat_config_XBAR()
    \- PM_endat22_setupPeriph()

endat21_runCommandSet()        depends on: EnDat_Init()
endat22_setupAddlData()        depends on: EnDat_Init(), ENCODER_TYPE == 22
EnDat_initDelayComp()          depends on: EnDat_Init(), position_clocks set
PM_endat22_setFreq(RUNTIME)    depends on: EnDat_initDelayComp()
endat21_initProducer()         depends on: runtime frequency selected, polePairs configured
endat21_startProducer()        depends on: endat21_initProducer()

Runtime:
    EPWM7 -> endatProducerISR()
          -> endat21_runProducerTick()
          -> PM_endat22_startOperation()
          -> spiRxFifoIsr()
          -> PM_endat22_receiveData()
          -> endatValidatePositionCrc()
          -> endatPublishPositionSample()

    EPWM1 -> Cla1Task1()
          -> consume published ClaData snapshot for FCL

    EPWM1 -> motor1ControlISR()
          -> endat21_getPublishedPosition()
          -> mirror published snapshot into motorVars[0]
```

---

## 7. Hardware Resource Map

### SPI-B (EnDat data interface)

| Resource | Assignment | Notes |
|---|---|---|
| Peripheral | SPI-B (`SpibRegs`) | RX FIFO interrupt driven |
| MOSI (TX) | GPIO63 | `SPISIMOB`; also monitored through InputXBAR |
| MISO (RX) | GPIO64 | `SPISOMIB` |
| CLK | GPIO65 | `SPICLKB` |
| CS | GPIO66 | `SPISTEB` |
| PIE interrupt | Group 6, INT3 | `SPIB_RX_INT` -> `spiRxFifoIsr()` |

### EPWM4 (EnDat clock generation)

| Resource | Assignment | Notes |
|---|---|---|
| EPWM4A | GPIO6 | EnDat clock master output |
| EPWM4B | GPIO7 | SPI clock slave output |
| TZ action | OST -> force high | Idle clock state during reset / bring-up |

### EPWM7 (runtime producer scheduler)

| Resource | Assignment | Notes |
|---|---|---|
| Peripheral | EPWM7 | Internal EnDat runtime scheduler |
| Interrupt | PIE Group 3, INT7 | `ENDAT_PRODUCER_INT` -> `endatProducerISR()` |
| Rate | `40 kHz` | Derived from `M1_INV_PWM_TICKS / 4` in this build |
| Phase offset | `ENDAT_PRODUCER_PHASE_TICKS` | Starts away from the EPWM1 PWM edge |
| GPIO output | None by default | GPIO157/158 are only muxed when `DACOUT_EN` is enabled |

### Other GPIO

| GPIO | Signal | Direction | Notes |
|---|---|---|---|
| GPIO9 | EnDat TxEN | OUT | RS-485 direction control |
| GPIO63 | EncData / SPISIMOB | IN | Also routed to InputXBAR Input1 |
| GPIO139 | EnDat 5V power | OUT | Powered during `EnDat_Init()` |

---

## 8. Command Usage

### Initialization commands

The blocking init sequence still uses:

- `ENCODER_RECEIVE_RESET`
- `SELECTION_OF_MEMORY_AREA`
- `ENCODER_SEND_PARAMETER`

### Runtime command

| Command | Additional data count | Used by |
|---|---|---|
| `ENCODER_SEND_POSITION_VALUES` | 0 | Producer runtime reads, blocking reads, delay compensation |
| `ENCODER_SEND_POSITION_VALUES_WITH_ADDITIONAL_DATA` | 2 | `endat22_readPositionWithAddlData()` |

The fast runtime producer is position-only. The additional-data path remains diagnostic / bring-up functionality.

---

## 9. Integration With Motor Control & FCL

The runtime responsibilities are intentionally split:

| Context | Responsibility |
|---|---|
| `endatProducerISR()` | Acquire EnDat frames, CRC-check, decode, publish |
| `Cla1Task1()` | Latch the latest published EnDat angle into `fclVars[0].qep.*` and `pangle` at the PWM edge |
| `motor1ControlISR()` | Mirror the same published sample into `motorVars[0]` for speed estimation and observability |

Important consequences:

- No EnDat CRC or decode work remains on the FCL critical CPU path.
- FCL still sees a PWM-synchronous position handoff because `Cla1Task1()` runs from `EPWM1INT`.
- The CPU speed estimator uses the same angle stream as FCL, but without being responsible for publishing it.
- Outside alignment, the CLA keeps the previous angle if no valid EnDat sample is available yet.

---

## 10. Failure Behaviour

| Condition | Runtime result |
|---|---|
| `endatInitDone == 0` | CPU mirror path returns early; CLA continues using prior state |
| CRC failure | `gEndatCrcFailCount++`; no new snapshot is published |
| Producer timeout | `gEndatTimeoutCount++`; stalled read is dropped and the next EPWM7 tick re-arms a new read |
| No valid published sample yet | CLA leaves the previous FCL position intact outside alignment |
| `position_clocks == 0` | Decode fails and publishing is skipped until init fixes encoder configuration |

Init-time CRC failures remain fatal and still hit `ESTOP0`.

---

## 11. Notes & Constraints

### Shared snapshot model

The fast runtime path is multi-consumer. CPU and CLA both read the same published snapshot from `ClaData`, so readers must not assume they are the only consumer.

### Blocking reads remain startup-only

`endat21_readPosition()` and `endat22_readPositionWithAddlData()` still spin on `dataReady` and are not appropriate for ISR context once the fast runtime producer is active.

### EnDat 2.2 additional data remains off the fast path

The fast producer intentionally uses `ENCODER_SEND_POSITION_VALUES` only. This keeps runtime latency low and avoids pushing additional-data decode work into the published-angle path.

### EPWM7 resource ownership

This build reserves `EPWM7` for EnDat scheduling. `DACOUT_EN` is compile-time blocked because the prior DAC debug muxing reused EPWM7 outputs.

### Producer rate assumption

The current implementation is tuned for the present build settings: `10 kHz` PWM, `SINGLE_SAMPLING`, and a `4x` EnDat producer ratio. If those assumptions change, re-check both the EPWM7 period macros and the expected producer-to-consumer timing.
