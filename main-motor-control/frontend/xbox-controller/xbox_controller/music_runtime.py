#!/usr/bin/env python3
"""Music-mode runtime used by the dashboard for musical-motor control."""

from __future__ import annotations

import asyncio
import threading
import time
from collections import deque
from copy import deepcopy
from typing import Any, Callable

from .app_logging import NullStructuredLogger
from .motor_config import load_motor_config
from .music_uart import (
    CTRL_ACTION_PAUSE,
    CTRL_ACTION_RESUME,
    CTRL_ACTION_STOP,
    MusicUARTLink,
    control_action_name,
)
from .runtime_models import (
    EventRecord,
    FrameRecord,
    MusicCommandRecord,
    MusicSongOption,
    MusicState,
    MusicStatusRecord,
    SessionSnapshot,
    to_payload,
)


POLL_RATE_HZ = 60
UI_SAMPLE_HZ = 10
TELEMETRY_STALE_SECONDS = 1.0
TRANSPORT_LOG_INTERVAL_SECONDS = 2.0
DEFAULT_MUSIC_VOLUME = 0.2

MUSIC_SONG_CATALOG = [
    MusicSongOption(song_id=0, label="Mario"),
    MusicSongOption(song_id=1, label="Megalovania"),
    MusicSongOption(song_id=2, label="Jingle Bells"),
]


class MusicRuntime:
    """Runs the music-mode UART/status loop in one background thread."""

    def __init__(
        self,
        poll_rate_hz: int = POLL_RATE_HZ,
        ui_sample_hz: int = UI_SAMPLE_HZ,
        frame_history: int = 1000,
        event_history: int = 50,
        link_factory: Callable[..., Any] | None = None,
        event_callback: Callable[[dict[str, Any]], None] | None = None,
        logger=None,
    ):
        self._poll_interval = 1.0 / poll_rate_hz
        self._ui_sample_interval = 1.0 / ui_sample_hz
        self._link_factory = link_factory or MusicUARTLink
        self._event_callback = event_callback
        self._logger = logger or NullStructuredLogger()

        self._lock = threading.RLock()
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None
        self._async_task: asyncio.Task | None = None
        self._link = None

        self._frame_history = deque(maxlen=frame_history)
        self._event_history = deque(maxlen=event_history)
        self._motor_config = load_motor_config()

        self._session_state = "idle"
        self._port: str | None = None
        self._baudrate = 115200
        self._counters: dict[str, int] = {}
        self._health: dict[str, Any] = {}
        self._started_at: float | None = None
        self._stopped_at: float | None = None
        self._updated_at = 0.0
        self._last_error: str | None = None
        self._next_ui_tick_at = 0.0

        self._songs = deepcopy(MUSIC_SONG_CATALOG)
        self._selected_song_id: int | None = self._songs[0].song_id if self._songs else None
        self._last_started_song_id: int | None = None
        self._volume = DEFAULT_MUSIC_VOLUME
        self._last_command: MusicCommandRecord | None = None
        self._latest_status: MusicStatusRecord | None = None
        self._play_state = "IDLE"
        self._play_mode = "SONG"
        self._note_index = 0
        self._note_total = 0
        self._current_freq_hz = 0.0
        self._amplitude = 0.0
        self._isr_ticker = 0

        self._pending_frames: list[FrameRecord] = []
        self._pending_events: list[EventRecord] = []
        self._last_transport_log_at: float | None = None
        self._last_transport_log_counters = {
            "tx_frames": 0,
            "rx_frames": 0,
            "checksum_errors": 0,
            "serial_errors": 0,
        }
        self._reset_runtime_state()

    def set_event_callback(self, callback: Callable[[dict[str, Any]], None] | None):
        """Register a callback that receives JSON-ready websocket events."""

        self._event_callback = callback

    def set_logger(self, logger):
        """Replace the structured logger used by the runtime."""

        self._logger = logger or NullStructuredLogger()

    def open(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Open the music UART link on the current thread."""

        del joystick_index
        normalized_port = None if port in (None, "", "demo") else port
        with self._lock:
            if self._thread and self._thread.is_alive():
                raise RuntimeError("Music runtime is already running")
            if self._async_task and not self._async_task.done():
                raise RuntimeError("Music runtime is already running")
            self._reset_runtime_state()
            self._session_state = "starting"
            self._port = normalized_port
            self._baudrate = baudrate
            self._started_at = time.time()
            self._updated_at = self._started_at
            self._record_event_locked("session", "Session Starting", "Opening musical-motor UART link")

        link = None
        try:
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
            self._set_error(str(exc))
            raise

        with self._lock:
            self._link = link
            self._session_state = "running"
            self._next_ui_tick_at = time.time() + self._ui_sample_interval
            self._last_transport_log_at = time.time()
            self._sync_link_state_locked()
            self._record_event_locked("session", "Session Running", "Music control loop is active")
        self._logger.log(
            "info",
            "music_runtime",
            "Music runtime opened",
            route="/api/session/start",
            metadata={
                "mode": "music",
                "port": normalized_port or "demo",
                "baudrate": baudrate,
            },
        )

    def start(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the music runtime in a background thread."""

        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()
        self._emit_snapshot(reset_ui_tick_window=True)

    async def start_async(self, port: str | None = None, baudrate: int = 115200, joystick_index: int = 0):
        """Start the music runtime as an asyncio task on the current thread."""

        self.open(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._stop_event.clear()
        self._async_task = asyncio.create_task(self._run_loop_async())
        self._emit_snapshot(reset_ui_tick_window=True)

    def request_shutdown(self):
        """Synchronously ask the runtime loop to stop without waiting for cleanup."""

        self._logger.log("info", "music_runtime", "Runtime shutdown requested", route="signal")
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
                mode="music",
                port=self._port,
                baudrate=self._baudrate,
                joystick_index=0,
                joystick_name="",
                mapping_name="MusicDashboard",
                controller_connected=False,
                controller_state=None,
                controller_layout=[],
                motor_config=deepcopy(self._motor_config),
                last_host_command=None,
                latest_mcu_status=None,
                active_override=None,
                recent_faults=[],
                recent_frames=deepcopy(list(self._frame_history)),
                recent_events=deepcopy(list(self._event_history)),
                telemetry_samples=[],
                music_state=deepcopy(self._build_music_state_locked()),
                counters=dict(self._counters),
                health=deepcopy(self._health),
                updated_at=self._updated_at,
                started_at=self._started_at,
                stopped_at=self._stopped_at,
                last_error=self._last_error,
            )

    def step(self):
        """Run a single music-status iteration on the current thread."""

        loop_started = time.time()
        link = self._link
        if link is None:
            raise RuntimeError("Music runtime is not open")

        frames = link.pop_frame_records()
        statuses = link.pop_statuses()
        ui_tick_payload = None
        transport_summary = None

        with self._lock:
            self._updated_at = loop_started
            self._counters["loop_iterations"] += 1
            self._ingest_frames_locked(frames)
            self._ingest_statuses_locked(statuses)
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
                "music_runtime",
                "Transport summary",
                route="/music_runtime/summary",
                metadata=transport_summary,
            )

        if ui_tick_payload is not None:
            self._emit("ui_tick", ui_tick_payload)

    def play_song(self, song_id: int, amplitude: float | None = None) -> SessionSnapshot:
        """Start a predefined song with the current or supplied amplitude."""

        with self._lock:
            self._ensure_active_locked()
            song_id = int(song_id)
            amplitude = float(self._volume if amplitude is None else amplitude)
            self._selected_song_id = song_id
            self._last_started_song_id = song_id
            self._volume = amplitude
            self._play_state = "PLAYING"
            self._play_mode = "SONG"
            self._amplitude = amplitude
            self._note_index = 0
            self._note_total = 0
            self._link.send_song(song_id, amplitude)
            self._last_command = MusicCommandRecord(
                command_type="song",
                song_id=song_id,
                song_label=self._song_label(song_id),
                amplitude=amplitude,
                timestamp=time.time(),
            )
            self._updated_at = time.time()
            self._record_event_locked(
                "music",
                "Song Start Requested",
                f"Starting {self._song_label(song_id)} at amplitude {amplitude:.2f}.",
                data={"song_id": song_id, "song_label": self._song_label(song_id), "amplitude": amplitude},
            )
            self._ingest_frames_locked(self._link.pop_frame_records())
            self._ingest_statuses_locked(self._link.pop_statuses())
            self._sync_link_state_locked()
        self._emit_snapshot(reset_ui_tick_window=True)
        return self.get_snapshot()

    def pause(self) -> SessionSnapshot:
        """Pause the current music session."""

        return self._send_control_command(
            CTRL_ACTION_PAUSE,
            next_play_state="PAUSED",
            title="Pause Requested",
            message="Pausing musical-motor playback.",
            route="/api/mcus/primary/music/pause",
        )

    def resume(self) -> SessionSnapshot:
        """Resume a paused music session."""

        return self._send_control_command(
            CTRL_ACTION_RESUME,
            next_play_state="PLAYING",
            title="Resume Requested",
            message="Resuming musical-motor playback.",
            route="/api/mcus/primary/music/resume",
        )

    def stop_playback(self) -> SessionSnapshot:
        """Stop the current music session."""

        with self._lock:
            self._ensure_active_locked()
            self._play_state = "IDLE"
            self._current_freq_hz = 0.0
            self._note_index = 0
            self._note_total = 0
            self._amplitude = self._volume
        return self._send_control_command(
            CTRL_ACTION_STOP,
            next_play_state="IDLE",
            title="Stop Requested",
            message="Stopping musical-motor playback.",
            route="/api/mcus/primary/music/stop",
        )

    def set_volume(self, volume: float) -> SessionSnapshot:
        """Change the global music volume."""

        with self._lock:
            self._ensure_active_locked()
            volume = float(volume)
            self._volume = volume
            self._amplitude = volume
            self._link.send_volume(volume)
            self._last_command = MusicCommandRecord(
                command_type="volume",
                volume=volume,
                timestamp=time.time(),
            )
            self._updated_at = time.time()
            self._record_event_locked(
                "music",
                "Volume Updated",
                f"Setting musical-motor volume to {volume:.2f}.",
                data={"volume": volume},
            )
            self._ingest_frames_locked(self._link.pop_frame_records())
            self._ingest_statuses_locked(self._link.pop_statuses())
            self._sync_link_state_locked()
        self._emit_snapshot(reset_ui_tick_window=True)
        return self.get_snapshot()

    def _send_control_command(
        self,
        action: int,
        next_play_state: str,
        title: str,
        message: str,
        route: str,
    ) -> SessionSnapshot:
        with self._lock:
            self._ensure_active_locked()
            self._play_state = next_play_state
            if next_play_state == "IDLE":
                self._current_freq_hz = 0.0
                self._note_index = 0
                self._note_total = 0
            self._link.send_control(action)
            self._last_command = MusicCommandRecord(
                command_type="control",
                action=control_action_name(action),
                timestamp=time.time(),
            )
            self._updated_at = time.time()
            self._record_event_locked(
                "music",
                title,
                message,
                data={"action": control_action_name(action)},
            )
            self._ingest_frames_locked(self._link.pop_frame_records())
            self._ingest_statuses_locked(self._link.pop_statuses())
            self._sync_link_state_locked()
        self._logger.log(
            "info",
            "music_runtime",
            title,
            route=route,
            metadata={"action": control_action_name(action)},
        )
        self._emit_snapshot(reset_ui_tick_window=True)
        return self.get_snapshot()

    def _ensure_active_locked(self):
        if self._session_state not in {"starting", "running", "error"} or self._link is None:
            raise RuntimeError("Music runtime is not active")

    def _song_label(self, song_id: int) -> str:
        for song in self._songs:
            if song.song_id == song_id:
                return song.label
        return f"Song {song_id}"

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

    def _ingest_frames_locked(self, frames: list[FrameRecord]):
        for frame in frames:
            self._frame_history.append(frame)
            self._pending_frames.append(frame)

    def _ingest_statuses_locked(self, statuses: list[MusicStatusRecord]):
        for status in statuses:
            self._latest_status = status
            self._play_state = status.play_state
            self._play_mode = status.play_mode
            self._note_index = status.note_index
            self._note_total = status.note_total
            self._current_freq_hz = status.current_freq_hz
            self._amplitude = status.amplitude
            self._isr_ticker = status.isr_ticker

    def _sync_link_state_locked(self):
        link = self._link
        if link is None:
            self._health = {
                "controller_connected": False,
                "port_open": False,
                "terminal_only": self._port is None,
                "has_mcu_telemetry": self._latest_status is not None,
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
                "fault_frames": 0,
                "checksum_errors": counters.checksum_errors,
                "serial_errors": counters.serial_errors,
            }
        )

        last_status_at = link_health.last_status_at
        telemetry_stale = False
        if self._session_state == "running" and not link_health.terminal_only and last_status_at is not None:
            telemetry_stale = (time.time() - last_status_at) > TELEMETRY_STALE_SECONDS

        if self._latest_status is None and self._session_state == "running" and not link_health.terminal_only:
            telemetry_stale = False

        combined_error = self._last_error or link_health.last_error
        self._health = {
            "controller_connected": False,
            "port_open": link_health.port_open,
            "terminal_only": link_health.terminal_only,
            "has_mcu_telemetry": self._latest_status is not None,
            "telemetry_stale": telemetry_stale,
            "last_error": combined_error,
            "last_frame_at": link_health.last_frame_at,
            "last_status_at": last_status_at,
        }

    def _reset_runtime_state(self):
        with self._lock:
            self._refresh_motor_config_locked()
            self._frame_history.clear()
            self._event_history.clear()
            self._session_state = "idle"
            self._port = None
            self._baudrate = 115200
            self._selected_song_id = self._songs[0].song_id if self._songs else None
            self._last_started_song_id = None
            self._volume = DEFAULT_MUSIC_VOLUME
            self._last_command = None
            self._latest_status = None
            self._play_state = "IDLE"
            self._play_mode = "SONG"
            self._note_index = 0
            self._note_total = 0
            self._current_freq_hz = 0.0
            self._amplitude = 0.0
            self._isr_ticker = 0
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
            self._next_ui_tick_at = 0.0
            self._pending_frames = []
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
        self._logger.log("error", "music_runtime", "Music runtime error", route="/music_runtime", metadata={"error": message})
        self._emit_snapshot(reset_ui_tick_window=True)

    def _cleanup_after_loop(self):
        link = self._link
        if link is not None:
            try:
                link.close()
            except Exception:
                pass

        with self._lock:
            self._link = None
            if self._session_state != "error":
                self._session_state = "stopped"
                self._record_event_locked("session", "Session Stopped", "Music control loop stopped")
            self._stopped_at = time.time()
            self._updated_at = self._stopped_at
            self._sync_link_state_locked()
        self._logger.log(
            "info",
            "music_runtime",
            "Music runtime stopped",
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
        self._pending_events.clear()

    def _build_music_state_locked(self) -> MusicState:
        latest_status = deepcopy(self._latest_status)
        return MusicState(
            songs=deepcopy(self._songs),
            selected_song_id=self._selected_song_id,
            last_started_song_id=self._last_started_song_id,
            volume=self._volume,
            last_command=deepcopy(self._last_command),
            latest_status=latest_status,
            play_state=self._play_state,
            play_mode=self._play_mode,
            note_index=self._note_index,
            note_total=self._note_total,
            current_freq_hz=self._current_freq_hz,
            amplitude=self._amplitude,
            isr_ticker=self._isr_ticker,
        )

    def _build_ui_tick_payload_locked(self) -> dict[str, Any]:
        self._refresh_motor_config_locked()
        return {
            "mode": "music",
            "music_state": deepcopy(self._build_music_state_locked()),
            "health": deepcopy(self._health),
            "counters": dict(self._counters),
            "new_frames": deepcopy(self._pending_frames),
            "new_faults": [],
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
        summary = {
            "session_state": self._session_state,
            "mode": "music",
            "port": self._port or "demo",
            "terminal_only": bool(self._health.get("terminal_only", False)),
            "tx_frames_delta": current["tx_frames"] - previous["tx_frames"],
            "rx_frames_delta": current["rx_frames"] - previous["rx_frames"],
            "checksum_errors_delta": current["checksum_errors"] - previous["checksum_errors"],
            "serial_errors_delta": current["serial_errors"] - previous["serial_errors"],
            "play_state": self._play_state,
            "current_freq_hz": self._current_freq_hz,
        }
        self._last_transport_log_at = timestamp
        self._last_transport_log_counters = current
        return summary
