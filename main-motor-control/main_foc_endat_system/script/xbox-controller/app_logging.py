#!/usr/bin/env python3
"""Readable file logging helpers backed by Python's logging module."""

from __future__ import annotations

import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Any


def _stringify_metadata(value: Any) -> str:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, set):
        return str(sorted(value))
    if hasattr(value, "name"):
        return str(getattr(value, "name"))
    return str(value)


def _format_metadata(metadata: dict[str, Any] | None) -> str:
    if not metadata:
        return "-"
    return json.dumps(metadata, default=_stringify_metadata, sort_keys=True)


class ReadableLogFormatter(logging.Formatter):
    """Format log records as readable single-line text."""

    def format(self, record: logging.LogRecord) -> str:
        timestamp = self.formatTime(record, self.datefmt)
        source = getattr(record, "source", "app")
        route = getattr(record, "route", "-") or "-"
        metadata_text = getattr(record, "metadata_text", "-")
        message = record.getMessage()
        return f"{timestamp} | {record.levelname:<7} | {source:<14} | {route:<28} | {message} | {metadata_text}"


class StructuredLogger:
    """Small adapter that writes readable log files through logging.FileHandler."""

    def __init__(self, path: Path):
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._logger = logging.getLogger(f"inverter_os.{self.path.stem}.{id(self)}")
        self._logger.setLevel(logging.DEBUG)
        self._logger.propagate = False

        self._handler = logging.FileHandler(self.path, encoding="utf-8")
        self._handler.setLevel(logging.DEBUG)
        self._handler.setFormatter(ReadableLogFormatter(datefmt="%Y-%m-%d %H:%M:%S"))
        self._logger.handlers.clear()
        self._logger.addHandler(self._handler)

    def log(
        self,
        level: str,
        source: str,
        message: str,
        route: str | None = None,
        metadata: dict[str, Any] | None = None,
    ):
        numeric_level = getattr(logging, str(level).upper(), logging.INFO)
        self._logger.log(
            numeric_level,
            message,
            extra={
                "source": source,
                "route": route or "-",
                "metadata_text": _format_metadata(metadata),
            },
        )

    def close(self):
        self._handler.flush()
        self._handler.close()
        self._logger.removeHandler(self._handler)


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
    """Create the paired backend/frontend log files for this app run."""

    run_stamp = timestamp or datetime.now().strftime("%Y%m%d-%H%M%S")
    backend = StructuredLogger(log_dir / f"backend-{run_stamp}.log")
    frontend = StructuredLogger(log_dir / f"frontend-{run_stamp}.log")
    return backend, frontend
