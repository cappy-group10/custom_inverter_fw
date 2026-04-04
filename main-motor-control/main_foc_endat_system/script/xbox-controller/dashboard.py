#!/usr/bin/env python3
"""Local FastAPI dashboard for the Xbox drive-mode handler."""

from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager, suppress
from pathlib import Path

import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from serial.tools import list_ports

from commands import CtrlState
from runtime import DriveRuntime
from runtime_models import to_payload


STATIC_DIR = Path(__file__).with_name("dashboard_static")
FRONTEND_ENTRY = STATIC_DIR / "index.html"
UNKNOWN_PORT_TEXT = {"", "n/a", "none", "unknown"}
PRIMARY_MCU_ID = "primary"


class StartSessionRequest(BaseModel):
    """Request body for starting the drive runtime."""

    port: str | None = None
    baudrate: int = Field(default=115200, ge=1)
    joystick_index: int = Field(default=0, ge=0)


class EventHub:
    """Fan-out hub for websocket clients."""

    def __init__(self):
        self._loop: asyncio.AbstractEventLoop | None = None
        self._subscribers: set[asyncio.Queue] = set()
        self._lock = asyncio.Lock()

    async def bind_loop(self):
        self._loop = asyncio.get_running_loop()

    async def subscribe(self) -> asyncio.Queue:
        queue: asyncio.Queue = asyncio.Queue(maxsize=256)
        async with self._lock:
            self._subscribers.add(queue)
        return queue

    async def unsubscribe(self, queue: asyncio.Queue):
        async with self._lock:
            self._subscribers.discard(queue)

    async def publish(self, event: dict):
        async with self._lock:
            subscribers = list(self._subscribers)
        for queue in subscribers:
            if queue.full():
                try:
                    queue.get_nowait()
                except asyncio.QueueEmpty:
                    pass
            try:
                queue.put_nowait(event)
            except asyncio.QueueFull:
                pass

    def publish_from_thread(self, event: dict):
        if self._loop is None:
            return
        self._loop.call_soon_threadsafe(asyncio.create_task, self.publish(event))


class DashboardServer(uvicorn.Server):
    """Uvicorn server that tells the drive runtime to stop on process exit."""

    def __init__(self, runtime: DriveRuntime, config: uvicorn.Config):
        super().__init__(config)
        self._runtime = runtime

    def handle_exit(self, sig: int, frame) -> None:
        self._runtime.request_shutdown()
        super().handle_exit(sig, frame)


def _clean_port_text(value: str | None) -> str:
    """Normalize empty or placeholder serial metadata."""

    if value is None:
        return ""
    text = value.strip()
    if text.lower() in UNKNOWN_PORT_TEXT:
        return ""
    return text


def _classify_port(port_info, app_uart_device: str | None = None, debug_uart_device: str | None = None) -> dict:
    """Classify the operator intent for a serial device."""

    device = port_info.device
    basename = Path(device).name
    description = _clean_port_text(getattr(port_info, "description", None))
    manufacturer = _clean_port_text(getattr(port_info, "manufacturer", None))
    hwid = _clean_port_text(getattr(port_info, "hwid", None))
    combined = " ".join(part.lower() for part in (device, basename, description, manufacturer, hwid))

    if device == app_uart_device:
        return {
            "category": "app_uart",
            "label": f"LaunchXL App UART (Recommended) ({basename})",
            "usage": "Best match for live MCU telemetry and host command traffic.",
            "recommended": True,
            "possible_mcu_uart": True,
        }
    if device == debug_uart_device:
        return {
            "category": "debug_uart",
            "label": f"LaunchXL Debug UART (Usually Console Only) ({basename})",
            "usage": "Likely the XDS110 debug console. Your motor-control data is usually on the App UART instead.",
            "recommended": False,
            "possible_mcu_uart": False,
        }
    if "bluetooth-incoming-port" in combined:
        return {
            "category": "system",
            "label": "macOS Bluetooth Incoming Port (Not MCU UART)",
            "usage": "macOS system port. This is not the LaunchXL or MCU serial link.",
            "recommended": False,
            "possible_mcu_uart": False,
        }
    if "debug-console" in combined:
        return {
            "category": "system",
            "label": "macOS Debug Console (Not MCU UART)",
            "usage": "macOS system/debug pseudo-port. This is not the LaunchXL application UART.",
            "recommended": False,
            "possible_mcu_uart": False,
        }
    if "wlan-debug" in combined:
        return {
            "category": "system",
            "label": "macOS WLAN Debug Port (Not MCU UART)",
            "usage": "macOS system/debug pseudo-port. This is not the LaunchXL application UART.",
            "recommended": False,
            "possible_mcu_uart": False,
        }
    if "usbmodem" in combined:
        return {
            "category": "usb_serial",
            "label": f"USB Modem Serial (Possible MCU UART) ({basename})",
            "usage": "Looks like a USB modem-style serial device. This could be the MCU UART if your board is connected.",
            "recommended": False,
            "possible_mcu_uart": True,
        }
    if "usbserial" in combined:
        return {
            "category": "usb_serial",
            "label": f"USB Serial Adapter (Possible MCU UART) ({basename})",
            "usage": "Looks like a USB serial adapter. This could be the MCU UART if your board uses FTDI/CP210x-style USB serial.",
            "recommended": False,
            "possible_mcu_uart": True,
        }
    if description:
        label = f"{description} ({basename})"
    elif manufacturer:
        label = f"{manufacturer} Serial Device ({basename})"
    else:
        label = f"Serial Port ({basename})"
    return {
        "category": "other",
        "label": label,
        "usage": "Unclassified serial device. Verify the board identity before using it for MCU telemetry.",
        "recommended": False,
        "possible_mcu_uart": False,
    }


def _friendly_port_name(port_info, app_uart_device: str | None = None, debug_uart_device: str | None = None) -> str:
    """Build an operator-friendly label for the serial-port dropdown."""

    return _classify_port(
        port_info,
        app_uart_device=app_uart_device,
        debug_uart_device=debug_uart_device,
    )["label"]


def _port_description(port_info) -> str:
    """Build supplementary detail for the selected port."""

    details = [f"Path: {port_info.device}"]
    description = _clean_port_text(getattr(port_info, "description", None))
    manufacturer = _clean_port_text(getattr(port_info, "manufacturer", None))
    hwid = _clean_port_text(getattr(port_info, "hwid", None))

    if description:
        details.append(f"Info: {description}")
    if manufacturer:
        details.append(f"Vendor: {manufacturer}")
    if hwid:
        details.append(f"HWID: {hwid}")
    return " | ".join(details)


def _likely_launchxl_roles(port_infos: list) -> tuple[str | None, str | None]:
    """Apply the existing XDS110 heuristic: higher-numbered modem is app UART."""

    modem_ports = []
    for port_info in port_infos:
        device = getattr(port_info, "device", "")
        combined = " ".join(
            _clean_port_text(getattr(port_info, field, None)).lower()
            for field in ("description", "manufacturer", "hwid")
        )
        if "usbmodem" not in device.lower():
            continue
        if any(token in combined for token in ("texas instruments", "xds", "launchxl", "0451")):
            modem_ports.append(device)

    if len(modem_ports) < 2:
        return None, None

    modem_ports.sort()
    return modem_ports[-1], modem_ports[0]


def _list_ports_payload():
    port_infos = list(list_ports.comports())
    app_uart_device, debug_uart_device = _likely_launchxl_roles(port_infos)
    ports = [
        {
            "port": "demo",
            "label": "Demo Mode (No MCU Serial)",
            "description": "Use the Xbox controller without connecting the MCU UART",
            "is_demo": True,
            "category": "demo",
            "recommended": False,
            "possible_mcu_uart": False,
            "usage": "Controller-only mode. Host TX runs without connecting the MCU serial port.",
        }
    ]
    for port_info in port_infos:
        classification = _classify_port(
            port_info,
            app_uart_device=app_uart_device,
            debug_uart_device=debug_uart_device,
        )
        ports.append(
            {
                "port": port_info.device,
                "label": classification["label"],
                "description": _port_description(port_info),
                "hwid": port_info.hwid,
                "is_demo": False,
                "category": classification["category"],
                "recommended": classification["recommended"],
                "possible_mcu_uart": classification["possible_mcu_uart"],
                "usage": classification["usage"],
            }
        )
    return {"ports": ports}


def _serve_frontend() -> FileResponse:
    return FileResponse(FRONTEND_ENTRY)


def _ctrl_state_name(value) -> str:
    if isinstance(value, str):
        return value
    if value is None:
        return "STOP"
    try:
        return CtrlState(int(value)).name
    except Exception:
        return str(value)


def _session_has_primary_mcu(snapshot) -> bool:
    return snapshot.session_state in {"starting", "running", "error"} and snapshot.started_at is not None


def _build_mcu_summary(snapshot) -> dict | None:
    if not _session_has_primary_mcu(snapshot):
        return None

    health = snapshot.health or {}
    latest_status = snapshot.latest_mcu_status
    command = snapshot.last_host_command
    ctrl_source = getattr(latest_status, "ctrl_state", None)
    if ctrl_source is None and command is not None:
        ctrl_source = getattr(command, "ctrl_state", None)

    return {
        "id": PRIMARY_MCU_ID,
        "name": "Primary MCU",
        "detail_path": f"/mcu/{PRIMARY_MCU_ID}",
        "configure_path": "/configure",
        "port": snapshot.port or "demo",
        "session_state": snapshot.session_state,
        "controller_connected": snapshot.controller_connected,
        "is_demo": snapshot.port is None,
        "has_mcu_telemetry": bool(health.get("has_mcu_telemetry", False)),
        "telemetry_stale": bool(health.get("telemetry_stale", False)),
        "last_frame_at": health.get("last_frame_at"),
        "ctrl_state": _ctrl_state_name(ctrl_source),
        "active_override": snapshot.active_override,
    }


def _build_mcu_detail(snapshot, mcu_id: str) -> dict:
    if mcu_id != PRIMARY_MCU_ID:
        raise KeyError(mcu_id)

    summary = _build_mcu_summary(snapshot)
    if summary is None:
        raise LookupError("No active MCU session")

    latest_status = snapshot.latest_mcu_status
    command = snapshot.last_host_command
    counters = snapshot.counters or {}
    health = snapshot.health or {}

    return {
        **summary,
        "baudrate": snapshot.baudrate,
        "joystick_index": snapshot.joystick_index,
        "joystick_name": snapshot.joystick_name,
        "command": to_payload(command) if command is not None else None,
        "status": to_payload(latest_status) if latest_status is not None else None,
        "telemetry": {
            "speed_ref": float(getattr(latest_status, "speed_ref", getattr(command, "speed_ref", 0.0))),
            "current_as": float(getattr(latest_status, "current_as", 0.0)),
            "current_bs": float(getattr(latest_status, "current_bs", 0.0)),
            "current_cs": float(getattr(latest_status, "current_cs", 0.0)),
            "vdc_bus": float(getattr(latest_status, "vdc_bus", 0.0)),
            "temperature_c": None,
            "temperature_available": False,
        },
        "transport": {
            "tx_frames": int(counters.get("tx_frames", 0)),
            "rx_frames": int(counters.get("rx_frames", 0)),
            "checksum_errors": int(counters.get("checksum_errors", 0)),
            "serial_errors": int(counters.get("serial_errors", 0)),
            "last_frame_at": health.get("last_frame_at"),
            "last_status_at": health.get("last_status_at"),
        },
    }


def create_app(runtime: DriveRuntime | None = None) -> FastAPI:
    """Create the FastAPI application."""

    hub = EventHub()
    runtime = runtime or DriveRuntime()
    if hasattr(runtime, "set_event_callback"):
        runtime.set_event_callback(hub.publish_from_thread)

    @asynccontextmanager
    async def lifespan(_app: FastAPI):
        await hub.bind_loop()
        try:
            yield
        finally:
            snapshot = runtime.get_snapshot()
            if snapshot.session_state in {"running", "starting", "error"}:
                runtime.set_event_callback(None)
                await runtime.stop_async()

    app = FastAPI(title="Xbox Drive Dashboard", lifespan=lifespan)

    app.state.runtime = runtime
    app.state.hub = hub

    @app.get("/api/ports")
    async def get_ports():
        return _list_ports_payload()

    @app.get("/api/state")
    async def get_state():
        return to_payload(app.state.runtime.get_snapshot())

    @app.post("/api/session/start")
    async def start_session(request: StartSessionRequest):
        try:
            await app.state.runtime.start_async(
                port=request.port,
                baudrate=request.baudrate,
                joystick_index=request.joystick_index,
            )
        except RuntimeError as exc:
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        except Exception as exc:
            raise HTTPException(status_code=500, detail=str(exc)) from exc
        return to_payload(app.state.runtime.get_snapshot())

    @app.post("/api/session/stop")
    async def stop_session():
        return to_payload(await app.state.runtime.stop_async())

    @app.get("/api/mcus")
    async def list_mcus():
        snapshot = app.state.runtime.get_snapshot()
        summary = _build_mcu_summary(snapshot)
        return {"mcus": [summary] if summary else []}

    @app.get("/api/mcus/{mcu_id}")
    async def get_mcu_detail(mcu_id: str):
        snapshot = app.state.runtime.get_snapshot()
        try:
            return _build_mcu_detail(snapshot, mcu_id)
        except KeyError as exc:
            raise HTTPException(status_code=404, detail="MCU not found") from exc
        except LookupError as exc:
            raise HTTPException(status_code=404, detail=str(exc)) from exc

    @app.post("/api/mcus/{mcu_id}/brake")
    async def engage_mcu_brake(mcu_id: str):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.engage_brake_override()
        except RuntimeError as exc:
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        return _build_mcu_detail(snapshot, mcu_id)

    @app.websocket("/api/stream")
    async def stream(websocket: WebSocket):
        await websocket.accept()
        queue = await app.state.hub.subscribe()
        try:
            await websocket.send_json(
                {"type": "snapshot", "payload": to_payload(app.state.runtime.get_snapshot())}
            )
            while True:
                queue_task = asyncio.create_task(queue.get())
                receive_task = asyncio.create_task(websocket.receive())
                done, pending = await asyncio.wait(
                    {queue_task, receive_task},
                    return_when=asyncio.FIRST_COMPLETED,
                )

                for task in pending:
                    task.cancel()
                    with suppress(asyncio.CancelledError):
                        await task

                if receive_task in done:
                    message = receive_task.result()
                    if message.get("type") == "websocket.disconnect":
                        break
                    continue

                event = queue_task.result()
                await websocket.send_json(event)
        except WebSocketDisconnect:
            pass
        finally:
            await app.state.hub.unsubscribe(queue)

    app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

    @app.get("/", include_in_schema=False)
    async def root():
        return _serve_frontend()

    @app.get("/{full_path:path}", include_in_schema=False)
    async def frontend_routes(full_path: str):
        if full_path.startswith("api/") or full_path.startswith("static/"):
            raise HTTPException(status_code=404, detail="Not found")
        return _serve_frontend()

    return app


def main():
    """Run the dashboard locally."""
    runtime = DriveRuntime()
    app = create_app(runtime=runtime)
    config = uvicorn.Config(app, host="127.0.0.1", port=8000, log_level="info")
    server = DashboardServer(runtime=runtime, config=config)
    try:
        server.run()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
