#!/usr/bin/env python3
"""Motor command types — mirrors the MCU control variables in dual_axis_servo_drive.c."""

from dataclasses import dataclass
from enum import IntEnum


class CtrlState(IntEnum):
    """Maps to CtrlState_e on the MCU (fcl_enum.h)."""
    STOP  = 0
    RUN   = 1
    BRAKE = 2
    RESET = 3
    FAULT = 4
    CALIBRATE = 5


@dataclass
class CommandLimits:
    """Defines the allowable range and step size for each control variable.

    Acts as a constraint layer between the input mapping and the MCU.
    Adjust these to match your motor and drive ratings.
    """
    speed_min: float = -0.30   # per-unit (negative = reverse)
    speed_max: float =  0.30

    id_min: float = -0.30      # negative = field weakening
    id_max: float =  0.30
    id_step: float = 0.02

    iq_min: float = -0.20
    iq_max: float =  0.20
    iq_step: float = 0.02

    def clamp_speed(self, val: float) -> float:
        return max(self.speed_min, min(self.speed_max, val))

    def clamp_id(self, val: float) -> float:
        return max(self.id_min, min(self.id_max, val))

    def clamp_iq(self, val: float) -> float:
        return max(self.iq_min, min(self.iq_max, val))


@dataclass
class MotorCommand:
    """Snapshot of all values to send to the MCU.

    Mirrors the global sync variables consumed by runSyncControl():
      ctrlState, speedRef, IdRef, IqRef
    """
    ctrl_state: CtrlState = CtrlState.STOP
    speed_ref: float = 0.0   # per-unit, typical range ±0.3
    id_ref: float = 0.0      # d-axis current reference
    iq_ref: float = 0.0      # q-axis current reference (torque)

    def apply_limits(self, limits: CommandLimits):
        """Clamp all values to the configured limits in-place."""
        self.speed_ref = limits.clamp_speed(self.speed_ref)
        self.id_ref = limits.clamp_id(self.id_ref)
        self.iq_ref = limits.clamp_iq(self.iq_ref)

    def __str__(self):
        return (f"ctrl={self.ctrl_state.name} "
                f"spd={self.speed_ref:+.4f} "
                f"Id={self.id_ref:+.4f} Iq={self.iq_ref:+.4f}")


@dataclass
class MusicCommand:
    """Command set for the musical inverter mode.

    Instead of conventional motor control, we drive the inverter to produce
    audible tones by spinning at specific electrical frequencies.
    """
    ctrl_state: CtrlState = CtrlState.STOP
    note: str = ""              # active note name, e.g. "A4"
    freq_hz: float = 0.0       # target electrical frequency in Hz
    freq_pu: float = 0.0       # same frequency in per-unit (freq_hz / base)
    iq_ref: float = 0.0        # torque — controls "volume" / intensity
    id_ref: float = 0.0        # placeholder: may need flux for audibility
    sustain: bool = False       # placeholder: hold note after button release
    # TODO: envelope shaping (attack/decay via iq ramp)
    # TODO: vibrato (small freq modulation from stick)
    # TODO: chord support (multiple simultaneous notes)

    def __str__(self):
        note_str = self.note if self.note else "---"
        return (f"ctrl={self.ctrl_state.name} "
                f"note={note_str} "
                f"f={self.freq_hz:.1f}Hz "
                f"Iq={self.iq_ref:+.4f}")
