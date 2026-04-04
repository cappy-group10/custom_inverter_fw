# Xbox Controller for Custom Inverter Control

Drive the inverter from an Xbox controller, stream framed UART commands to the MCU, and inspect the host/MCU link through either the terminal CLI or a local browser dashboard.

## Setup

```bash
cd script/xbox-controller
uv sync
```

To install the test dependencies as well:

```bash
uv sync --extra dev
```

## Usage

Connect the Xbox controller, then choose one of the following entrypoints.

### Drive CLI

```bash
uv run xbox-ctrl drive --port /dev/tty.usbmodemXXXX
```

Omit `--port` to run in demo / terminal-only TX mode.

### Browser Dashboard

```bash
uv run xbox-dashboard
```

Then open `http://127.0.0.1:8000` in your browser.

The dashboard can:

- start or stop the drive runtime
- visualize controller input live
- show host TX frames and raw UART bytes
- decode MCU status and fault frames when telemetry is present
- plot rolling telemetry charts for speed, d/q currents, DC bus, and phase currents

## Helpful Serial Port Discovery

```bash
../find_launchxl_port.sh
```

On macOS LaunchXL/XDS110 setups, the higher-numbered `usbmodem` port is often the application UART.

## Architecture

- `controller.py` — polls pygame, normalizes D-pad input, and stores a `ControllerState`.
- `mapping.py` — converts controller state into drive or music commands.
- `uart.py` — packs host commands into frames and parses MCU status/fault frames with thread-safe buffers.
- `runtime.py` — shared drive-mode control loop used by both the CLI and the dashboard backend.
- `dashboard.py` — FastAPI app with websocket streaming for the browser UI.
- `dashboard_static/` — static HTML/CSS/JS frontend for the local dashboard.

## Testing

```bash
uv run --extra dev pytest
```
