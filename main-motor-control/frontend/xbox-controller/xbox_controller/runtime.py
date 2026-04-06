#!/usr/bin/env python3
"""Shared drive-mode runtime used by the CLI and browser dashboard."""

from __future__ import annotations

import asyncio
import threading
import time
from collections import deque
from copy import deepcopy
from typing import Any, Callable

from .app_logging import NullStructuredLogger
from .commands import CtrlState, MotorCommand
from .controller import ButtonEdge, XboxController
from .mapping import DriveMapping
from .motor_config import load_motor_config
from .runtime_models import EventRecord, SessionSnapshot, TelemetrySample, to_payload
from .uart import FRAME_FAULT, FRAME_STATUS, MCUFault, MCUStatus, UARTLink


POLL_RATE_HZ = 60
UI_SAMPLE_HZ = 10
TELEMETRY_STALE_SECONDS = 1.0
KEEPALIVE_INTERVAL_SECONDS = 0.25
SPEED_TX_THRESHOLD_PU = 0.01
TRANSPORT_LOG_INTERVAL_SECONDS = 2.0


class DriveRuntime:
    """Runs the drive-mode controller + UART loop in one background thread."""

    def __init__(
        self,
        poll_rate_hz: int = POLL_RATE_HZ,
        ui_sample_hz: int = UI_SAMPLE_HZ,
        deadzone: float = 0.08,
        frame_history: int = 1000,
        event_history: int = 50,
        telemetry_history: int = 300,
        controller_factory: Callable[..., Any] | None = None,
        mapping_factory: Callable[[], Any] | None = None,
        link_factory: Callable[..., Any] | None = None,
        event_callback: Callable[[dict[str, Any]], None] | None = None,
        logger=None,
    ):
        self._poll_interval = 1.0 / poll_rate_hz
        self._ui_sample_interval = 1.0 / ui_sample_hz
        self._deadzone = deadzone
        self._controller_factory = controller_factory or XboxController
        self._mapping_factory = mapping_factory or DriveMapping
        self._link_factory = link_factory or UARTLink
        self._event_callback = event_callback
        self._logger = logger or NullStructuredLogger()

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
        self._motor_config = load_motor_config()
        self._last_host_command = MotorCommand()
        self._latest_mcu_status: MCUStatus | None = None
        self._active_override: str | None = None
        self._counters: dict[str, int] = {}
        self._health: dict[str, Any] = {}
        self._started_at: float | None = None
        self._stopped_at: float | None = None
        self._updated_at = 0.0
        self._last_error: str | None = None
        self._next_ui_sample_at = 0.0
        self._next_ui_tick_at = 0.0
        self._keepalive_interval = KEEPALIVE_INTERVAL_SECONDS
        self._last_tx_at: float | None = None
        self._last_transmitted_command: MotorCommand | None = None
        self._last_transmitted_left_stick_active = False
        self._pending_frames: list[FrameRecord] = []
        self._pending_faults: list[MCUFault] = []
        self._pending_events: list[EventRecord] = []
        self._last_transport_log_at: float | None = None
        self._last_transport_log_counters = {
            "tx_frames": 0,
            "rx_frames": 0,
            "checksum_errors": 0,
            "serial_errors": 0,
        }
        self._reset_runtime_state()
        self._controller_layout = self._describe_controller_layout()

    def set_event_callback(self, callback: Callable[[dict[str, Any]], None] | None):
        """Register a callback that receives JSON-ready websocket events."""
        self._event_callback = callback

    def set_logger(self, logger):
        """Replace the structured logger used by the runtime."""
        self._logger = logger or NullStructuredLogger()

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
            if hasattr(link, "set_logger"):
                link.set_logger(self._logger)
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
            self._next_ui_tick_at = self._next_ui_sample_at + self._ui_sample_interval
            self._last_transport_log_at = self._next_ui_sample_at
            self._sync_link_state_locked()
            self._record_event_locked("session", "Session Running", "Drive control loop is active")
        self._logger.log(
            "info",
            "runtime",
            "Drive runtime opened",
            route="/api/session/start",
            metadata={
                "port": normalized_port or "demo",
                "baudrate": baudrate,
                "joystick_index": joystick_index,
                "joystick_name": getattr(controller, "name", ""),
            },
        )

    def start(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the drive-mode runtime in a background thread."""
        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()
        self._emit_snapshot(reset_ui_tick_window=True)

    async def start_async(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the drive-mode runtime as an asyncio task on the current thread."""
        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._async_task = asyncio.create_task(self._run_loop_async())
        self._emit_snapshot(reset_ui_tick_window=True)

    def request_shutdown(self):
        """Synchronously ask the runtime loop to stop without waiting for cleanup.

        This is used from process signal handlers so the async drive loop can wind
        down before the ASGI server starts waiting for background tasks to finish.
        """
        self._logger.log("info", "runtime", "Runtime shutdown requested", route="signal")
        self._stop_event.set()
        self.set_event_callback(None)

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
            self._refresh_motor_config_locked()
            return SessionSnapshot(
                session_state=self._session_state,
                port=self._port,
                baudrate=self._baudrate,
                joystick_index=self._joystick_index,
                joystick_name=self._joystick_name,
                controller_connected=self._controller_connected,
                controller_state=deepcopy(self._controller_state),
                controller_layout=deepcopy(self._controller_layout),
                motor_config=deepcopy(self._motor_config),
                last_host_command=deepcopy(self._last_host_command),
                latest_mcu_status=deepcopy(self._latest_mcu_status),
                active_override=self._active_override,
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
        command = self._apply_override(command)
        transmit_reason = self._get_transmit_reason(controller_state, button_events, command, loop_started)
        if transmit_reason is not None:
            link.send(command)
            self._note_transmit_locked(controller_state, command, loop_started)

        frames = link.pop_frame_records()
        statuses = link.pop_statuses()
        faults = link.pop_faults()
        ui_tick_payload = None
        transport_summary = None

        with self._lock:
            self._controller_state = controller_state
            self._updated_at = loop_started
            self._counters["loop_iterations"] += 1
            self._ingest_button_events_locked(button_events)
            self._ingest_frames_locked(frames)
            self._ingest_statuses_locked(statuses, loop_started)
            self._ingest_faults_locked(faults)
            self._sync_link_state_locked()
            transport_summary = self._maybe_build_transport_summary_locked(loop_started)
            if loop_started >= self._next_ui_tick_at:
                if self._event_callback is not None:
                    ui_tick_payload = to_payload(self._build_ui_tick_payload_locked())
                self._clear_pending_stream_items_locked()
                self._next_ui_tick_at = loop_started + self._ui_sample_interval

        if transport_summary is not None:
            self._logger.log(
                "info",
                "runtime",
                "Transport summary",
                route="/runtime/summary",
                metadata=transport_summary,
            )

        if ui_tick_payload is not None:
            self._emit("ui_tick", ui_tick_payload)

    def _left_stick_active(self, controller_state) -> bool:
        return abs(float(getattr(controller_state, "left_y", 0.0) or 0.0)) > 0.0

    def _commands_require_transmit(self, command: MotorCommand) -> bool:
        previous = self._last_transmitted_command
        if previous is None:
            return True
        if command.ctrl_state != previous.ctrl_state:
            return True
        if abs(command.id_ref - previous.id_ref) > 1e-9:
            return True
        if abs(command.iq_ref - previous.iq_ref) > 1e-9:
            return True
        if abs(command.speed_ref - previous.speed_ref) >= SPEED_TX_THRESHOLD_PU:
            return True
        return False

    def _command_streaming_matters(self, command: MotorCommand) -> bool:
        if self._active_override is not None:
            return True
        if command.ctrl_state != CtrlState.STOP:
            return True
        if abs(command.speed_ref) >= SPEED_TX_THRESHOLD_PU:
            return True
        return False

    def _get_transmit_reason(self, controller_state, button_events, command: MotorCommand, now: float) -> str | None:
        if button_events:
            return "button_edge"

        left_stick_active = self._left_stick_active(controller_state)
        if self._commands_require_transmit(command):
            return "command_change"
        if left_stick_active != self._last_transmitted_left_stick_active:
            return "command_change"

        if (
            self._command_streaming_matters(command)
            and self._last_tx_at is not None
            and (now - self._last_tx_at) >= self._keepalive_interval
        ):
            return "keepalive"
        return None

    def _note_transmit_locked(self, controller_state, command: MotorCommand, now: float):
        self._last_tx_at = now
        self._last_host_command = deepcopy(command)
        self._last_transmitted_command = deepcopy(command)
        self._last_transmitted_left_stick_active = self._left_stick_active(controller_state)

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

    def engage_brake_override(self) -> SessionSnapshot:
        """Latch a BRAKE override until the session stops or restarts."""
        with self._lock:
            if self._session_state not in {"starting", "running", "error"} or self._link is None:
                raise RuntimeError("Drive runtime is not active")
            is_new_override = self._active_override != "BRAKE"
            self._active_override = "BRAKE"
            self._updated_at = time.time()
            if is_new_override:
                self._record_event_locked(
                    kind="override",
                    title="Emergency Brake Engaged",
                    message="UI brake override latched for this drive session.",
                    data={"override": "BRAKE"},
                )
            self._logger.log(
                "warning" if is_new_override else "info",
                "runtime",
                "Brake override updated",
                route="/api/mcus/primary/brake",
                metadata={"active_override": "BRAKE", "is_new_override": is_new_override},
            )
        self._emit_snapshot(reset_ui_tick_window=True)
        return self.get_snapshot()

    def release_brake_override(self) -> SessionSnapshot:
        """Clear the brake override, force STOP, and transmit it immediately."""

        with self._lock:
            if self._session_state not in {"starting", "running", "error"} or self._link is None:
                raise RuntimeError("Drive runtime is not active")
            if self._active_override != "BRAKE":
                raise RuntimeError("Brake override is not latched")

            stop_command = MotorCommand(
                ctrl_state=CtrlState.STOP,
                speed_ref=0.0,
                id_ref=0.0,
                iq_ref=0.0,
            )
            if self._mapping is not None:
                force_motor_command = getattr(self._mapping, "force_motor_command", None)
                if callable(force_motor_command):
                    force_motor_command(stop_command)

            self._active_override = None
            self._link.send(stop_command)
            self._note_transmit_locked(self._controller_state, stop_command, time.time())
            self._ingest_frames_locked(self._link.pop_frame_records())
            self._ingest_statuses_locked(self._link.pop_statuses(), time.time())
            self._ingest_faults_locked(self._link.pop_faults())
            self._sync_link_state_locked()
            self._updated_at = time.time()
            self._record_event_locked(
                kind="override",
                title="Emergency Brake Released",
                message="BRAKE override cleared and STOP command transmitted immediately.",
                data={"override": None, "ctrl_state": CtrlState.STOP.name},
            )
            self._logger.log(
                "info",
                "runtime",
                "Brake override released",
                route="/api/mcus/primary/brake/release",
                metadata={"active_override": None, "ctrl_state": CtrlState.STOP.name},
            )

        self._emit_snapshot(reset_ui_tick_window=True)
        return self.get_snapshot()

    def _apply_override(self, command: MotorCommand) -> MotorCommand:
        override = self._active_override
        if override != "BRAKE":
            return command
        command.ctrl_state = CtrlState.BRAKE
        command.speed_ref = 0.0
        command.id_ref = 0.0
        command.iq_ref = 0.0
        return command

    def _ingest_frames_locked(self, frames):
        for frame in frames:
            self._frame_history.append(frame)
            self._pending_frames.append(frame)

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
                latest_command = self._last_host_command or MotorCommand()
                self._telemetry_history.append(
                    TelemetrySample(
                        timestamp=timestamp,
                        speed_ref=status.speed_ref,
                        speed_fbk=status.speed_fbk,
                        id_ref=latest_command.id_ref,
                        id_fbk=status.id_fbk,
                        iq_ref=latest_command.iq_ref,
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
            self._pending_faults.append(fault)
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
            self._refresh_motor_config_locked()
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
            self._active_override = None
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
            self._next_ui_tick_at = 0.0
            self._last_tx_at = None
            self._last_transmitted_command = None
            self._last_transmitted_left_stick_active = False
            self._pending_frames = []
            self._pending_faults = []
            self._pending_events = []
            self._last_transport_log_at = None
            self._last_transport_log_counters = {
                "tx_frames": 0,
                "rx_frames": 0,
                "checksum_errors": 0,
                "serial_errors": 0,
            }

    def _set_error(self, message: str):
        with self._lock:
            self._last_error = message
            self._session_state = "error"
            self._updated_at = time.time()
            self._record_event_locked("error", "Runtime Error", message)
            self._sync_link_state_locked()
        self._logger.log("error", "runtime", "Drive runtime error", route="/runtime", metadata={"error": message})
        self._emit_snapshot(reset_ui_tick_window=True)

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
            self._active_override = None
            if self._session_state != "error":
                self._session_state = "stopped"
                self._record_event_locked("session", "Session Stopped", "Drive control loop stopped")
            self._stopped_at = time.time()
            self._updated_at = self._stopped_at
            self._sync_link_state_locked()
        self._logger.log(
            "info",
            "runtime",
            "Drive runtime stopped",
            route="/api/session/stop",
            metadata={"session_state": self.get_snapshot().session_state, "port": self.get_snapshot().port or "demo"},
        )
        self._thread = None
        self._async_task = None
        self._emit_snapshot(reset_ui_tick_window=True)

    def _record_event_locked(self, kind: str, title: str, message: str, data: dict[str, Any] | None = None):
        event = EventRecord(
            kind=kind,
            title=title,
            message=message,
            timestamp=time.time(),
            data=data or {},
        )
        self._event_history.append(event)
        self._pending_events.append(event)

    def _emit(self, event_type: str, payload: Any):
        callback = self._event_callback
        if callback is None:
            return
        callback({"type": event_type, "payload": payload})

    def _clear_pending_stream_items_locked(self):
        self._pending_frames.clear()
        self._pending_faults.clear()
        self._pending_events.clear()

    def _build_ui_tick_payload_locked(self) -> dict[str, Any]:
        self._refresh_motor_config_locked()
        return {
            "controller_state": deepcopy(self._controller_state),
            "motor_config": deepcopy(self._motor_config),
            "last_host_command": deepcopy(self._last_host_command),
            "latest_mcu_status": deepcopy(self._latest_mcu_status),
            "health": deepcopy(self._health),
            "counters": dict(self._counters),
            "new_frames": deepcopy(self._pending_frames),
            "new_faults": deepcopy(self._pending_faults),
            "new_events": deepcopy(self._pending_events),
        }

    def _refresh_motor_config_locked(self):
        self._motor_config = load_motor_config()

    def _emit_snapshot(self, reset_ui_tick_window: bool = False):
        snapshot_payload = to_payload(self.get_snapshot())
        with self._lock:
            self._clear_pending_stream_items_locked()
            if reset_ui_tick_window:
                self._next_ui_tick_at = time.time() + self._ui_sample_interval
        self._emit("snapshot", snapshot_payload)

    def _maybe_build_transport_summary_locked(self, timestamp: float) -> dict[str, Any] | None:
        if self._session_state != "running":
            return None

        if self._last_transport_log_at is None:
            self._last_transport_log_at = timestamp
            self._last_transport_log_counters = {
                "tx_frames": int(self._counters.get("tx_frames", 0)),
                "rx_frames": int(self._counters.get("rx_frames", 0)),
                "checksum_errors": int(self._counters.get("checksum_errors", 0)),
                "serial_errors": int(self._counters.get("serial_errors", 0)),
            }
            return None

        if (timestamp - self._last_transport_log_at) < TRANSPORT_LOG_INTERVAL_SECONDS:
            return None

        current = {
            "tx_frames": int(self._counters.get("tx_frames", 0)),
            "rx_frames": int(self._counters.get("rx_frames", 0)),
            "checksum_errors": int(self._counters.get("checksum_errors", 0)),
            "serial_errors": int(self._counters.get("serial_errors", 0)),
        }
        previous = self._last_transport_log_counters
        latest_status = self._latest_mcu_status
        latest_command = self._last_host_command

        summary = {
            "session_state": self._session_state,
            "port": self._port or "demo",
            "terminal_only": bool(self._health.get("terminal_only", False)),
            "tx_frames_delta": current["tx_frames"] - previous["tx_frames"],
            "rx_frames_delta": current["rx_frames"] - previous["rx_frames"],
            "checksum_errors_delta": current["checksum_errors"] - previous["checksum_errors"],
            "serial_errors_delta": current["serial_errors"] - previous["serial_errors"],
            "ctrl_state": getattr(latest_status, "ctrl_state", getattr(latest_command, "ctrl_state", None)),
            "speed_ref": getattr(latest_status, "speed_ref", getattr(latest_command, "speed_ref", None)),
        }

        self._last_transport_log_at = timestamp
        self._last_transport_log_counters = current
        return summary
