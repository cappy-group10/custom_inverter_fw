#!/usr/bin/env python3
"""Xbox controller input handler using pygame."""

import pygame
from dataclasses import dataclass, field
from typing import Dict


@dataclass
class ControllerState:
    """Snapshot of all controller inputs."""
    # Axes as raw floats (-1.0 to 1.0)
    left_x: float = 0.0
    left_y: float = 0.0
    right_x: float = 0.0
    right_y: float = 0.0
    left_trigger: float = 0.0
    right_trigger: float = 0.0

    # Button states (True = pressed)
    buttons: Dict[str, bool] = field(default_factory=dict)

    # D-pad
    dpad: tuple = (0, 0)  # (x, y) each -1, 0, or 1

    def left_x_u8(self) -> int:
        """Left X axis scaled to 0-255."""
        return int((self.left_x + 1.0) * 127.5)

    def left_y_u8(self) -> int:
        """Left Y axis scaled to 0-255."""
        return int((self.left_y + 1.0) * 127.5)

    def right_x_u8(self) -> int:
        """Right X axis scaled to 0-255."""
        return int((self.right_x + 1.0) * 127.5)

    def right_y_u8(self) -> int:
        """Right Y axis scaled to 0-255."""
        return int((self.right_y + 1.0) * 127.5)

    def buttons_mask(self) -> int:
        """All buttons packed into a 16-bit bitmask."""
        mask = 0
        for i, pressed in enumerate(self.buttons.values()):
            if pressed:
                mask |= (1 << i)
        return mask


# Xbox controller button mapping (pygame index -> name)
# Matches Xbox Wireless Controller on macOS via pygame
XBOX_BUTTON_MAP = {
    0: "a",
    1: "b",
    2: "x",
    3: "y",
    4: "lb",
    5: "rb",
    6: "back",
    7: "start",
    8: "guide",
    9: "left_stick",
    10: "right_stick",
}

# Axis mapping
AXIS_LEFT_X = 0
AXIS_LEFT_Y = 1
AXIS_RIGHT_X = 2   # may be 3 on some systems
AXIS_RIGHT_Y = 3   # may be 4 on some systems
AXIS_LEFT_TRIGGER = 4   # may be 2 on some systems
AXIS_RIGHT_TRIGGER = 5  # may be 5 on some systems


class XboxController:
    """Reads and stores Xbox controller input via pygame.

    Usage:
        ctrl = XboxController()
        ctrl.connect()
        while True:
            ctrl.poll()
            print(ctrl.state)
    """

    def __init__(self, joystick_index: int = 0, deadzone: float = 0.05):
        self._index = joystick_index
        self._deadzone = deadzone
        self._joystick = None  # type: pygame.joystick.JoystickType | None
        self.state = ControllerState()
        self.connected = False

    def connect(self):
        """Initialize pygame and connect to the controller."""
        pygame.init()
        pygame.joystick.init()

        count = pygame.joystick.get_count()
        if count == 0:
            raise RuntimeError("No joystick detected. Is the Xbox controller connected?")

        if self._index >= count:
            raise RuntimeError(
                f"Joystick index {self._index} out of range (found {count} controller(s))"
            )

        self._joystick = pygame.joystick.Joystick(self._index)
        self._joystick.init()
        self.connected = True

        # Pre-populate button dict with names
        for i in range(self._joystick.get_numbuttons()):
            name = XBOX_BUTTON_MAP.get(i, f"btn_{i}")
            self.state.buttons[name] = False

        print(f"Connected: {self._joystick.get_name()}")
        print(f"  Axes: {self._joystick.get_numaxes()}, "
              f"Buttons: {self._joystick.get_numbuttons()}, "
              f"Hats: {self._joystick.get_numhats()}")

    def disconnect(self):
        """Clean up pygame resources."""
        if self._joystick:
            self._joystick.quit()
            self._joystick = None
        self.connected = False
        pygame.quit()

    def _apply_deadzone(self, value: float) -> float:
        if abs(value) < self._deadzone:
            return 0.0
        return value

    def poll(self):
        """Read the latest input from the controller and update self.state."""
        if not self.connected or not self._joystick:
            raise RuntimeError("Controller not connected. Call connect() first.")

        pygame.event.pump()

        js = self._joystick
        num_axes = js.get_numaxes()

        # Axes
        if num_axes > AXIS_LEFT_X:
            self.state.left_x = self._apply_deadzone(js.get_axis(AXIS_LEFT_X))
        if num_axes > AXIS_LEFT_Y:
            self.state.left_y = self._apply_deadzone(js.get_axis(AXIS_LEFT_Y))
        if num_axes > AXIS_RIGHT_X:
            self.state.right_x = self._apply_deadzone(js.get_axis(AXIS_RIGHT_X))
        if num_axes > AXIS_RIGHT_Y:
            self.state.right_y = self._apply_deadzone(js.get_axis(AXIS_RIGHT_Y))
        if num_axes > AXIS_LEFT_TRIGGER:
            self.state.left_trigger = js.get_axis(AXIS_LEFT_TRIGGER)
        if num_axes > AXIS_RIGHT_TRIGGER:
            self.state.right_trigger = js.get_axis(AXIS_RIGHT_TRIGGER)

        # Buttons
        for i in range(js.get_numbuttons()):
            name = XBOX_BUTTON_MAP.get(i, f"btn_{i}")
            self.state.buttons[name] = bool(js.get_button(i))

        # D-pad (hat)
        if js.get_numhats() > 0:
            self.state.dpad = js.get_hat(0)
