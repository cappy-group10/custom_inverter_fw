#!/usr/bin/env python3
"""UART transport layer — packs commands into frames and parses MCU responses."""

import struct
import threading
from collections import deque
from dataclasses import dataclass

import serial

from commands import MotorCommand, MusicCommand, CtrlState


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
#   [posMechTheta:4f][Vdcbus:4f][IdFbk:4f][IqFbk:4f]
#   [currentAs:4f][currentBs:4f][currentCs:4f][isrTicker:4I][chksum:1]
#   total = 39 bytes
RX_STATUS_FMT = ">BBBBHfffffffIB"
RX_STATUS_LEN = struct.calcsize(RX_STATUS_FMT)

# Fault frame:
#   [SYNC:1][ID:1][tripFlag:2H][tripCount:2H][chksum:1]
#   total = 8 bytes
RX_FAULT_FMT = ">BBHHB"
RX_FAULT_LEN = struct.calcsize(RX_FAULT_FMT)


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
    pos_mech_theta: float = 0.0 # mechanical rotor position
    vdc_bus: float = 0.0        # DC bus voltage (Vdcbus)
    id_fbk: float = 0.0         # d-axis current feedback (pi_id.fbk)
    iq_fbk: float = 0.0         # q-axis current feedback (pi_iq.fbk)
    current_as: float = 0.0     # phase A current
    current_bs: float = 0.0     # phase B current
    current_cs: float = 0.0     # phase C current
    isr_ticker: int = 0         # ISR heartbeat counter

    def __str__(self):
        state = CtrlState(self.ctrl_state).name if self.ctrl_state <= 4 else f"?{self.ctrl_state}"
        return (f"MCU: run={self.run_motor} ctrl={state} "
                f"trip={self.trip_flag:#06x} spd={self.speed_ref:+.4f} "
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

        # Buffers
        self.rx_status: deque[MCUStatus] = deque(maxlen=rx_buf_size)
        self.rx_faults: deque[MCUFault] = deque(maxlen=rx_buf_size)
        self.tx_log: deque[bytes] = deque(maxlen=tx_buf_size)

        self._rx_thread: threading.Thread | None = None
        self._running = False
        self._rx_buf = bytearray()

    # -- lifecycle ---------------------------------------------------------

    def open(self):
        """Open the serial port and start the RX listener thread."""
        if self._port_name is None:
            print("[UART] terminal-only mode (no serial port)")
            return

        self._ser = serial.Serial(
            self._port_name,
            baudrate=self._baudrate,
            timeout=0.01,
        )
        self._running = True
        self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._rx_thread.start()
        print(f"[UART] opened {self._port_name} @ {self._baudrate}")

    def close(self):
        """Stop the RX thread and close the port."""
        self._running = False
        if self._rx_thread:
            self._rx_thread.join(timeout=1.0)
            self._rx_thread = None
        if self._ser:
            self._ser.close()
            self._ser = None

    # -- TX: command -> wire -----------------------------------------------

    @staticmethod
    def _checksum(data: bytes) -> int:
        return sum(data) & 0xFF

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

        self.tx_log.append(frame)

        if self._ser and self._ser.is_open:
            self._ser.write(frame)

    # -- RX: wire -> parsed data -------------------------------------------

    def _rx_loop(self):
        """Background thread: read bytes and parse frames."""
        while self._running and self._ser:
            try:
                chunk = self._ser.read(64)
            except serial.SerialException:
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
                if len(self._rx_buf) < RX_STATUS_LEN:
                    return
                frame = bytes(self._rx_buf[:RX_STATUS_LEN])
                self._rx_buf = self._rx_buf[RX_STATUS_LEN:]

                if self._verify_checksum(frame):
                    self._handle_status(frame)
                else:
                    print("[UART] status frame checksum mismatch")

            elif frame_id == FRAME_FAULT:
                if len(self._rx_buf) < RX_FAULT_LEN:
                    return
                frame = bytes(self._rx_buf[:RX_FAULT_LEN])
                self._rx_buf = self._rx_buf[RX_FAULT_LEN:]

                if self._verify_checksum(frame):
                    self._handle_fault(frame)
                else:
                    print("[UART] fault frame checksum mismatch")

            else:
                # Unknown frame id — skip this sync byte and rescan
                self._rx_buf = self._rx_buf[1:]

    def _verify_checksum(self, frame: bytes) -> bool:
        return self._checksum(frame[:-1]) == frame[-1]

    def _handle_status(self, frame: bytes):
        vals = struct.unpack(RX_STATUS_FMT, frame)
        # vals: (sync, id, runMotor, ctrlState, tripFlag, speedRef,
        #        posMechTheta, Vdcbus, IdFbk, IqFbk,
        #        currentAs, currentBs, currentCs, isrTicker, chk)
        status = MCUStatus(
            run_motor=vals[2],
            ctrl_state=vals[3],
            trip_flag=vals[4],
            speed_ref=vals[5],
            pos_mech_theta=vals[6],
            vdc_bus=vals[7],
            id_fbk=vals[8],
            iq_fbk=vals[9],
            current_as=vals[10],
            current_bs=vals[11],
            current_cs=vals[12],
            isr_ticker=vals[13],
        )
        self.rx_status.append(status)

    def _handle_fault(self, frame: bytes):
        vals = struct.unpack(RX_FAULT_FMT, frame)
        # vals: (sync, id, tripFlag, tripCount, chk)
        fault = MCUFault(trip_flag=vals[2], trip_count=vals[3])
        self.rx_faults.append(fault)
        print(f"\r\n[RX] {fault}")

    # -- drain helpers for main loop ---------------------------------------

    def drain_status(self):
        """Return and clear all buffered status messages."""
        msgs = list(self.rx_status)
        self.rx_status.clear()
        return msgs

    def drain_faults(self):
        """Return and clear all buffered fault messages."""
        msgs = list(self.rx_faults)
        self.rx_faults.clear()
        return msgs
