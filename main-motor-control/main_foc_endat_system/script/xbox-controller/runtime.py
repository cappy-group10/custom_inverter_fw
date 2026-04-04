#!/usr/bin/env python3
"""Shared drive-mode runtime used by the CLI and browser dashboard."""

from __future__ import annotations

import asyncio
import threading
import time
from collections import deque
from copy import deepcopy
from typing import Any, Callable

from commands import MotorCommand
from controller import ButtonEdge, XboxController
from mapping import DriveMapping
from runtime_models import EventRecord, SessionSnapshot, TelemetrySample, to_payload
from uart import FRAME_FAULT, FRAME_STATUS, MCUFault, MCUStatus, UARTLink


POLL_RATE_HZ = 60
UI_SAMPLE_HZ = 10
TELEMETRY_STALE_SECONDS = 1.0


class DriveRuntime:
    """Runs the drive-mode controller + UART loop in one background thread."""

    def __init__(
        self,
        poll_rate_hz: int = POLL_RATE_HZ,
        ui_sample_hz: int = UI_SAMPLE_HZ,
        deadzone: float = 0.08,
        frame_history: int = 200,
        event_history: int = 50,
        telemetry_history: int = 300,
        controller_factory: Callable[..., Any] | None = None,
        mapping_factory: Callable[[], Any] | None = None,
        link_factory: Callable[..., Any] | None = None,
        event_callback: Callable[[dict[str, Any]], None] | None = None,
    ):
        self._poll_interval = 1.0 / poll_rate_hz
        self._ui_sample_interval = 1.0 / ui_sample_hz
        self._deadzone = deadzone
        self._controller_factory = controller_factory or XboxController
        self._mapping_factory = mapping_factory or DriveMapping
        self._link_factory = link_factory or UARTLink
        self._event_callback = event_callback

        self._lock = threading.RLock()
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None
        self._async_task: asyncio.Task | None = None

        self._controller = None
        self._mapping = None
        self._link = None

        self._frame_history = deque(maxlen=frame_history)
        self._fault_history = deque(maxlen=event_history)
        self._event_history = deque(maxlen=event_history)
        self._telemetry_history = deque(maxlen=telemetry_history)

        self._session_state = "idle"
        self._port: str | None = None
        self._baudrate = 115200
        self._joystick_index = 0
        self._joystick_name = ""
        self._controller_connected = False
        self._controller_state = None
        self._controller_layout = []
        self._last_host_command = MotorCommand()
        self._latest_mcu_status: MCUStatus | None = None
        self._counters: dict[str, int] = {}
        self._health: dict[str, Any] = {}
        self._started_at: float | None = None
        self._stopped_at: float | None = None
        self._updated_at = 0.0
        self._last_error: str | None = None
        self._next_ui_sample_at = 0.0
        self._reset_runtime_state()
        self._controller_layout = self._describe_controller_layout()

    def set_event_callback(self, callback: Callable[[dict[str, Any]], None] | None):
        """Register a callback that receives JSON-ready websocket events."""
        self._event_callback = callback

    def open(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Open the controller and UART link on the current thread."""
        normalized_port = None if port in (None, "", "demo") else port
        with self._lock:
            if self._thread and self._thread.is_alive():
                raise RuntimeError("Drive runtime is already running")
            if self._async_task and not self._async_task.done():
                raise RuntimeError("Drive runtime is already running")
            self._reset_runtime_state()
            self._session_state = "starting"
            self._port = normalized_port
            self._baudrate = baudrate
            self._joystick_index = joystick_index
            self._started_at = time.time()
            self._updated_at = self._started_at
            self._record_event_locked("session", "Session Starting", "Opening controller and UART link")

        controller = None
        link = None
        mapping = None

        try:
            controller = self._controller_factory(joystick_index=joystick_index, deadzone=self._deadzone)
            controller.connect()
            mapping = self._mapping_factory()
            link = self._link_factory(port=normalized_port, baudrate=baudrate)
            link.open()
        except Exception as exc:
            if link is not None:
                try:
                    link.close()
                except Exception:
                    pass
            if controller is not None:
                try:
                    controller.disconnect()
                except Exception:
                    pass
            self._set_error(str(exc))
            raise

        with self._lock:
            self._controller = controller
            self._mapping = mapping
            self._link = link
            self._controller_connected = True
            self._joystick_name = getattr(controller, "name", "")
            self._controller_state = deepcopy(getattr(controller, "state", None))
            self._controller_layout = self._describe_controller_layout(mapping)
            self._session_state = "running"
            self._next_ui_sample_at = time.time()
            self._sync_link_state_locked()
            self._record_event_locked("session", "Session Running", "Drive control loop is active")

    def start(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the drive-mode runtime in a background thread."""
        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()
        self._emit("snapshot", to_payload(self.get_snapshot()))

    async def start_async(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the drive-mode runtime as an asyncio task on the current thread."""
        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._async_task = asyncio.create_task(self._run_loop_async())
        self._emit("snapshot", to_payload(self.get_snapshot()))

    def stop(self):
        """Stop the runtime and return the final snapshot."""
        self._stop_event.set()
        thread = self._thread
        if thread and thread.is_alive() and thread is not threading.current_thread():
            thread.join(timeout=2.0)
        elif self._async_task is None and self._session_state in {"running", "starting", "error"}:
            self._cleanup_after_loop()
        return self.get_snapshot()

    async def stop_async(self):
        """Stop the runtime when it is running on an asyncio task."""
        self._stop_event.set()
        task = self._async_task
        if task is not None:
            await task
        elif self._thread is None and self._session_state in {"running", "starting", "error"}:
            self._cleanup_after_loop()
        return self.get_snapshot()

    def get_snapshot(self) -> SessionSnapshot:
        """Return a deep-copied snapshot for API consumers."""
        with self._lock:
            return SessionSnapshot(
                session_state=self._session_state,
                port=self._port,
                baudrate=self._baudrate,
                joystick_index=self._joystick_index,
                joystick_name=self._joystick_name,
                controller_connected=self._controller_connected,
                controller_state=deepcopy(self._controller_state),
                controller_layout=deepcopy(self._controller_layout),
                last_host_command=deepcopy(self._last_host_command),
                latest_mcu_status=deepcopy(self._latest_mcu_status),
                recent_faults=deepcopy(list(self._fault_history)),
                recent_frames=deepcopy(list(self._frame_history)),
                recent_events=deepcopy(list(self._event_history)),
                telemetry_samples=deepcopy(list(self._telemetry_history)),
                counters=dict(self._counters),
                health=deepcopy(self._health),
                updated_at=self._updated_at,
                started_at=self._started_at,
                stopped_at=self._stopped_at,
                last_error=self._last_error,
            )

    def step(self):
        """Run a single drive-control iteration on the current thread."""
        loop_started = time.time()
        controller = self._controller
        mapping = self._mapping
        link = self._link

        if controller is None or mapping is None or link is None:
            raise RuntimeError("Drive runtime is not open")

        controller.poll()
        controller_state = deepcopy(controller.state)
        button_events = controller.drain_events()
        command = deepcopy(mapping.process(controller_state, button_events))
        link.send(command)

        frames = link.pop_frame_records()
        statuses = link.pop_statuses()
        faults = link.pop_faults()

        with self._lock:
            self._controller_state = controller_state
            self._last_host_command = command
            self._updated_at = loop_started
            self._counters["loop_iterations"] += 1
            self._ingest_button_events_locked(button_events)
            self._ingest_frames_locked(frames)
            self._ingest_statuses_locked(statuses, loop_started)
            self._ingest_faults_locked(faults)
            self._sync_link_state_locked()

        self._emit("controller_state", to_payload(controller_state))
        self._emit("host_command", to_payload(command))
        for frame in frames:
            self._emit("tx_frame", to_payload(frame))
        status_frames = [frame for frame in frames if frame.frame_id == FRAME_STATUS]
        for idx, status in enumerate(statuses):
            payload = {"status": to_payload(status)}
            if idx < len(status_frames):
                payload["frame"] = to_payload(status_frames[idx])
            self._emit("mcu_status", payload)
        fault_frames = [frame for frame in frames if frame.frame_id == FRAME_FAULT]
        for idx, fault in enumerate(faults):
            payload = {"fault": to_payload(fault)}
            if idx < len(fault_frames):
                payload["frame"] = to_payload(fault_frames[idx])
            self._emit("fault", payload)
        self._emit("health", to_payload(self._health))

    def _run_loop(self):
        try:
            while not self._stop_event.is_set():
                loop_started = time.time()
                self.step()

                elapsed = time.time() - loop_started
                if elapsed < self._poll_interval:
                    time.sleep(self._poll_interval - elapsed)
        except Exception as exc:
            self._set_error(str(exc))
        finally:
            self._cleanup_after_loop()

    async def _run_loop_async(self):
        try:
            while not self._stop_event.is_set():
                loop_started = time.time()
                self.step()
                elapsed = time.time() - loop_started
                if elapsed < self._poll_interval:
                    await asyncio.sleep(self._poll_interval - elapsed)
        except Exception as exc:
            self._set_error(str(exc))
        finally:
            self._cleanup_after_loop()

    def _ingest_button_events_locked(self, button_events):
        for event in button_events:
            edge = "Pressed" if event.edge == ButtonEdge.PRESSED else "Released"
            self._record_event_locked(
                kind="button",
                title=f"Button {edge}",
                message=f"{edge.lower()} {event.button}",
                data={"button": event.button, "edge": event.edge.name},
            )

    def _ingest_frames_locked(self, frames):
        for frame in frames:
            self._frame_history.append(frame)

    def _describe_controller_layout(self, mapping=None):
        descriptor_source = mapping
        if descriptor_source is None:
            try:
                descriptor_source = self._mapping_factory()
            except Exception:
                return []

        describe = getattr(descriptor_source, "describe_controller_layout", None)
        if callable(describe):
            return describe()
        return []

    def _ingest_statuses_locked(self, statuses: list[MCUStatus], timestamp: float):
        for status in statuses:
            self._latest_mcu_status = status
            if timestamp >= self._next_ui_sample_at:
                self._telemetry_history.append(
                    TelemetrySample(
                        timestamp=timestamp,
                        speed_ref=status.speed_ref,
                        id_fbk=status.id_fbk,
                        iq_fbk=status.iq_fbk,
                        vdc_bus=status.vdc_bus,
                        current_as=status.current_as,
                        current_bs=status.current_bs,
                        current_cs=status.current_cs,
                    )
                )
                self._next_ui_sample_at = timestamp + self._ui_sample_interval

    def _ingest_faults_locked(self, faults: list[MCUFault]):
        for fault in faults:
            self._fault_history.append(fault)
            self._record_event_locked(
                kind="fault",
                title="MCU Fault",
                message=str(fault),
                data=to_payload(fault),
            )

    def _sync_link_state_locked(self):
        link = self._link
        if link is None:
            self._health = {
                "controller_connected": self._controller_connected,
                "port_open": False,
                "terminal_only": self._port is None,
                "has_mcu_telemetry": self._latest_mcu_status is not None,
                "telemetry_stale": False,
                "last_error": self._last_error,
                "last_frame_at": None,
                "last_status_at": None,
            }
            return

        counters = link.get_counters()
        link_health = link.get_health()
        self._counters.update(
            {
                "tx_frames": counters.tx_frames,
                "rx_frames": counters.rx_frames,
                "status_frames": counters.status_frames,
                "fault_frames": counters.fault_frames,
                "checksum_errors": counters.checksum_errors,
                "serial_errors": counters.serial_errors,
            }
        )

        last_status_at = link_health.last_status_at
        telemetry_stale = False
        if (
            self._session_state == "running"
            and not link_health.terminal_only
            and last_status_at is not None
        ):
            telemetry_stale = (time.time() - last_status_at) > TELEMETRY_STALE_SECONDS

        if self._latest_mcu_status is None and self._session_state == "running" and not link_health.terminal_only:
            telemetry_stale = False

        combined_error = self._last_error or link_health.last_error
        self._health = {
            "controller_connected": self._controller_connected,
            "port_open": link_health.port_open,
            "terminal_only": link_health.terminal_only,
            "has_mcu_telemetry": self._latest_mcu_status is not None,
            "telemetry_stale": telemetry_stale,
            "last_error": combined_error,
            "last_frame_at": link_health.last_frame_at,
            "last_status_at": last_status_at,
        }

    def _reset_runtime_state(self):
        with self._lock:
            self._frame_history.clear()
            self._fault_history.clear()
            self._event_history.clear()
            self._telemetry_history.clear()
            self._session_state = "idle"
            self._port = None
            self._baudrate = 115200
            self._joystick_index = 0
            self._joystick_name = ""
            self._controller_connected = False
            self._controller_state = None
            self._last_host_command = MotorCommand()
            self._latest_mcu_status = None
            self._counters = {
                "loop_iterations": 0,
                "tx_frames": 0,
                "rx_frames": 0,
                "status_frames": 0,
                "fault_frames": 0,
                "checksum_errors": 0,
                "serial_errors": 0,
            }
            self._health = {
                "controller_connected": False,
                "port_open": False,
                "terminal_only": True,
                "has_mcu_telemetry": False,
                "telemetry_stale": False,
                "last_error": None,
                "last_frame_at": None,
                "last_status_at": None,
            }
            self._started_at = None
            self._stopped_at = None
            self._updated_at = time.time()
            self._last_error = None
            self._next_ui_sample_at = 0.0

    def _set_error(self, message: str):
        with self._lock:
            self._last_error = message
            self._session_state = "error"
            self._updated_at = time.time()
            self._record_event_locked("error", "Runtime Error", message)
            self._sync_link_state_locked()
        self._emit("health", to_payload(self._health))
        self._emit("snapshot", to_payload(self.get_snapshot()))

    def _cleanup_after_loop(self):
        controller = self._controller
        link = self._link
        if link is not None:
            try:
                link.close()
            except Exception:
                pass
        if controller is not None:
            try:
                controller.disconnect()
            except Exception:
                pass

        with self._lock:
            self._controller = None
            self._mapping = None
            self._link = None
            self._controller_connected = False
            if self._session_state != "error":
                self._session_state = "stopped"
                self._record_event_locked("session", "Session Stopped", "Drive control loop stopped")
            self._stopped_at = time.time()
            self._updated_at = self._stopped_at
            self._sync_link_state_locked()
            snapshot = to_payload(self.get_snapshot())
        self._thread = None
        self._async_task = None
        self._emit("health", to_payload(self._health))
        self._emit("snapshot", snapshot)

    def _record_event_locked(self, kind: str, title: str, message: str, data: dict[str, Any] | None = None):
        event = EventRecord(
            kind=kind,
            title=title,
            message=message,
            timestamp=time.time(),
            data=data or {},
        )
        self._event_history.append(event)
        self._emit("event", to_payload(event))

    def _emit(self, event_type: str, payload: Any):
        callback = self._event_callback
        if callback is None:
            return
        callback({"type": event_type, "payload": payload})
