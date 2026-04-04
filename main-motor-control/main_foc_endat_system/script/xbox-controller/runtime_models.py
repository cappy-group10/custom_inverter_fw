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
    id_fbk: float
    iq_fbk: float
    vdc_bus: float
    current_as: float
    current_bs: float
    current_cs: float


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
    last_host_command: Any = None
    latest_mcu_status: Any = None
    recent_faults: list[Any] = field(default_factory=list)
    recent_frames: list[FrameRecord] = field(default_factory=list)
    recent_events: list[EventRecord] = field(default_factory=list)
    telemetry_samples: list[TelemetrySample] = field(default_factory=list)
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
