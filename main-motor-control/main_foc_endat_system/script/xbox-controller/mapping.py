#!/usr/bin/env python3
"""Customizable input mappings from Xbox controller to motor commands."""

from abc import ABC, abstractmethod

from commands import MotorCommand, MusicCommand, CtrlState, CommandLimits
from controller import ControllerState, ButtonEvent, ButtonEdge


class InputMapping(ABC):
    """Base class for controller-to-command mappings.

    Subclass this and override `process()` to create new mappings
    (e.g. DriveMapping for motor control, MusicalMapping for notes).
    """

    @abstractmethod
    def process(self, state: ControllerState,
                events: list[ButtonEvent]) -> MotorCommand | MusicCommand:
        """Consume the current controller state + buffered button events
        and return the command to send to the MCU."""
        ...


# ---------------------------------------------------------------------------
#  Default button bindings — override by passing a dict to DriveMapping()
# ---------------------------------------------------------------------------
DEFAULT_DRIVE_BINDINGS: dict[str, str] = {
    # button_name  -> action
    "a":           "run",
    "b":           "stop",
    "x":           "brake",
    "y":           "reset",
    "start":       "run",
    "back":        "stop",
    "dpad_up":     "iq_up",        # btn_11: increase Iq
    "dpad_down":   "iq_down",      # btn_12: decrease Iq
    "dpad_left":   "id_down",      # btn_13: decrease Id (field weakening)
    "dpad_right":  "id_up",        # btn_14: increase Id
}


class DriveMapping(InputMapping):
    """Standard motor-drive mapping.

    Left stick Y  -> speed_ref  (up/LY=-1 = max forward, down/LY=+1 = max reverse)
    D-pad up/down -> Iq step    (btn_11 = increase, btn_12 = decrease)
    D-pad R/L     -> Id step    (btn_14 = increase, btn_13 = decrease / field weaken)
    A/Start       -> run        B/Back -> stop    X -> brake    Y -> reset

    All button-to-action assignments live in `self.bindings`.
    All variable ranges and increments live in `self.limits`.
    Both can be replaced at construction time or at runtime.
    """

    def __init__(
        self,
        bindings: dict[str, str] | None = None,
        limits: CommandLimits | None = None,
    ):
        self.bindings = bindings if bindings is not None else dict(DEFAULT_DRIVE_BINDINGS)
        self.limits = limits if limits is not None else CommandLimits()

        self._cmd = MotorCommand(
            ctrl_state=CtrlState.STOP,
            speed_ref=0.0,
            id_ref=0.0,
            iq_ref=0.0,
        )

    # -- helpers -----------------------------------------------------------

    def _event_matches(self, event: ButtonEvent, action: str) -> bool:
        return (event.edge == ButtonEdge.PRESSED
                and self.bindings.get(event.button) == action)

    # -- main entry point --------------------------------------------------

    def process(self, state: ControllerState,
                events: list[ButtonEvent]) -> MotorCommand:
        lim = self.limits

        # --- discrete button events ---------------------------------------
        for ev in events:
            if self._event_matches(ev, "run"):
                self._cmd.ctrl_state = CtrlState.RUN

            elif self._event_matches(ev, "stop"):
                self._cmd.ctrl_state = CtrlState.STOP

            elif self._event_matches(ev, "brake"):
                self._cmd.ctrl_state = CtrlState.BRAKE

            elif self._event_matches(ev, "reset"):
                self._cmd.ctrl_state = CtrlState.RESET
                self._cmd.id_ref = 0.0
                self._cmd.iq_ref = 0.0

            elif self._event_matches(ev, "iq_up"):
                self._cmd.iq_ref += lim.iq_step

            elif self._event_matches(ev, "iq_down"):
                self._cmd.iq_ref -= lim.iq_step

            elif self._event_matches(ev, "id_up"):
                self._cmd.id_ref += lim.id_step

            elif self._event_matches(ev, "id_down"):
                self._cmd.id_ref -= lim.id_step

        # --- continuous axes ----------------------------------------------
        # Left stick Y -> speed_ref (negate: stick up = LY -1 = positive speed)
        self._cmd.speed_ref = -state.left_y * lim.speed_max

        # --- enforce limits on every output -------------------------------
        self._cmd.apply_limits(lim)

        return self._cmd


# ---------------------------------------------------------------------------
#  Musical-inverter mapping (skeleton — fill in note frequencies later)
# ---------------------------------------------------------------------------
DEFAULT_MUSIC_BINDINGS: dict[str, str] = {
    "a":  "C4",
    "b":  "D4",
    "x":  "E4",
    "y":  "F4",
    "lb": "G4",
    "rb": "A4",
    "back":  "B4",
    "start": "C5",
}

# Note -> electrical frequency in per-unit (you'll scale to your base freq)
NOTE_FREQ_HZ: dict[str, float] = {
    "C4":  261.63,
    "D4":  293.66,
    "E4":  329.63,
    "F4":  349.23,
    "G4":  392.00,
    "A4":  440.00,
    "B4":  493.88,
    "C5":  523.25,
}


class MusicalMapping(InputMapping):
    """Maps each button to a musical note (frequency).

    Press a button -> motor spins at the corresponding electrical frequency.
    Release all    -> motor stops.
    Returns a MusicCommand instead of MotorCommand.
    """

    def __init__(
        self,
        bindings: dict[str, str] | None = None,
        base_freq_hz: float = 1000.0,  # motor rated electrical freq for per-unit
        iq_intensity: float = 0.10,     # "volume" — torque while playing a note
        id_flux: float = 0.0,           # placeholder: flux bias for audibility
    ):
        self.bindings = bindings if bindings is not None else dict(DEFAULT_MUSIC_BINDINGS)
        self.base_freq_hz = base_freq_hz
        self.iq_intensity = iq_intensity
        self.id_flux = id_flux
        self._cmd = MusicCommand()

    def process(self, state: ControllerState,
                events: list[ButtonEvent]) -> MusicCommand:

        # Find which note button is currently held
        note = ""
        for btn_name, n in self.bindings.items():
            if state.buttons.get(btn_name, False):
                note = n
                break  # highest-priority = first match

        if note and note in NOTE_FREQ_HZ:
            freq_hz = NOTE_FREQ_HZ[note]
            self._cmd.ctrl_state = CtrlState.RUN
            self._cmd.note = note
            self._cmd.freq_hz = freq_hz
            self._cmd.freq_pu = freq_hz / self.base_freq_hz
            self._cmd.iq_ref = self.iq_intensity
            self._cmd.id_ref = self.id_flux
            # TODO: right stick Y -> iq_ref for live volume control
            # TODO: left stick X -> slight freq detune for vibrato
        else:
            self._cmd.ctrl_state = CtrlState.STOP
            self._cmd.note = ""
            self._cmd.freq_hz = 0.0
            self._cmd.freq_pu = 0.0
            self._cmd.iq_ref = 0.0
            self._cmd.id_ref = 0.0

        return self._cmd
