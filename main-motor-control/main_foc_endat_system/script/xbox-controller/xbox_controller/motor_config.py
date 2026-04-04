#!/usr/bin/env python3
"""Load operator-facing motor scaling values from the active firmware config."""

from __future__ import annotations

import re
from pathlib import Path

from .runtime_models import MotorConfig


PROJECT_DIR = Path(__file__).resolve().parent.parent
REPO_ROOT = PROJECT_DIR.parent.parent
FIRMWARE_CONFIG_PATH = REPO_ROOT / "include/boostxl_3phganinv/dual_axis_servo_drive_user.h"

MACRO_PATTERN = re.compile(
    r"^\s*#define\s+(?P<name>\w+)\s+(?P<value>[-+]?(?:\d+(?:\.\d+)?|\.\d+))\b",
    re.MULTILINE,
)

DEFAULT_MACROS = {
    "M1_BASE_CURRENT": 5.0,
    "M1_BASE_FREQ": 1000.0,
    "M1_POLES": 10.0,
    "M1_VDCBUS_MIN": 24.0,
    "M1_VDCBUS_MAX": 600.0,
}


def _load_numeric_macros() -> dict[str, float]:
    try:
        contents = FIRMWARE_CONFIG_PATH.read_text(encoding="utf-8")
    except OSError:
        return dict(DEFAULT_MACROS)

    macros = dict(DEFAULT_MACROS)
    for match in MACRO_PATTERN.finditer(contents):
        name = match.group("name")
        if name not in macros:
            continue
        macros[name] = float(match.group("value"))

    return macros


def load_motor_config() -> MotorConfig:
    """Return operator scaling values derived from the active M1 firmware macros."""

    macros = _load_numeric_macros()
    base_current_a = float(macros["M1_BASE_CURRENT"])
    base_freq_hz = float(macros["M1_BASE_FREQ"])
    poles = float(macros["M1_POLES"])
    vdcbus_min_v = float(macros["M1_VDCBUS_MIN"])
    vdcbus_max_v = float(macros["M1_VDCBUS_MAX"])

    base_speed_rpm = 0.0 if poles == 0.0 else (120.0 * base_freq_hz) / poles
    rated_input_power_w = vdcbus_max_v * base_current_a

    return MotorConfig(
        base_speed_rpm=base_speed_rpm,
        base_current_a=base_current_a,
        vdcbus_min_v=vdcbus_min_v,
        vdcbus_max_v=vdcbus_max_v,
        rated_input_power_w=rated_input_power_w,
    )


DEFAULT_MOTOR_CONFIG = load_motor_config()
