# Xbox Controller for Custom Inverter Control

Reads Xbox controller input via pygame and exposes it as structured state for commanding the inverter over UART.

## Setup

```bash
cd script/xbox-controller
uv sync
```

## Usage

Connect the Xbox controller via Bluetooth, then:

```bash
uv run python main.py
```

This prints live axis, trigger, button, and d-pad values to the terminal.

## Architecture

- `controller.py` — `XboxController` class that polls the gamepad and stores input in a `ControllerState` dataclass. Provides raw float axes, 0-255 scaled helpers, and a packed button bitmask.
- `main.py` — Terminal demo that prints controller state at 60 Hz.

UART transport to the LaunchXL board will be added as a separate module that consumes `ControllerState`.
