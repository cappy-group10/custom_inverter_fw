#!/usr/bin/env python3
"""UART transport layer for the musical-motor firmware protocol."""

from __future__ import annotations

import struct
import threading
import time
from collections import deque
from copy import deepcopy
from dataclasses import dataclass

import serial

from .app_logging import NullStructuredLogger
from .runtime_models import FrameRecord, MusicStatusRecord, to_payload


TX_SYNC = 0xAA
RX_SYNC = 0x55

FRAME_SONG_CMD = 0x20
FRAME_CTRL_CMD = 0x22
FRAME_VOL_CMD = 0x23
FRAME_STATUS = 0x30

CTRL_ACTION_STOP = 0
CTRL_ACTION_PAUSE = 1
CTRL_ACTION_RESUME = 2

TX_SONG_FMT = ">BBBfB"
TX_SONG_LEN = struct.calcsize(TX_SONG_FMT)

TX_CTRL_FMT = ">BBBB"
TX_CTRL_LEN = struct.calcsize(TX_CTRL_FMT)

TX_VOL_FMT = ">BBfB"
TX_VOL_LEN = struct.calcsize(TX_VOL_FMT)

RX_STATUS_FMT = ">BBBBBHHffIB"
RX_STATUS_LEN = struct.calcsize(RX_STATUS_FMT)


PLAY_STATE_NAMES = {
    0: "IDLE",
    1: "PLAYING",
    2: "PAUSED",
}

PLAY_MODE_NAMES = {
    0: "SONG",
    1: "TONE",
}

CONTROL_ACTION_NAMES = {
    CTRL_ACTION_STOP: "STOP",
    CTRL_ACTION_PAUSE: "PAUSE",
    CTRL_ACTION_RESUME: "RESUME",
}


@dataclass
class MusicUARTCounters:
    """Transport counters exposed to the music runtime."""

    tx_frames: int = 0
    rx_frames: int = 0
    status_frames: int = 0
    checksum_errors: int = 0
    serial_errors: int = 0


@dataclass
class MusicUARTHealth:
    """Current UART health state used by the dashboard."""

    port_open: bool = False
    terminal_only: bool = False
    last_error: str | None = None
    last_frame_at: float | None = None
    last_status_at: float | None = None


def control_action_name(action: int) -> str:
    """Return a dashboard-friendly control action label."""

    return CONTROL_ACTION_NAMES.get(int(action), f"ACTION_{int(action)}")


class MusicUARTLink:
    """Bidirectional UART link for musical-motor song/control/status frames."""

    def __init__(
        self,
        port: str | None = None,
        baudrate: int = 115200,
        rx_buf_size: int = 128,
    ):
        self._port_name = port
        self._baudrate = baudrate
        self._ser: serial.Serial | None = None
        self._lock = threading.Lock()

        self.rx_status: deque[MusicStatusRecord] = deque(maxlen=rx_buf_size)
        self._frame_events: deque[FrameRecord] = deque(maxlen=rx_buf_size * 2)
        self._latest_status: MusicStatusRecord | None = None
        self._counters = MusicUARTCounters()
        self._health = MusicUARTHealth(terminal_only=port is None)
        self._logger = NullStructuredLogger()

        self._rx_thread: threading.Thread | None = None
        self._running = False
        self._rx_buf = bytearray()

    def set_logger(self, logger):
        """Attach the structured logger used for diagnostics."""

        self._logger = logger or NullStructuredLogger()

    def open(self):
        """Open the serial port and start the RX listener thread."""

        if self._port_name is None:
            with self._lock:
                self._health.terminal_only = True
                self._health.port_open = False
                self._health.last_error = None
            print("[Music UART] terminal-only mode (no serial port)")
            self._logger.log(
                "info",
                "music_uart",
                "Music UART opened in terminal-only mode",
                route="/music_uart/open",
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
        self._logger.log(
            "info",
            "music_uart",
            "Music UART serial port opened",
            route="/music_uart/open",
            metadata={"port": self._port_name, "baudrate": self._baudrate},
        )

    def close(self):
        """Stop the RX thread and close the port."""

        self._running = False
        if self._rx_thread is not None:
            self._rx_thread.join(timeout=1.0)
            self._rx_thread = None
        if self._ser is not None:
            self._ser.close()
            self._ser = None
        with self._lock:
            self._health.port_open = False
        self._logger.log(
            "info",
            "music_uart",
            "Music UART link closed",
            route="/music_uart/close",
            metadata={"port": self._port_name or "demo"},
        )

    @staticmethod
    def _checksum(data: bytes) -> int:
        return sum(data) & 0xFF

    @staticmethod
    def _frame_name(frame_id: int) -> str:
        return {
            FRAME_SONG_CMD: "song_cmd",
            FRAME_CTRL_CMD: "ctrl_cmd",
            FRAME_VOL_CMD: "vol_cmd",
            FRAME_STATUS: "status",
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

    def pack_song(self, song_id: int, amplitude: float) -> bytes:
        """Pack a predefined-song command into a wire frame."""

        payload = struct.pack(">BBBf", TX_SYNC, FRAME_SONG_CMD, int(song_id), float(amplitude))
        return payload + bytes([self._checksum(payload)])

    def pack_control(self, action: int) -> bytes:
        """Pack a stop/pause/resume control command into a wire frame."""

        payload = struct.pack(">BBB", TX_SYNC, FRAME_CTRL_CMD, int(action))
        return payload + bytes([self._checksum(payload)])

    def pack_volume(self, volume: float) -> bytes:
        """Pack a global volume command into a wire frame."""

        payload = struct.pack(">BBf", TX_SYNC, FRAME_VOL_CMD, float(volume))
        return payload + bytes([self._checksum(payload)])

    def _send_frame(self, frame: bytes, decoded: dict[str, object]):
        frame_id = frame[1]
        self._record_frame(
            direction="tx",
            frame_id=frame_id,
            frame=frame,
            decoded=decoded,
            checksum_ok=True,
        )
        if self._ser is not None and self._ser.is_open:
            try:
                self._ser.write(frame)
            except serial.SerialException as exc:
                with self._lock:
                    self._counters.serial_errors += 1
                    self._health.last_error = str(exc)
                self._logger.log(
                    "error",
                    "music_uart",
                    "Music UART serial write failed",
                    route="/music_uart/send",
                    metadata={"port": self._port_name or "demo", "error": str(exc)},
                )

    def send_song(self, song_id: int, amplitude: float):
        """Transmit a predefined-song selection command."""

        frame = self.pack_song(song_id, amplitude)
        self._send_frame(
            frame,
            {"command_type": "song", "song_id": int(song_id), "amplitude": float(amplitude)},
        )

    def send_control(self, action: int):
        """Transmit a stop/pause/resume command."""

        frame = self.pack_control(action)
        self._send_frame(
            frame,
            {"command_type": "control", "action": control_action_name(action)},
        )

    def send_volume(self, volume: float):
        """Transmit a global volume command."""

        frame = self.pack_volume(volume)
        self._send_frame(
            frame,
            {"command_type": "volume", "volume": float(volume)},
        )

    def _rx_loop(self):
        """Read bytes from the serial port and parse them into status frames."""

        while self._running and self._ser is not None:
            try:
                chunk = self._ser.read(64)
            except serial.SerialException as exc:
                with self._lock:
                    self._counters.serial_errors += 1
                    self._health.last_error = str(exc)
                self._logger.log(
                    "error",
                    "music_uart",
                    "Music UART serial read failed",
                    route="/music_uart/read",
                    metadata={"port": self._port_name or "demo", "error": str(exc)},
                )
                break
            if chunk:
                self._rx_buf.extend(chunk)
                self._parse_rx_buf()

    def _parse_rx_buf(self):
        """Scan the RX buffer for complete status frames."""

        while True:
            idx = self._rx_buf.find(bytes([RX_SYNC]))
            if idx < 0:
                self._rx_buf.clear()
                return
            if idx > 0:
                self._rx_buf = self._rx_buf[idx:]

            if len(self._rx_buf) < 2:
                return

            frame_id = self._rx_buf[1]
            if frame_id != FRAME_STATUS:
                self._rx_buf = self._rx_buf[1:]
                continue

            if len(self._rx_buf) < RX_STATUS_LEN:
                return

            frame = bytes(self._rx_buf[:RX_STATUS_LEN])
            self._rx_buf = self._rx_buf[RX_STATUS_LEN:]

            if self._checksum(frame[:-1]) == frame[-1]:
                self._handle_status(frame)
            else:
                self._record_frame(
                    direction="rx",
                    frame_id=frame_id,
                    frame=frame,
                    decoded={"error": "status checksum mismatch"},
                    checksum_ok=False,
                )

    def _handle_status(self, frame: bytes):
        values = struct.unpack(RX_STATUS_FMT, frame)
        status = MusicStatusRecord(
            play_state=PLAY_STATE_NAMES.get(values[2], str(values[2])),
            play_mode=PLAY_MODE_NAMES.get(values[3], str(values[3])),
            song_id=values[4],
            note_index=values[5],
            note_total=values[6],
            current_freq_hz=values[7],
            amplitude=values[8],
            isr_ticker=values[9],
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

    def pop_statuses(self) -> list[MusicStatusRecord]:
        """Return and clear all queued MCU status updates."""

        with self._lock:
            statuses = list(self.rx_status)
            self.rx_status.clear()
        return statuses

    def pop_frame_records(self) -> list[FrameRecord]:
        """Return and clear the accumulated UART frame records."""

        with self._lock:
            frames = list(self._frame_events)
            self._frame_events.clear()
        return frames

    def get_counters(self) -> MusicUARTCounters:
        """Return a snapshot of transport counters."""

        with self._lock:
            return deepcopy(self._counters)

    def get_health(self) -> MusicUARTHealth:
        """Return a snapshot of current transport health."""

        with self._lock:
            return deepcopy(self._health)
