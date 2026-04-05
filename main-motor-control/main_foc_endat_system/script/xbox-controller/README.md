# Xbox Controller for Custom Inverter Control

Drive the inverter from an Xbox controller, stream framed UART commands to the MCU, and inspect the host/MCU link through either the terminal CLI or a local React dashboard.

## Setup

```bash
cd script/xbox-controller
uv sync
npm install
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

### React Dashboard

```bash
uv run xbox-dashboard
```

Then open `http://127.0.0.1:8000` in your browser.

The dashboard can:

- launch the primary MCU session from a landing page
- validate controller state and UART link health under `Configure`
- inspect TX/RX UART traffic in a terminal-style debug panel
- open a dedicated motor page with a speedometer-style speed view, phase currents, DC bus, and an emergency brake override

To rebuild the production frontend after changing the React source:

```bash
npm run build
```

For frontend-only development with Vite:

```bash
npm run dev
```

Keep `uv run xbox-dashboard` running in a separate terminal while using the Vite dev server.

## Helpful Serial Port Discovery

```bash
../find_launchxl_port.sh
```

On macOS LaunchXL/XDS110 setups, the higher-numbered `usbmodem` port is often the application UART.

## Architecture

- `xbox_controller/` — Python backend package for the CLI, runtime, UART transport, logging, and FastAPI dashboard server.
- `xbox_controller/controller.py` — polls pygame, normalizes D-pad input, and stores a `ControllerState`.
- `xbox_controller/mapping.py` — converts controller state into drive or music commands.
- `xbox_controller/uart.py` — packs host commands into frames and parses MCU status/fault frames with thread-safe buffers.
- `xbox_controller/runtime.py` — shared drive-mode control loop used by both the CLI and the dashboard backend.
- `xbox_controller/dashboard.py` — FastAPI app with websocket streaming for the browser UI.
- `src/` — React + TypeScript source for the multi-page operator frontend.
- `dashboard_static/` — built Vite output served by FastAPI in production mode.

## Testing

```bash
npm test
uv run --extra dev pytest
```
