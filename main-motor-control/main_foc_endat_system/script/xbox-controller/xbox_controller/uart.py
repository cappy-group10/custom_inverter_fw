#!/usr/bin/env python3
"""UART transport layer — packs commands into frames and parses MCU responses."""

import struct
import threading
import time
from copy import deepcopy
from collections import deque
from dataclasses import dataclass

import serial

from .app_logging import NullStructuredLogger
from .commands import MotorCommand, MusicCommand, CtrlState
from .runtime_models import FrameRecord, to_payload


# ---------------------------------------------------------------------------
#  Wire protocol constants
# ---------------------------------------------------------------------------
TX_SYNC = 0xAA          # start-of-frame marker (host -> MCU)
RX_SYNC = 0x55          # start-of-frame marker (MCU -> host)

# Frame IDs (host -> MCU)
FRAME_MOTOR_CMD = 0x01
FRAME_MUSIC_CMD = 0x02

# Frame IDs (MCU -> host)
FRAME_STATUS    = 0x10
FRAME_FAULT     = 0x11


# ---------------------------------------------------------------------------
#  TX frame formats
# ---------------------------------------------------------------------------
# MotorCommand frame:
#   [SYNC:1][ID:1][ctrlState:1][speedRef:4f][idRef:4f][iqRef:4f][chksum:1]
#   total = 16 bytes
TX_MOTOR_FMT = ">BBBfffB"
TX_MOTOR_LEN = struct.calcsize(TX_MOTOR_FMT)

# MusicCommand frame:
#   [SYNC:1][ID:1][ctrlState:1][freqPu:4f][iqRef:4f][idRef:4f][sustain:1][chksum:1]
#   total = 17 bytes
TX_MUSIC_FMT = ">BBBfffBB"
TX_MUSIC_LEN = struct.calcsize(TX_MUSIC_FMT)


# ---------------------------------------------------------------------------
#  RX frame formats (MCU -> host)
# ---------------------------------------------------------------------------
# Status frame:
#   [SYNC:1][ID:1][runMotor:1][ctrlState:1][tripFlag:2H][speedRef:4f]
#   [speedFbk:4f][posMechTheta:4f][Vdcbus:4f][IdFbk:4f][IqFbk:4f]
#   [currentAs:4f][currentBs:4f][currentCs:4f][isrTicker:4I][chksum:1]
#   total = 47 bytes
RX_STATUS_FMT = ">BBBBHfffffffffIB"
RX_STATUS_LEN = struct.calcsize(RX_STATUS_FMT)

# Legacy Phase 2 status frame without speed feedback:
#   [SYNC:1][ID:1][runMotor:1][ctrlState:1][tripFlag:2H][speedRef:4f]
#   [posMechTheta:4f][Vdcbus:4f][IdFbk:4f][IqFbk:4f]
#   [currentAs:4f][currentBs:4f][currentCs:4f][isrTicker:4I][chksum:1]
#   total = 43 bytes
RX_STATUS_FMT_LEGACY = ">BBBBHffffffffIB"
RX_STATUS_LEN_LEGACY = struct.calcsize(RX_STATUS_FMT_LEGACY)

# Fault frame:
#   [SYNC:1][ID:1][tripFlag:2H][tripCount:2H][chksum:1]
#   total = 8 bytes
RX_FAULT_FMT = ">BBHHB"
RX_FAULT_LEN = struct.calcsize(RX_FAULT_FMT)


@dataclass
class UARTCounters:
    """Transport counters exposed to the drive runtime."""

    tx_frames: int = 0
    rx_frames: int = 0
    status_frames: int = 0
    fault_frames: int = 0
    checksum_errors: int = 0
    serial_errors: int = 0


@dataclass
class UARTHealth:
    """Current UART state used by the dashboard health panel."""

    port_open: bool = False
    terminal_only: bool = False
    last_error: str | None = None
    last_frame_at: float | None = None
    last_status_at: float | None = None


# ---------------------------------------------------------------------------
#  Parsed RX data
# ---------------------------------------------------------------------------
@dataclass
class MCUStatus:
    """Status telemetry received from the MCU."""
    run_motor: int = 0          # MotorRunStop_e: 0=STOP, 1=RUN
    ctrl_state: int = 0         # CtrlState_e
    trip_flag: int = 0          # tripFlagDMC
    speed_ref: float = 0.0      # current speedRef on MCU side
    speed_fbk: float = 0.0      # speed estimator feedback on MCU side
    pos_mech_theta: float = 0.0 # mechanical rotor position
    vdc_bus: float = 0.0        # DC bus voltage (Vdcbus)
    id_fbk: float = 0.0         # d-axis current feedback (pi_id.fbk)
    iq_fbk: float = 0.0         # q-axis current feedback (pi_iq.fbk)
    current_as: float = 0.0     # phase A current
    current_bs: float = 0.0     # phase B current
    current_cs: float = 0.0     # phase C current
    temp_motor_winding_c: float | None = None
    temp_mcu_c: float | None = None
    temp_igbts_c: float | None = None
    isr_ticker: int = 0         # ISR heartbeat counter

    def __str__(self):
        state = CtrlState(self.ctrl_state).name if self.ctrl_state <= 4 else f"?{self.ctrl_state}"
        return (f"MCU: run={self.run_motor} ctrl={state} "
                f"trip={self.trip_flag:#06x} spd_ref={self.speed_ref:+.4f} spd_fbk={self.speed_fbk:+.4f} "
                f"theta={self.pos_mech_theta:+.4f} "
                f"Vdc={self.vdc_bus:.1f} "
                f"Id={self.id_fbk:+.4f} Iq={self.iq_fbk:+.4f} "
                f"Ia={self.current_as:+.3f} Ib={self.current_bs:+.3f} Ic={self.current_cs:+.3f} "
                f"tick={self.isr_ticker}")


@dataclass
class MCUFault:
    """Fault report received from the MCU."""
    trip_flag: int = 0
    trip_count: int = 0

    def __str__(self):
        return f"FAULT: flag={self.trip_flag:#06x} count={self.trip_count}"


# ---------------------------------------------------------------------------
#  UARTLink
# ---------------------------------------------------------------------------
class UARTLink:
    """Bidirectional UART link to the motor-control MCU.

    TX: packs MotorCommand / MusicCommand into framed packets.
    RX: parses MCU status/fault frames into dataclasses.
    Both directions are buffered; RX runs in a background thread.

    For development without hardware, set `port=None` to use
    terminal-only mode (prints TX hex, no RX).
    """

    def __init__(
        self,
        port: str | None = None,
        baudrate: int = 115200,
        rx_buf_size: int = 128,
        tx_buf_size: int = 64,
    ):
        self._port_name = port
        self._baudrate = baudrate
        self._ser: serial.Serial | None = None
        self._lock = threading.Lock()

        # Buffers
        self.rx_status: deque[MCUStatus] = deque(maxlen=rx_buf_size)
        self.rx_faults: deque[MCUFault] = deque(maxlen=rx_buf_size)
        self.tx_log: deque[bytes] = deque(maxlen=tx_buf_size)
        self._frame_events: deque[FrameRecord] = deque(maxlen=rx_buf_size + tx_buf_size)
        self._latest_status: MCUStatus | None = None
        self._counters = UARTCounters()
        self._health = UARTHealth(terminal_only=port is None)
        self._logger = NullStructuredLogger()
        self._legacy_status_warned = False

        self._rx_thread: threading.Thread | None = None
        self._running = False
        self._rx_buf = bytearray()

    def set_logger(self, logger):
        """Attach a structured logger used for UART diagnostics."""
        self._logger = logger or NullStructuredLogger()

    # -- lifecycle ---------------------------------------------------------

    def open(self):
        """Open the serial port and start the RX listener thread."""
        if self._port_name is None:
            with self._lock:
                self._health.terminal_only = True
                self._health.port_open = False
                self._health.last_error = None
            print("[UART] terminal-only mode (no serial port)")
            self._logger.log(
                "info",
                "uart",
                "UART opened in terminal-only mode",
                route="/uart/open",
                metadata={"port": "demo", "baudrate": self._baudrate},
            )
            return

        self._ser = serial.Serial(
            self._port_name,
            baudrate=self._baudrate,
            timeout=0.01,
        )
        self._running = True
        self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._rx_thread.start()
        with self._lock:
            self._health.port_open = True
            self._health.terminal_only = False
            self._health.last_error = None
        print(f"[UART] opened {self._port_name} @ {self._baudrate}")
        self._logger.log(
            "info",
            "uart",
            "UART serial port opened",
            route="/uart/open",
            metadata={"port": self._port_name, "baudrate": self._baudrate},
        )

    def close(self):
        """Stop the RX thread and close the port."""
        self._running = False
        if self._rx_thread:
            self._rx_thread.join(timeout=1.0)
            self._rx_thread = None
        if self._ser:
            self._ser.close()
            self._ser = None
        with self._lock:
            self._health.port_open = False
        self._logger.log(
            "info",
            "uart",
            "UART link closed",
            route="/uart/close",
            metadata={"port": self._port_name or "demo"},
        )

    # -- TX: command -> wire -----------------------------------------------

    @staticmethod
    def _checksum(data: bytes) -> int:
        return sum(data) & 0xFF

    @staticmethod
    def _frame_name(frame_id: int) -> str:
        return {
            FRAME_MOTOR_CMD: "motor_cmd",
            FRAME_MUSIC_CMD: "music_cmd",
            FRAME_STATUS: "status",
            FRAME_FAULT: "fault",
        }.get(frame_id, f"frame_{frame_id:02X}")

    def _record_frame(
        self,
        direction: str,
        frame_id: int,
        frame: bytes,
        decoded: dict[str, object],
        checksum_ok: bool,
    ):
        now = time.time()
        record = FrameRecord(
            direction=direction,
            frame_id=frame_id,
            frame_name=self._frame_name(frame_id),
            raw_hex=frame.hex(" "),
            decoded=decoded,
            checksum_ok=checksum_ok,
            timestamp=now,
        )
        with self._lock:
            self._frame_events.append(record)
            self._health.last_frame_at = now
            if direction == "tx":
                self._counters.tx_frames += 1
            else:
                self._counters.rx_frames += 1
                if not checksum_ok:
                    self._counters.checksum_errors += 1

    def pack_motor(self, cmd: MotorCommand) -> bytes:
        """Pack a MotorCommand into a wire frame."""
        payload = struct.pack(
            ">BBBfff",
            TX_SYNC, FRAME_MOTOR_CMD, int(cmd.ctrl_state),
            cmd.speed_ref, cmd.id_ref, cmd.iq_ref,
        )
        return payload + bytes([self._checksum(payload)])

    def pack_music(self, cmd: MusicCommand) -> bytes:
        """Pack a MusicCommand into a wire frame."""
        payload = struct.pack(
            ">BBBfffB",
            TX_SYNC, FRAME_MUSIC_CMD, int(cmd.ctrl_state),
            cmd.freq_pu, cmd.iq_ref, cmd.id_ref,
            int(cmd.sustain),
        )
        return payload + bytes([self._checksum(payload)])

    def send(self, cmd: MotorCommand | MusicCommand):
        """Pack and transmit a command. Logs to tx_log and prints in terminal mode."""
        if isinstance(cmd, MusicCommand):
            frame = self.pack_music(cmd)
        else:
            frame = self.pack_motor(cmd)

        with self._lock:
            self.tx_log.append(frame)
        self._record_frame(
            direction="tx",
            frame_id=frame[1],
            frame=frame,
            decoded=to_payload(cmd),
            checksum_ok=True,
        )

        if self._ser and self._ser.is_open:
            try:
                self._ser.write(frame)
            except serial.SerialException as exc:
                with self._lock:
                    self._counters.serial_errors += 1
                    self._health.last_error = str(exc)
                self._logger.log(
                    "error",
                    "uart",
                    "UART serial write failed",
                    route="/uart/send",
                    metadata={"port": self._port_name or "demo", "error": str(exc)},
                )

    # -- RX: wire -> parsed data -------------------------------------------

    def _rx_loop(self):
        """Background thread: read bytes and parse frames."""
        while self._running and self._ser:
            try:
                chunk = self._ser.read(64)
            except serial.SerialException as exc:
                with self._lock:
                    self._counters.serial_errors += 1
                    self._health.last_error = str(exc)
                self._logger.log(
                    "error",
                    "uart",
                    "UART serial read failed",
                    route="/uart/read",
                    metadata={"port": self._port_name or "demo", "error": str(exc)},
                )
                break
            if chunk:
                self._rx_buf.extend(chunk)
                self._parse_rx_buf()

    def _parse_rx_buf(self):
        """Scan the RX buffer for complete frames."""
        while True:
            # Find sync byte
            idx = self._rx_buf.find(bytes([RX_SYNC]))
            if idx < 0:
                self._rx_buf.clear()
                return
            if idx > 0:
                self._rx_buf = self._rx_buf[idx:]

            # Need at least sync + id to know frame type
            if len(self._rx_buf) < 2:
                return

            frame_id = self._rx_buf[1]

            if frame_id == FRAME_STATUS:
                if len(self._rx_buf) < RX_STATUS_LEN_LEGACY:
                    return

                if len(self._rx_buf) >= RX_STATUS_LEN:
                    frame = bytes(self._rx_buf[:RX_STATUS_LEN])
                    if self._verify_checksum(frame):
                        self._rx_buf = self._rx_buf[RX_STATUS_LEN:]
                        self._handle_status(frame)
                        continue

                    legacy_frame = bytes(self._rx_buf[:RX_STATUS_LEN_LEGACY])
                    if self._verify_checksum(legacy_frame):
                        self._rx_buf = self._rx_buf[RX_STATUS_LEN_LEGACY:]
                        self._handle_status_legacy(legacy_frame)
                        continue

                    self._record_frame(
                        direction="rx",
                        frame_id=frame_id,
                        frame=frame,
                        decoded={"error": "status checksum mismatch"},
                        checksum_ok=False,
                    )
                    self._logger.log(
                        "warning",
                        "uart",
                        "Status frame checksum mismatch",
                        route="/uart/rx",
                        metadata={"frame_id": frame_id, "raw_hex": frame.hex(" ")},
                    )
                    print("[UART] status frame checksum mismatch")
                    self._rx_buf = self._rx_buf[1:]
                    continue

                legacy_frame = bytes(self._rx_buf[:RX_STATUS_LEN_LEGACY])
                if self._verify_checksum(legacy_frame):
                    self._rx_buf = self._rx_buf[RX_STATUS_LEN_LEGACY:]
                    self._handle_status_legacy(legacy_frame)
                    continue

                return

            elif frame_id == FRAME_FAULT:
                if len(self._rx_buf) < RX_FAULT_LEN:
                    return
                frame = bytes(self._rx_buf[:RX_FAULT_LEN])
                self._rx_buf = self._rx_buf[RX_FAULT_LEN:]

                if self._verify_checksum(frame):
                    self._handle_fault(frame)
                else:
                    self._record_frame(
                        direction="rx",
                        frame_id=frame_id,
                        frame=frame,
                        decoded={"error": "fault checksum mismatch"},
                        checksum_ok=False,
                    )
                    self._logger.log(
                        "warning",
                        "uart",
                        "Fault frame checksum mismatch",
                        route="/uart/rx",
                        metadata={"frame_id": frame_id, "raw_hex": frame.hex(" ")},
                    )
                    print("[UART] fault frame checksum mismatch")

            else:
                # Unknown frame id — skip this sync byte and rescan
                self._rx_buf = self._rx_buf[1:]

    def _verify_checksum(self, frame: bytes) -> bool:
        return self._checksum(frame[:-1]) == frame[-1]

    def _handle_status(self, frame: bytes):
        vals = struct.unpack(RX_STATUS_FMT, frame)
        # vals: (sync, id, runMotor, ctrlState, tripFlag, speedRef, speedFbk,
        #        posMechTheta, Vdcbus, IdFbk, IqFbk,
        #        currentAs, currentBs, currentCs, isrTicker, chk)
        status = MCUStatus(
            run_motor=vals[2],
            ctrl_state=vals[3],
            trip_flag=vals[4],
            speed_ref=vals[5],
            speed_fbk=vals[6],
            pos_mech_theta=vals[7],
            vdc_bus=vals[8],
            id_fbk=vals[9],
            iq_fbk=vals[10],
            current_as=vals[11],
            current_bs=vals[12],
            current_cs=vals[13],
            isr_ticker=vals[14],
        )
        now = time.time()
        with self._lock:
            self.rx_status.append(status)
            self._latest_status = status
            self._health.last_status_at = now
            self._counters.status_frames += 1
        self._record_frame(
            direction="rx",
            frame_id=FRAME_STATUS,
            frame=frame,
            decoded=to_payload(status),
            checksum_ok=True,
        )

    def _handle_status_legacy(self, frame: bytes):
        vals = struct.unpack(RX_STATUS_FMT_LEGACY, frame)
        status = MCUStatus(
            run_motor=vals[2],
            ctrl_state=vals[3],
            trip_flag=vals[4],
            speed_ref=vals[5],
            speed_fbk=0.0,
            pos_mech_theta=vals[6],
            vdc_bus=vals[7],
            id_fbk=vals[8],
            iq_fbk=vals[9],
            current_as=vals[10],
            current_bs=vals[11],
            current_cs=vals[12],
            isr_ticker=vals[13],
        )
        now = time.time()
        with self._lock:
            self.rx_status.append(status)
            self._latest_status = status
            self._health.last_status_at = now
            self._counters.status_frames += 1
        self._record_frame(
            direction="rx",
            frame_id=FRAME_STATUS,
            frame=frame,
            decoded={**to_payload(status), "legacy_status_frame": True},
            checksum_ok=True,
        )
        if not self._legacy_status_warned:
            self._legacy_status_warned = True
            self._logger.log(
                "warning",
                "uart",
                "Legacy 43-byte status frame detected",
                route="/uart/rx",
                metadata={
                    "frame_id": FRAME_STATUS,
                    "message": "MCU telemetry is using the older status frame without speed feedback. Reflash the MCU to enable true speed_fbk telemetry.",
                },
            )

    def _handle_fault(self, frame: bytes):
        vals = struct.unpack(RX_FAULT_FMT, frame)
        # vals: (sync, id, tripFlag, tripCount, chk)
        fault = MCUFault(trip_flag=vals[2], trip_count=vals[3])
        with self._lock:
            self.rx_faults.append(fault)
            self._counters.fault_frames += 1
        self._record_frame(
            direction="rx",
            frame_id=FRAME_FAULT,
            frame=frame,
            decoded=to_payload(fault),
            checksum_ok=True,
        )
        self._logger.log(
            "warning",
            "uart",
            "MCU fault frame parsed",
            route="/uart/rx",
            metadata={"frame_id": FRAME_FAULT, "decoded": to_payload(fault)},
        )
        print(f"\r\n[RX] {fault}")

    # -- thread-safe consumers ---------------------------------------------

    def pop_statuses(self) -> list[MCUStatus]:
        """Return and clear buffered status messages."""
        with self._lock:
            msgs = list(self.rx_status)
            self.rx_status.clear()
        return msgs

    def pop_faults(self) -> list[MCUFault]:
        """Return and clear buffered fault messages."""
        with self._lock:
            msgs = list(self.rx_faults)
            self.rx_faults.clear()
        return msgs

    def pop_frame_records(self) -> list[FrameRecord]:
        """Return and clear buffered frame records."""
        with self._lock:
            frames = list(self._frame_events)
            self._frame_events.clear()
        return frames

    def drain_status(self) -> list[MCUStatus]:
        """Backward-compatible alias for pop_statuses()."""
        return self.pop_statuses()

    def drain_faults(self) -> list[MCUFault]:
        """Backward-compatible alias for pop_faults()."""
        return self.pop_faults()

    def get_latest_status(self) -> MCUStatus | None:
        """Return the latest status without consuming buffered updates."""
        with self._lock:
            return deepcopy(self._latest_status)

    def get_counters(self) -> UARTCounters:
        """Return a copy of the current transport counters."""
        with self._lock:
            return deepcopy(self._counters)

    def get_health(self) -> UARTHealth:
        """Return a copy of the current transport health flags."""
        with self._lock:
            health = deepcopy(self._health)
            if self._ser:
                health.port_open = self._ser.is_open
        return health
