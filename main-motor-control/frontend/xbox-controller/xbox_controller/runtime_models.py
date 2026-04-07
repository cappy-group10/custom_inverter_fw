#!/usr/bin/env python3
"""Shared records and serialization helpers for the drive runtime."""

from __future__ import annotations

from dataclasses import dataclass, field, fields, is_dataclass
from enum import Enum
from typing import Any


@dataclass(slots=True)
class FrameRecord:
    """One host or MCU frame with decoded metadata for UI/log consumers."""

    direction: str
    frame_id: int
    frame_name: str
    raw_hex: str
    decoded: dict[str, Any]
    checksum_ok: bool
    timestamp: float


@dataclass(slots=True)
class EventRecord:
    """Higher-level runtime event such as button edges or faults."""

    kind: str
    title: str
    message: str
    timestamp: float
    data: dict[str, Any] = field(default_factory=dict)


@dataclass(slots=True)
class ControllerLayoutDescriptor:
    """Drive-mode control mapping exposed to the dashboard."""

    control_id: str
    label: str
    group: str
    mapped: bool
    mapping_target: str
    mapping_text: str


@dataclass(slots=True)
class TelemetrySample:
    """Reduced-rate MCU telemetry sample used by the dashboard charts."""

    timestamp: float
    speed_ref: float
    speed_fbk: float
    id_ref: float
    id_fbk: float
    iq_ref: float
    iq_fbk: float
    vdc_bus: float
    current_as: float
    current_bs: float
    current_cs: float


@dataclass(slots=True)
class MotorConfig:
    """Operator-facing scaling and limit values derived from firmware settings."""

    base_speed_rpm: float = 0.0
    base_current_a: float = 0.0
    vdcbus_min_v: float = 0.0
    vdcbus_max_v: float = 0.0
    rated_input_power_w: float = 0.0


@dataclass(slots=True)
class MusicSongOption:
    """One predefined musical-motor song exposed to the dashboard."""

    song_id: int
    label: str


@dataclass(slots=True)
class MusicCommandRecord:
    """Last host-originated music command sent to the MCU."""

    command_type: str = ""
    song_id: int | None = None
    song_label: str | None = None
    action: str | None = None
    volume: float | None = None
    amplitude: float | None = None
    timestamp: float = 0.0


@dataclass(slots=True)
class MusicStatusRecord:
    """Decoded musical-motor status telemetry from the MCU."""

    play_state: str = "IDLE"
    play_mode: str = "SONG"
    song_id: int = 0
    note_index: int = 0
    note_total: int = 0
    current_freq_hz: float = 0.0
    amplitude: float = 0.0
    isr_ticker: int = 0


@dataclass(slots=True)
class MusicState:
    """Thread-safe dashboard view of the music-mode session state."""

    songs: list[MusicSongOption] = field(default_factory=list)
    selected_song_id: int | None = None
    last_started_song_id: int | None = None
    volume: float = 0.0
    last_command: MusicCommandRecord | None = None
    latest_status: MusicStatusRecord | None = None
    play_state: str = "IDLE"
    play_mode: str = "SONG"
    note_index: int = 0
    note_total: int = 0
    current_freq_hz: float = 0.0
    amplitude: float = 0.0
    isr_ticker: int = 0


@dataclass(slots=True)
class SessionSnapshot:
    """Thread-safe view of the current drive session state."""

    session_state: str = "idle"
    mode: str = "drive"
    port: str | None = None
    baudrate: int = 115200
    joystick_index: int = 0
    joystick_name: str = ""
    mapping_name: str = "DriveMapping"
    controller_connected: bool = False
    controller_state: Any = None
    controller_layout: list[ControllerLayoutDescriptor] = field(default_factory=list)
    motor_config: MotorConfig = field(default_factory=MotorConfig)
    last_host_command: Any = None
    latest_mcu_status: Any = None
    active_override: str | None = None
    recent_faults: list[Any] = field(default_factory=list)
    recent_frames: list[FrameRecord] = field(default_factory=list)
    recent_events: list[EventRecord] = field(default_factory=list)
    telemetry_samples: list[TelemetrySample] = field(default_factory=list)
    music_state: MusicState | None = None
    counters: dict[str, int] = field(default_factory=dict)
    health: dict[str, Any] = field(default_factory=dict)
    updated_at: float = 0.0
    started_at: float | None = None
    stopped_at: float | None = None
    last_error: str | None = None


def to_payload(value: Any) -> Any:
    """Serialize dataclasses and enums into JSON-friendly values."""

    if is_dataclass(value):
        return {item.name: to_payload(getattr(value, item.name)) for item in fields(value)}
    if isinstance(value, Enum):
        return value.name
    if isinstance(value, dict):
        return {str(key): to_payload(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [to_payload(item) for item in value]
    return value
