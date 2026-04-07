#!/usr/bin/env python3
"""Mode-aware dashboard session manager for drive and music runtimes."""

from __future__ import annotations

from copy import deepcopy
from typing import Any, Callable

from .app_logging import NullStructuredLogger
from .music_runtime import MusicRuntime
from .runtime import DriveRuntime
from .runtime_models import SessionSnapshot


class DashboardRuntimeManager:
    """Owns the currently active dashboard runtime and switches by mode."""

    def __init__(
        self,
        drive_runtime_factory: Callable[[], Any] | None = None,
        music_runtime_factory: Callable[[], Any] | None = None,
    ):
        self._drive_runtime_factory = drive_runtime_factory or DriveRuntime
        self._music_runtime_factory = music_runtime_factory or MusicRuntime
        self._runtime = None
        self._event_callback = None
        self._logger = NullStructuredLogger()
        self._last_snapshot = SessionSnapshot()

    def set_event_callback(self, callback):
        """Register the websocket event sink used by the active runtime."""

        self._event_callback = callback
        runtime = self._runtime
        if runtime is not None and hasattr(runtime, "set_event_callback"):
            runtime.set_event_callback(callback)

    def set_logger(self, logger):
        """Replace the structured logger used by child runtimes."""

        self._logger = logger or NullStructuredLogger()
        runtime = self._runtime
        if runtime is not None and hasattr(runtime, "set_logger"):
            runtime.set_logger(self._logger)

    async def start_async(
        self,
        port: str | None = None,
        baudrate: int = 115200,
        joystick_index: int = 0,
        mode: str = "drive",
    ) -> SessionSnapshot:
        """Start either the drive runtime or the music runtime."""

        current = self._runtime
        if current is not None:
            snapshot = current.get_snapshot()
            if snapshot.session_state in {"starting", "running", "error"}:
                raise RuntimeError(f"{snapshot.mode.capitalize()} runtime is already running")

        runtime = self._music_runtime_factory() if mode == "music" else self._drive_runtime_factory()
        if hasattr(runtime, "set_event_callback"):
            runtime.set_event_callback(self._event_callback)
        if hasattr(runtime, "set_logger"):
            runtime.set_logger(self._logger)

        self._runtime = runtime
        await runtime.start_async(port=port, baudrate=baudrate, joystick_index=joystick_index)
        self._last_snapshot = runtime.get_snapshot()
        return deepcopy(self._last_snapshot)

    async def stop_async(self) -> SessionSnapshot:
        """Stop whichever runtime is currently active."""

        runtime = self._runtime
        if runtime is None:
            return deepcopy(self._last_snapshot)
        self._last_snapshot = await runtime.stop_async()
        return deepcopy(self._last_snapshot)

    def request_shutdown(self):
        """Forward process shutdown requests to the active runtime."""

        runtime = self._runtime
        if runtime is not None and hasattr(runtime, "request_shutdown"):
            runtime.request_shutdown()

    def get_snapshot(self) -> SessionSnapshot:
        """Return the latest snapshot from the active runtime."""

        runtime = self._runtime
        if runtime is not None:
            self._last_snapshot = runtime.get_snapshot()
        return deepcopy(self._last_snapshot)

    def engage_brake_override(self) -> SessionSnapshot:
        """Engage the drive-only brake override."""

        runtime = self._require_runtime(mode="drive", action="Brake override")
        self._last_snapshot = runtime.engage_brake_override()
        return deepcopy(self._last_snapshot)

    def release_brake_override(self) -> SessionSnapshot:
        """Release the drive-only brake override."""

        runtime = self._require_runtime(mode="drive", action="Brake release")
        self._last_snapshot = runtime.release_brake_override()
        return deepcopy(self._last_snapshot)

    def play_music(self, song_id: int, amplitude: float | None = None) -> SessionSnapshot:
        """Start a predefined song on the active music runtime."""

        runtime = self._require_runtime(mode="music", action="Music playback")
        self._last_snapshot = runtime.play_song(song_id, amplitude=amplitude)
        return deepcopy(self._last_snapshot)

    def pause_music(self) -> SessionSnapshot:
        """Pause the active music runtime."""

        runtime = self._require_runtime(mode="music", action="Music pause")
        self._last_snapshot = runtime.pause()
        return deepcopy(self._last_snapshot)

    def resume_music(self) -> SessionSnapshot:
        """Resume the active music runtime."""

        runtime = self._require_runtime(mode="music", action="Music resume")
        self._last_snapshot = runtime.resume()
        return deepcopy(self._last_snapshot)

    def stop_music(self) -> SessionSnapshot:
        """Stop playback on the active music runtime."""

        runtime = self._require_runtime(mode="music", action="Music stop")
        self._last_snapshot = runtime.stop_playback()
        return deepcopy(self._last_snapshot)

    def set_music_volume(self, volume: float) -> SessionSnapshot:
        """Set the global music-mode volume."""

        runtime = self._require_runtime(mode="music", action="Music volume")
        self._last_snapshot = runtime.set_volume(volume)
        return deepcopy(self._last_snapshot)

    def _require_runtime(self, mode: str, action: str):
        runtime = self._runtime
        if runtime is None:
            raise RuntimeError(f"{action} requires an active {mode} session")

        snapshot = runtime.get_snapshot()
        if snapshot.mode != mode or snapshot.session_state not in {"starting", "running", "error"}:
            if mode == "drive":
                raise RuntimeError("Brake override is only available in drive mode")
            raise RuntimeError(f"{action} requires an active music session")

        return runtime
