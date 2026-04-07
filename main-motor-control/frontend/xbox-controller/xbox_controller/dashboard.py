#!/usr/bin/env python3
"""Local FastAPI dashboard for the Xbox drive-mode handler."""

from __future__ import annotations

import asyncio
import time
from contextlib import asynccontextmanager, suppress
from pathlib import Path
from typing import Literal

import uvicorn
from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from serial.tools import list_ports

from .app_logging import create_timestamped_loggers
from .commands import CtrlState
from .dashboard_runtime import DashboardRuntimeManager
from .runtime_models import to_payload


PROJECT_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = PROJECT_DIR / "dashboard_static"
FRONTEND_ENTRY = STATIC_DIR / "index.html"
LOG_DIR = PROJECT_DIR / "log"
UNKNOWN_PORT_TEXT = {"", "n/a", "none", "unknown"}
PRIMARY_MCU_ID = "primary"


class StartSessionRequest(BaseModel):
    """Request body for starting the active dashboard runtime."""

    port: str | None = None
    baudrate: int = Field(default=115200, ge=1)
    joystick_index: int = Field(default=0, ge=0)
    mode: Literal["drive", "music"] = "drive"


class MusicPlayRequest(BaseModel):
    """Request body for starting a predefined musical-motor song."""

    song_id: int = Field(ge=0)
    amplitude: float | None = Field(default=None, ge=0.0, le=1.0)


class MusicVolumeRequest(BaseModel):
    """Request body for updating the global musical-motor volume."""

    volume: float = Field(ge=0.0, le=1.0)


class FrontendLogRecordRequest(BaseModel):
    """Structured frontend log record posted by the React client."""

    timestamp: float | None = None
    level: str = Field(default="info")
    source: str = Field(default="frontend")
    route: str = Field(default="")
    message: str
    metadata: dict = Field(default_factory=dict)
    client_session_id: str | None = None


class FrontendLogBatchRequest(BaseModel):
    """Batch of frontend log records."""

    records: list[FrontendLogRecordRequest] = Field(default_factory=list)


class EventHub:
    """Fan-out hub for websocket clients."""

    def __init__(self):
        self._loop: asyncio.AbstractEventLoop | None = None
        self._subscribers: set[asyncio.Queue] = set()
        self._lock = asyncio.Lock()
        self._inbound: asyncio.Queue | None = None
        self._pump_task: asyncio.Task | None = None

    async def bind_loop(self):
        self._loop = asyncio.get_running_loop()
        self._inbound = asyncio.Queue(maxsize=512)
        self._pump_task = asyncio.create_task(self._pump_inbound())

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
        if self._loop is None or self._inbound is None:
            return
        self._loop.call_soon_threadsafe(self._enqueue_inbound, event)

    def _enqueue_inbound(self, event: dict):
        if self._inbound is None:
            return
        if self._inbound.full():
            try:
                self._inbound.get_nowait()
            except asyncio.QueueEmpty:
                pass
        try:
            self._inbound.put_nowait(event)
        except asyncio.QueueFull:
            pass

    async def _pump_inbound(self):
        if self._inbound is None:
            return
        try:
            while True:
                event = await self._inbound.get()
                await self.publish(event)
        except asyncio.CancelledError:
            return

    async def shutdown(self):
        if self._pump_task is not None:
            self._pump_task.cancel()
            with suppress(asyncio.CancelledError):
                await self._pump_task
            self._pump_task = None
        self._inbound = None


class DashboardServer(uvicorn.Server):
    """Uvicorn server that tells the active runtime to stop on process exit."""

    def __init__(self, runtime, config: uvicorn.Config):
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


def _music_play_state_name(snapshot) -> str:
    music_state = getattr(snapshot, "music_state", None)
    if music_state is None:
        return "IDLE"
    return getattr(music_state, "play_state", "IDLE") or "IDLE"


def _session_has_primary_mcu(snapshot) -> bool:
    return snapshot.session_state in {"starting", "running", "error"} and snapshot.started_at is not None


def _build_mcu_summary(snapshot) -> dict | None:
    if not _session_has_primary_mcu(snapshot):
        return None

    health = snapshot.health or {}
    mode = getattr(snapshot, "mode", "drive") or "drive"

    if mode == "music":
        ctrl_source = _music_play_state_name(snapshot)
        detail_path = "/configure#music"
        configure_path = "/configure#music"
    else:
        latest_status = snapshot.latest_mcu_status
        command = snapshot.last_host_command
        ctrl_source = getattr(latest_status, "ctrl_state", None)
        if ctrl_source is None and command is not None:
            ctrl_source = getattr(command, "ctrl_state", None)
        detail_path = f"/mcu/{PRIMARY_MCU_ID}"
        configure_path = "/configure"

    return {
        "id": PRIMARY_MCU_ID,
        "name": "Primary MCU",
        "detail_path": detail_path,
        "configure_path": configure_path,
        "port": snapshot.port or "demo",
        "session_state": snapshot.session_state,
        "controller_connected": snapshot.controller_connected,
        "is_demo": snapshot.port is None,
        "has_mcu_telemetry": bool(health.get("has_mcu_telemetry", False)),
        "telemetry_stale": bool(health.get("telemetry_stale", False)),
        "last_frame_at": health.get("last_frame_at"),
        "ctrl_state": _ctrl_state_name(ctrl_source),
        "active_override": snapshot.active_override if mode == "drive" else None,
        "mode": mode,
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

    detail = {
        **summary,
        "baudrate": snapshot.baudrate,
        "joystick_index": snapshot.joystick_index,
        "joystick_name": snapshot.joystick_name,
        "motor_config": to_payload(snapshot.motor_config),
        "command": to_payload(command) if command is not None else None,
        "status": to_payload(latest_status) if latest_status is not None else None,
        "telemetry": {
            "speed_ref": float(getattr(latest_status, "speed_ref", getattr(command, "speed_ref", 0.0))),
            "speed_fbk": float(getattr(latest_status, "speed_fbk", 0.0)),
            "id_ref": float(getattr(command, "id_ref", 0.0)),
            "id_fbk": float(getattr(latest_status, "id_fbk", 0.0)),
            "iq_ref": float(getattr(command, "iq_ref", 0.0)),
            "iq_fbk": float(getattr(latest_status, "iq_fbk", 0.0)),
            "current_as": float(getattr(latest_status, "current_as", 0.0)),
            "current_bs": float(getattr(latest_status, "current_bs", 0.0)),
            "current_cs": float(getattr(latest_status, "current_cs", 0.0)),
            "vdc_bus": float(getattr(latest_status, "vdc_bus", 0.0)),
            "temp_motor_winding_c": getattr(latest_status, "temp_motor_winding_c", None),
            "temp_mcu_c": getattr(latest_status, "temp_mcu_c", None),
            "temp_igbts_c": getattr(latest_status, "temp_igbts_c", None),
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
    if getattr(snapshot, "mode", "drive") == "music":
        detail["music_state"] = to_payload(snapshot.music_state)
    return detail


def _build_music_detail(snapshot, mcu_id: str) -> dict:
    if mcu_id != PRIMARY_MCU_ID:
        raise KeyError(mcu_id)

    summary = _build_mcu_summary(snapshot)
    if summary is None or summary.get("mode") != "music":
        raise LookupError("No active music session")

    counters = snapshot.counters or {}
    health = snapshot.health or {}
    return {
        **summary,
        "baudrate": snapshot.baudrate,
        "songs": to_payload(getattr(snapshot.music_state, "songs", [])),
        "music_state": to_payload(snapshot.music_state),
        "transport": {
            "tx_frames": int(counters.get("tx_frames", 0)),
            "rx_frames": int(counters.get("rx_frames", 0)),
            "checksum_errors": int(counters.get("checksum_errors", 0)),
            "serial_errors": int(counters.get("serial_errors", 0)),
            "last_frame_at": health.get("last_frame_at"),
            "last_status_at": health.get("last_status_at"),
        },
        "health": to_payload(health),
    }


def create_app(runtime: DashboardRuntimeManager | None = None, log_dir: Path | None = None) -> FastAPI:
    """Create the FastAPI application."""

    hub = EventHub()
    runtime = runtime or DashboardRuntimeManager()
    backend_logger, frontend_logger = create_timestamped_loggers(log_dir or LOG_DIR)
    if hasattr(runtime, "set_logger"):
        runtime.set_logger(backend_logger)
    if hasattr(runtime, "set_event_callback"):
        runtime.set_event_callback(hub.publish_from_thread)

    @asynccontextmanager
    async def lifespan(_app: FastAPI):
        await hub.bind_loop()
        backend_logger.log(
            "info",
            "backend",
            "Dashboard application startup complete",
            route="startup",
            metadata={"static_dir": STATIC_DIR, "frontend_entry": FRONTEND_ENTRY},
        )
        try:
            yield
        finally:
            backend_logger.log("info", "backend", "Dashboard shutdown requested", route="shutdown")
            snapshot = runtime.get_snapshot()
            if snapshot.session_state in {"running", "starting", "error"}:
                runtime.set_event_callback(None)
                await runtime.stop_async()
            await hub.shutdown()
            backend_logger.log("info", "backend", "Dashboard application shutdown complete", route="shutdown")
            backend_logger.close()
            frontend_logger.close()

    app = FastAPI(title="Xbox Motor Dashboard", lifespan=lifespan)

    app.state.runtime = runtime
    app.state.hub = hub
    app.state.backend_logger = backend_logger
    app.state.frontend_logger = frontend_logger

    @app.middleware("http")
    async def log_api_requests(request: Request, call_next):
        if not request.url.path.startswith("/api/"):
            return await call_next(request)

        started_at = time.perf_counter()
        try:
            response = await call_next(request)
        except Exception as exc:
            backend_logger.log(
                "error",
                "api",
                "API request failed",
                route=request.url.path,
                metadata={
                    "method": request.method,
                    "duration_ms": round((time.perf_counter() - started_at) * 1000, 3),
                    "error": str(exc),
                },
            )
            raise

        backend_logger.log(
            "info",
            "api",
            "API request completed",
            route=request.url.path,
            metadata={
                "method": request.method,
                "status_code": response.status_code,
                "duration_ms": round((time.perf_counter() - started_at) * 1000, 3),
            },
        )
        return response

    @app.get("/api/ports")
    async def get_ports():
        return _list_ports_payload()

    @app.get("/api/state")
    async def get_state():
        return to_payload(app.state.runtime.get_snapshot())

    @app.post("/api/session/start")
    async def start_session(request: StartSessionRequest):
        backend_logger.log(
            "info",
            "session",
            "Session start requested",
            route="/api/session/start",
            metadata={
                "port": request.port or "demo",
                "baudrate": request.baudrate,
                "joystick_index": request.joystick_index,
                "mode": request.mode,
            },
        )
        try:
            await app.state.runtime.start_async(
                port=request.port,
                baudrate=request.baudrate,
                joystick_index=request.joystick_index,
                mode=request.mode,
            )
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Session start rejected",
                route="/api/session/start",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        except Exception as exc:
            backend_logger.log(
                "error",
                "session",
                "Session start failed",
                route="/api/session/start",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=500, detail=str(exc)) from exc
        backend_logger.log("info", "session", "Session start succeeded", route="/api/session/start")
        return to_payload(app.state.runtime.get_snapshot())

    @app.post("/api/session/stop")
    async def stop_session():
        backend_logger.log("info", "session", "Session stop requested", route="/api/session/stop")
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

    @app.get("/api/mcus/{mcu_id}/music")
    async def get_music_detail(mcu_id: str):
        snapshot = app.state.runtime.get_snapshot()
        try:
            return _build_music_detail(snapshot, mcu_id)
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
            backend_logger.log(
                "warning",
                "session",
                "Brake override rejected",
                route=f"/api/mcus/{mcu_id}/brake",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        backend_logger.log(
            "warning",
            "session",
            "Brake override engaged",
            route=f"/api/mcus/{mcu_id}/brake",
            metadata={"mcu_id": mcu_id},
        )
        return _build_mcu_detail(snapshot, mcu_id)

    @app.post("/api/mcus/{mcu_id}/brake/release")
    async def release_mcu_brake(mcu_id: str):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.release_brake_override()
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Brake release rejected",
                route=f"/api/mcus/{mcu_id}/brake/release",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        backend_logger.log(
            "info",
            "session",
            "Brake override released",
            route=f"/api/mcus/{mcu_id}/brake/release",
            metadata={"mcu_id": mcu_id},
        )
        return _build_mcu_detail(snapshot, mcu_id)

    @app.post("/api/mcus/{mcu_id}/music/play")
    async def play_music(mcu_id: str, request: MusicPlayRequest):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.play_music(request.song_id, amplitude=request.amplitude)
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Music play rejected",
                route=f"/api/mcus/{mcu_id}/music/play",
                metadata={"error": str(exc), "song_id": request.song_id},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        backend_logger.log(
            "info",
            "session",
            "Music play accepted",
            route=f"/api/mcus/{mcu_id}/music/play",
            metadata={"song_id": request.song_id, "mode": "music"},
        )
        return to_payload(snapshot)

    @app.post("/api/mcus/{mcu_id}/music/pause")
    async def pause_music(mcu_id: str):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.pause_music()
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Music pause rejected",
                route=f"/api/mcus/{mcu_id}/music/pause",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        return to_payload(snapshot)

    @app.post("/api/mcus/{mcu_id}/music/resume")
    async def resume_music(mcu_id: str):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.resume_music()
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Music resume rejected",
                route=f"/api/mcus/{mcu_id}/music/resume",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        return to_payload(snapshot)

    @app.post("/api/mcus/{mcu_id}/music/stop")
    async def stop_music(mcu_id: str):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.stop_music()
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Music stop rejected",
                route=f"/api/mcus/{mcu_id}/music/stop",
                metadata={"error": str(exc)},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        return to_payload(snapshot)

    @app.post("/api/mcus/{mcu_id}/music/volume")
    async def set_music_volume(mcu_id: str, request: MusicVolumeRequest):
        if mcu_id != PRIMARY_MCU_ID:
            raise HTTPException(status_code=404, detail="MCU not found")
        try:
            snapshot = app.state.runtime.set_music_volume(request.volume)
        except RuntimeError as exc:
            backend_logger.log(
                "warning",
                "session",
                "Music volume rejected",
                route=f"/api/mcus/{mcu_id}/music/volume",
                metadata={"error": str(exc), "volume": request.volume},
            )
            raise HTTPException(status_code=409, detail=str(exc)) from exc
        return to_payload(snapshot)

    @app.post("/api/logs/frontend")
    async def ingest_frontend_logs(payload: FrontendLogBatchRequest):
        accepted = 0
        for record in payload.records:
            frontend_logger.log(
                record.level,
                record.source,
                record.message,
                route=record.route,
                metadata={
                    **record.metadata,
                    "client_session_id": record.client_session_id,
                    "client_timestamp": record.timestamp,
                },
            )
            accepted += 1
        if accepted:
            backend_logger.log(
                "info",
                "frontend_logs",
                "Frontend log batch ingested",
                route="/api/logs/frontend",
                metadata={"accepted": accepted},
            )
        return {"accepted": accepted}

    @app.websocket("/api/stream")
    async def stream(websocket: WebSocket):
        await websocket.accept()
        backend_logger.log(
            "info",
            "websocket",
            "Dashboard websocket connected",
            route="/api/stream",
            metadata={"client": getattr(websocket.client, "host", "unknown")},
        )
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
            backend_logger.log("info", "websocket", "Dashboard websocket disconnected", route="/api/stream")
        finally:
            await app.state.hub.unsubscribe(queue)
            backend_logger.log("info", "websocket", "Dashboard websocket cleanup complete", route="/api/stream")

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
    runtime = DashboardRuntimeManager()
    app = create_app(runtime=runtime)
    config = uvicorn.Config(app, host="127.0.0.1", port=8000, log_level="info")
    server = DashboardServer(runtime=runtime, config=config)
    try:
        server.run()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
