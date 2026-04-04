import type {
  ActiveOverride,
  ControllerLayoutDescriptor,
  ControllerState,
  FrameRecord,
  McuDetail,
  McuSummary,
  PortOption,
  SessionSnapshot,
} from "./types";

const FRAME_LIMIT = 1000;
const EVENT_LIMIT = 50;
const TELEMETRY_LIMIT = 300;
const TELEMETRY_SAMPLE_MS = 100;

export function createEmptySnapshot(): SessionSnapshot {
  return {
    session_state: "idle",
    mode: "drive",
    port: null,
    baudrate: 115200,
    joystick_index: 0,
    joystick_name: "",
    controller_connected: false,
    controller_state: null,
    controller_layout: [],
    last_host_command: null,
    latest_mcu_status: null,
    active_override: null,
    recent_faults: [],
    recent_frames: [],
    recent_events: [],
    telemetry_samples: [],
    counters: {
      loop_iterations: 0,
      tx_frames: 0,
      rx_frames: 0,
      status_frames: 0,
      fault_frames: 0,
      checksum_errors: 0,
      serial_errors: 0,
    },
    health: {
      terminal_only: true,
      has_mcu_telemetry: false,
      telemetry_stale: false,
      last_error: null,
      last_frame_at: null,
      last_status_at: null,
    },
    updated_at: 0,
    started_at: null,
    stopped_at: null,
    last_error: null,
  };
}

export function limitList<T>(items: T[], limit: number): T[] {
  return items.slice(Math.max(items.length - limit, 0));
}

export function mergeStreamEvent(
  previous: SessionSnapshot | null,
  type: string,
  payload: any,
  lastChartSampleMsRef: { current: number },
): SessionSnapshot {
  if (type === "snapshot") {
    return payload as SessionSnapshot;
  }

  const snapshot = previous ? structuredClone(previous) : createEmptySnapshot();
  snapshot.updated_at = Date.now() / 1000;

  switch (type) {
    case "controller_state":
      snapshot.controller_state = payload;
      return snapshot;
    case "host_command":
      snapshot.last_host_command = payload;
      return snapshot;
    case "tx_frame":
      return appendFrame(snapshot, payload);
    case "mcu_status":
      snapshot.latest_mcu_status = payload.status;
      if (payload.frame) {
        appendFrame(snapshot, payload.frame);
      }
      appendTelemetrySample(snapshot, payload.status, lastChartSampleMsRef);
      return snapshot;
    case "fault":
      snapshot.recent_faults = limitList([...(snapshot.recent_faults || []), payload.fault], EVENT_LIMIT);
      if (payload.frame) {
        appendFrame(snapshot, payload.frame);
      }
      return snapshot;
    case "health":
      snapshot.health = payload;
      return snapshot;
    case "event":
      snapshot.recent_events = limitList([...(snapshot.recent_events || []), payload], EVENT_LIMIT);
      return snapshot;
    default:
      return snapshot;
  }
}

function appendFrame(snapshot: SessionSnapshot, frame: FrameRecord): SessionSnapshot {
  snapshot.recent_frames = limitList([...(snapshot.recent_frames || []), frame], FRAME_LIMIT);
  const nextCounters = { ...(snapshot.counters || {}) };
  if (frame.direction === "tx") {
    nextCounters.tx_frames = (nextCounters.tx_frames ?? 0) + 1;
  } else {
    nextCounters.rx_frames = (nextCounters.rx_frames ?? 0) + 1;
  }
  if (!frame.checksum_ok) {
    nextCounters.checksum_errors = (nextCounters.checksum_errors ?? 0) + 1;
  }
  snapshot.counters = nextCounters;
  snapshot.health = {
    ...(snapshot.health || {}),
    last_frame_at: frame.timestamp,
  };
  return snapshot;
}

function appendTelemetrySample(
  snapshot: SessionSnapshot,
  status: SessionSnapshot["latest_mcu_status"],
  lastChartSampleMsRef: { current: number },
) {
  const nowMs = Date.now();
  if (nowMs - lastChartSampleMsRef.current < TELEMETRY_SAMPLE_MS) {
    return;
  }
  lastChartSampleMsRef.current = nowMs;
  snapshot.telemetry_samples = limitList(
    [
      ...(snapshot.telemetry_samples || []),
      {
        timestamp: nowMs / 1000,
        speed_ref: Number(status?.speed_ref || 0),
        id_fbk: Number(status?.id_fbk || 0),
        iq_fbk: Number(status?.iq_fbk || 0),
        vdc_bus: Number(status?.vdc_bus || 0),
        current_as: Number(status?.current_as || 0),
        current_bs: Number(status?.current_bs || 0),
        current_cs: Number(status?.current_cs || 0),
      },
    ],
    TELEMETRY_LIMIT,
  );
}

export function derivePrimaryMcuSummary(snapshot: SessionSnapshot | null): McuSummary | null {
  if (!snapshot || !snapshot.started_at || !["starting", "running", "error"].includes(snapshot.session_state)) {
    return null;
  }
  return {
    id: "primary",
    name: "Primary MCU",
    detail_path: "/mcu/primary",
    configure_path: "/configure",
    port: snapshot.port || "demo",
    session_state: snapshot.session_state,
    controller_connected: snapshot.controller_connected,
    is_demo: snapshot.port == null,
    has_mcu_telemetry: Boolean(snapshot.health?.has_mcu_telemetry),
    telemetry_stale: Boolean(snapshot.health?.telemetry_stale),
    last_frame_at: snapshot.health?.last_frame_at ?? null,
    ctrl_state: normalizeCtrlState(
      snapshot.latest_mcu_status?.ctrl_state ?? snapshot.last_host_command?.ctrl_state ?? "STOP",
    ),
    active_override: snapshot.active_override ?? null,
  };
}

export function derivePrimaryMcuDetail(snapshot: SessionSnapshot | null): McuDetail | null {
  const summary = derivePrimaryMcuSummary(snapshot);
  if (!summary || !snapshot) {
    return null;
  }
  const status = snapshot.latest_mcu_status;
  const command = snapshot.last_host_command;
  return {
    ...summary,
    baudrate: snapshot.baudrate,
    joystick_index: snapshot.joystick_index,
    joystick_name: snapshot.joystick_name,
    command,
    status,
    telemetry: {
      speed_ref: Number(status?.speed_ref ?? command?.speed_ref ?? 0),
      current_as: Number(status?.current_as ?? 0),
      current_bs: Number(status?.current_bs ?? 0),
      current_cs: Number(status?.current_cs ?? 0),
      vdc_bus: Number(status?.vdc_bus ?? 0),
      temperature_c: null,
      temperature_available: false,
    },
    transport: {
      tx_frames: Number(snapshot.counters?.tx_frames ?? 0),
      rx_frames: Number(snapshot.counters?.rx_frames ?? 0),
      checksum_errors: Number(snapshot.counters?.checksum_errors ?? 0),
      serial_errors: Number(snapshot.counters?.serial_errors ?? 0),
      last_frame_at: snapshot.health?.last_frame_at ?? null,
      last_status_at: snapshot.health?.last_status_at ?? null,
    },
  };
}

export function normalizeCtrlState(value: unknown): string {
  if (typeof value === "string") {
    return value;
  }
  if (typeof value === "number") {
    switch (value) {
      case 0:
        return "STOP";
      case 1:
        return "RUN";
      case 2:
        return "BRAKE";
      case 3:
        return "RESET";
      case 4:
        return "FAULT";
      default:
        return String(value);
    }
  }
  return "STOP";
}

export function formatSigned(value: number | undefined | null, digits = 2): string {
  const safe = Number(value || 0);
  return `${safe >= 0 ? "+" : ""}${safe.toFixed(digits)}`;
}

export function formatFixed(value: number | undefined | null, digits = 2): string {
  return Number(value || 0).toFixed(digits);
}

export function formatTimestamp(timestamp: number | undefined | null): string {
  if (!timestamp) {
    return "—";
  }
  return new Date(timestamp * 1000).toLocaleTimeString();
}

export function formatTripFlag(value: number | undefined | null): string {
  return `0x${Number(value || 0).toString(16).padStart(4, "0")}`;
}

export function compactDecoded(decoded: unknown, limit = 120): string {
  const safe = JSON.stringify(decoded || {});
  return safe.length > limit ? `${safe.slice(0, limit)}...` : safe;
}

export function compactRawHex(rawHex: string, limit = 60): string {
  const safe = (rawHex || "").trim();
  return safe.length > limit ? `${safe.slice(0, limit)}...` : safe;
}

export function getRawBytes(rawHex: string): number[] {
  if (!rawHex) {
    return [];
  }
  return rawHex
    .trim()
    .split(/\s+/)
    .filter(Boolean)
    .map((token) => Number.parseInt(token, 16))
    .filter((value) => Number.isFinite(value));
}

export function getAsciiPreview(rawHex: string): string {
  const bytes = getRawBytes(rawHex);
  return bytes.map((byte) => (byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : ".")).join("");
}

export function getControlMappings(layout: ControllerLayoutDescriptor[]): string[] {
  return layout
    .filter((item) => item.mapped)
    .map((item) => `${item.label}: ${item.mapping_text}`);
}

export function getActiveButtons(controllerState: ControllerState | null): string[] {
  return Object.entries(controllerState?.buttons || {})
    .filter(([, active]) => active)
    .map(([name]) => name);
}

export function getPortHint(selectedPort: string | null, ports: PortOption[]): { message: string; tone: "neutral" | "good" | "warn" } {
  const selected = ports.find((port) => port.port === (selectedPort || "demo")) || null;
  const recommended = ports.find((port) => port.recommended) || null;

  if (!selected || selected.is_demo) {
    return {
      message: "Demo mode runs the controller and UI without opening the MCU UART.",
      tone: "neutral",
    };
  }
  if (selected.recommended) {
    return {
      message: `${selected.label} is the best match for the MCU telemetry UART.`,
      tone: "good",
    };
  }
  if (selected.category === "system" || selected.category === "debug_uart") {
    return {
      message: recommended
        ? `${selected.label} is not the main MCU telemetry port. Prefer ${recommended.label}.`
        : `${selected.label} is not the main MCU telemetry port.`,
      tone: "warn",
    };
  }
  return {
    message: selected.usage || "Verify the serial device identity before starting the session.",
    tone: selected.possible_mcu_uart ? "good" : "neutral",
  };
}

export function getOverrideLabel(override: ActiveOverride): string {
  return override ? `${override} latched` : "No active override";
}
