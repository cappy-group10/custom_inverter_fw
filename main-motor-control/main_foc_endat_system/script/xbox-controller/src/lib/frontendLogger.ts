import { api } from "./api";
import type { FrontendLogRecord } from "./types";

const FLUSH_INTERVAL_MS = 1200;
const MAX_BATCH_SIZE = 20;
const LOG_ENDPOINT = "/api/logs/frontend";
const isTestMode = import.meta.env.MODE === "test";

class FrontendLogger {
  private queue: FrontendLogRecord[] = [];
  private flushTimer: number | null = null;
  private inFlight = false;
  private readonly clientSessionId = `frontend-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
  private listenersBound = false;

  constructor() {
    this.bindLifecycleListeners();
  }

  log(level: FrontendLogRecord["level"], source: string, message: string, metadata: Record<string, unknown> = {}) {
    if (isTestMode) {
      return;
    }
    const record: FrontendLogRecord = {
      timestamp: Date.now() / 1000,
      level,
      source,
      route: `${window.location.pathname}${window.location.search}${window.location.hash}`,
      message,
      metadata,
      client_session_id: this.clientSessionId,
    };
    this.queue.push(record);
    if (this.queue.length >= MAX_BATCH_SIZE) {
      void this.flush();
      return;
    }
    if (this.flushTimer === null) {
      this.flushTimer = window.setTimeout(() => {
        this.flushTimer = null;
        void this.flush();
      }, FLUSH_INTERVAL_MS);
    }
  }

  info(source: string, message: string, metadata: Record<string, unknown> = {}) {
    this.log("info", source, message, metadata);
  }

  warn(source: string, message: string, metadata: Record<string, unknown> = {}) {
    this.log("warn", source, message, metadata);
  }

  error(source: string, message: string, metadata: Record<string, unknown> = {}) {
    this.log("error", source, message, metadata);
  }

  async flush() {
    if (isTestMode || this.inFlight || !this.queue.length) {
      return;
    }
    const batch = this.queue.splice(0, MAX_BATCH_SIZE);
    this.inFlight = true;
    try {
      await api.postFrontendLogs(batch);
    } catch {
      this.queue = [...batch, ...this.queue].slice(-200);
    } finally {
      this.inFlight = false;
    }
  }

  flushWithBeacon() {
    if (isTestMode || !this.queue.length || typeof navigator.sendBeacon !== "function") {
      return;
    }
    const batch = this.queue.splice(0, this.queue.length);
    const blob = new Blob([JSON.stringify({ records: batch })], { type: "application/json" });
    navigator.sendBeacon(LOG_ENDPOINT, blob);
  }

  private bindLifecycleListeners() {
    if (this.listenersBound || typeof window === "undefined") {
      return;
    }
    this.listenersBound = true;
    window.addEventListener("beforeunload", () => {
      this.flushWithBeacon();
    });
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "hidden") {
        this.flushWithBeacon();
      }
    });
  }
}

export const frontendLogger = new FrontendLogger();
