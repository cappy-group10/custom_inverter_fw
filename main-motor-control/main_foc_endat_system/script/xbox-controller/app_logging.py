#!/usr/bin/env python3
"""Structured file logging helpers for the dashboard backend and frontend."""

from __future__ import annotations

import json
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def _json_default(value: Any):
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, set):
        return sorted(value)
    if hasattr(value, "name"):
        return getattr(value, "name")
    return str(value)


class StructuredLogger:
    """Append JSONL entries to a timestamped file."""

    def __init__(self, path: Path):
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._file = self.path.open("a", encoding="utf-8", buffering=1)

    def log(
        self,
        level: str,
        source: str,
        message: str,
        route: str | None = None,
        metadata: dict[str, Any] | None = None,
    ):
        timestamp = time.time()
        entry = {
            "timestamp": round(timestamp, 6),
            "iso_timestamp": datetime.fromtimestamp(timestamp, tz=timezone.utc).isoformat(),
            "level": level.lower(),
            "source": source,
            "route": route or "",
            "message": message,
            "metadata": metadata or {},
        }
        line = json.dumps(entry, default=_json_default, sort_keys=True)
        with self._lock:
            self._file.write(f"{line}\n")

    def close(self):
        with self._lock:
            if not self._file.closed:
                self._file.close()


class NullStructuredLogger:
    """No-op logger used when file logging is not configured."""

    path: Path | None = None

    def log(
        self,
        level: str,
        source: str,
        message: str,
        route: str | None = None,
        metadata: dict[str, Any] | None = None,
    ):
        return None

    def close(self):
        return None


def create_timestamped_loggers(log_dir: Path, timestamp: str | None = None) -> tuple[StructuredLogger, StructuredLogger]:
    """Create the paired backend/frontend structured log files for this app run."""

    run_stamp = timestamp or datetime.now().strftime("%Y%m%d-%H%M%S")
    backend = StructuredLogger(log_dir / f"backend-{run_stamp}.log")
    frontend = StructuredLogger(log_dir / f"frontend-{run_stamp}.log")
    return backend, frontend
