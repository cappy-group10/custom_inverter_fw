#!/usr/bin/env python3
"""Local FastAPI dashboard for the Xbox drive-mode handler."""

from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
from pathlib import Path

import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from serial.tools import list_ports

from runtime import DriveRuntime
from runtime_models import to_payload


STATIC_DIR = Path(__file__).with_name("dashboard_static")
UNKNOWN_PORT_TEXT = {"", "n/a", "none", "unknown"}


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

    @app.get("/")
    async def root():
        return FileResponse(STATIC_DIR / "index.html")

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

    @app.websocket("/api/stream")
    async def stream(websocket: WebSocket):
        await websocket.accept()
        queue = await app.state.hub.subscribe()
        try:
            await websocket.send_json(
                {"type": "snapshot", "payload": to_payload(app.state.runtime.get_snapshot())}
            )
            while True:
                event = await queue.get()
                await websocket.send_json(event)
        except WebSocketDisconnect:
            pass
        finally:
            await app.state.hub.unsubscribe(queue)

    app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")
    return app


def main():
    """Run the dashboard locally."""

    uvicorn.run(create_app(), host="127.0.0.1", port=8000, log_level="info")


if __name__ == "__main__":
    main()
