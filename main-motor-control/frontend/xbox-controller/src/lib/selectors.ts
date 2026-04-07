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
    motor_config: {
      base_speed_rpm: 0,
      base_current_a: 0,
      vdcbus_min_v: 0,
      vdcbus_max_v: 0,
      rated_input_power_w: 0,
    },
    last_host_command: null,
    latest_mcu_status: null,
    active_override: null,
    recent_faults: [],
    recent_frames: [],
    recent_events: [],
    telemetry_samples: [],
    music_state: null,
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

  const snapshot = previous ?? createEmptySnapshot();
  const updatedAt = Date.now() / 1000;

  switch (type) {
    case "controller_state":
      return {
        ...snapshot,
        updated_at: updatedAt,
        controller_state: payload,
      };
    case "host_command":
      return {
        ...snapshot,
        updated_at: updatedAt,
        last_host_command: payload,
      };
    case "tx_frame":
      return appendFrame(snapshot, payload, updatedAt);
    case "mcu_status":
      return mergeStatusEvent(snapshot, payload, lastChartSampleMsRef, updatedAt);
    case "fault":
      return mergeFaultEvent(snapshot, payload, updatedAt);
    case "health":
      return {
        ...snapshot,
        updated_at: updatedAt,
        health: payload,
      };
    case "event":
      return appendEvents(snapshot, [payload], updatedAt);
    case "ui_tick":
      return mergeUiTick(snapshot, payload, lastChartSampleMsRef, updatedAt);
    default:
      return {
        ...snapshot,
        updated_at: updatedAt,
      };
  }
}

function mergeUiTick(
  snapshot: SessionSnapshot,
  payload: any,
  lastChartSampleMsRef: { current: number },
  updatedAt: number,
): SessionSnapshot {
  let nextSnapshot: SessionSnapshot = {
    ...snapshot,
    updated_at: updatedAt,
    mode: payload.mode ?? snapshot.mode,
    controller_state: payload.controller_state ?? snapshot.controller_state,
    motor_config: payload.motor_config ?? snapshot.motor_config,
    last_host_command: payload.last_host_command ?? snapshot.last_host_command,
    latest_mcu_status: payload.latest_mcu_status ?? snapshot.latest_mcu_status,
    music_state: payload.music_state ?? snapshot.music_state,
    health: payload.health ?? snapshot.health,
    counters: payload.counters ? { ...(snapshot.counters || {}), ...payload.counters } : snapshot.counters,
  };

  nextSnapshot = appendFrames(nextSnapshot, payload.new_frames || [], updatedAt, !payload.counters);
  nextSnapshot = appendFaults(nextSnapshot, payload.new_faults || [], updatedAt);
  nextSnapshot = appendEvents(nextSnapshot, payload.new_events || [], updatedAt);

  if (payload.latest_mcu_status) {
    appendTelemetrySample(
      nextSnapshot,
      payload.latest_mcu_status,
      payload.last_host_command ?? snapshot.last_host_command,
      lastChartSampleMsRef,
    );
  }

  return nextSnapshot;
}

function mergeStatusEvent(
  snapshot: SessionSnapshot,
  payload: any,
  lastChartSampleMsRef: { current: number },
  updatedAt: number,
): SessionSnapshot {
  let nextSnapshot: SessionSnapshot = {
    ...snapshot,
    updated_at: updatedAt,
    latest_mcu_status: payload.status,
  };

  if (payload.frame) {
    nextSnapshot = appendFrame(nextSnapshot, payload.frame, updatedAt);
  }

  appendTelemetrySample(nextSnapshot, payload.status, snapshot.last_host_command, lastChartSampleMsRef);
  return nextSnapshot;
}

function mergeFaultEvent(snapshot: SessionSnapshot, payload: any, updatedAt: number): SessionSnapshot {
  let nextSnapshot = appendFaults(snapshot, [payload.fault], updatedAt);
  if (payload.frame) {
    nextSnapshot = appendFrame(nextSnapshot, payload.frame, updatedAt);
  }
  return nextSnapshot;
}

function appendFrames(
  snapshot: SessionSnapshot,
  frames: FrameRecord[],
  updatedAt: number,
  updateCountersFromFrames: boolean,
): SessionSnapshot {
  if (!frames.length) {
    return snapshot;
  }

  const nextFrames = limitList([...(snapshot.recent_frames || []), ...frames], FRAME_LIMIT);
  const nextHealth = {
    ...(snapshot.health || {}),
    last_frame_at: frames[frames.length - 1]?.timestamp ?? snapshot.health?.last_frame_at ?? null,
  };

  if (!updateCountersFromFrames) {
    return {
      ...snapshot,
      updated_at: updatedAt,
      recent_frames: nextFrames,
      health: nextHealth,
    };
  }

  const nextCounters = { ...(snapshot.counters || {}) };
  for (const frame of frames) {
    if (frame.direction === "tx") {
      nextCounters.tx_frames = (nextCounters.tx_frames ?? 0) + 1;
    } else {
      nextCounters.rx_frames = (nextCounters.rx_frames ?? 0) + 1;
    }
    if (!frame.checksum_ok) {
      nextCounters.checksum_errors = (nextCounters.checksum_errors ?? 0) + 1;
    }
  }

  return {
    ...snapshot,
    updated_at: updatedAt,
    recent_frames: nextFrames,
    counters: nextCounters,
    health: nextHealth,
  };
}

function appendFrame(snapshot: SessionSnapshot, frame: FrameRecord, updatedAt: number): SessionSnapshot {
  return appendFrames(snapshot, [frame], updatedAt, true);
}

function appendFaults(snapshot: SessionSnapshot, faults: SessionSnapshot["recent_faults"], updatedAt: number): SessionSnapshot {
  if (!faults.length) {
    return snapshot;
  }
  return {
    ...snapshot,
    updated_at: updatedAt,
    recent_faults: limitList([...(snapshot.recent_faults || []), ...faults], EVENT_LIMIT),
  };
}

function appendEvents(snapshot: SessionSnapshot, events: SessionSnapshot["recent_events"], updatedAt: number): SessionSnapshot {
  if (!events.length) {
    return snapshot;
  }
  return {
    ...snapshot,
    updated_at: updatedAt,
    recent_events: limitList([...(snapshot.recent_events || []), ...events], EVENT_LIMIT),
  };
}

function appendTelemetrySample(
  snapshot: SessionSnapshot,
  status: SessionSnapshot["latest_mcu_status"],
  command: SessionSnapshot["last_host_command"],
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
        speed_fbk: Number(status?.speed_fbk || 0),
        id_ref: Number(command?.id_ref || 0),
        id_fbk: Number(status?.id_fbk || 0),
        iq_ref: Number(command?.iq_ref || 0),
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
  const mode = snapshot.mode || "drive";
  const ctrlState =
    mode === "music"
      ? normalizePlayState(snapshot.music_state?.play_state ?? "IDLE")
      : normalizeCtrlState(snapshot.latest_mcu_status?.ctrl_state ?? snapshot.last_host_command?.ctrl_state ?? "STOP");
  return {
    id: "primary",
    name: "Primary MCU",
    detail_path: mode === "music" ? "/configure#music" : "/mcu/primary",
    configure_path: mode === "music" ? "/configure#music" : "/configure",
    port: snapshot.port || "demo",
    session_state: snapshot.session_state,
    controller_connected: snapshot.controller_connected,
    is_demo: snapshot.port == null,
    has_mcu_telemetry: Boolean(snapshot.health?.has_mcu_telemetry),
    telemetry_stale: Boolean(snapshot.health?.telemetry_stale),
    last_frame_at: snapshot.health?.last_frame_at ?? null,
    ctrl_state: ctrlState,
    active_override: mode === "drive" ? snapshot.active_override ?? null : null,
    mode,
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
    motor_config: snapshot.motor_config,
    command,
    status,
    telemetry: {
      speed_ref: Number(status?.speed_ref ?? command?.speed_ref ?? 0),
      speed_fbk: Number(status?.speed_fbk ?? 0),
      id_ref: Number(command?.id_ref ?? 0),
      id_fbk: Number(status?.id_fbk ?? 0),
      iq_ref: Number(command?.iq_ref ?? 0),
      iq_fbk: Number(status?.iq_fbk ?? 0),
      current_as: Number(status?.current_as ?? 0),
      current_bs: Number(status?.current_bs ?? 0),
      current_cs: Number(status?.current_cs ?? 0),
      vdc_bus: Number(status?.vdc_bus ?? 0),
      temp_motor_winding_c: status?.temp_motor_winding_c ?? null,
      temp_mcu_c: status?.temp_mcu_c ?? null,
      temp_igbts_c: status?.temp_igbts_c ?? null,
    },
    transport: {
      tx_frames: Number(snapshot.counters?.tx_frames ?? 0),
      rx_frames: Number(snapshot.counters?.rx_frames ?? 0),
      checksum_errors: Number(snapshot.counters?.checksum_errors ?? 0),
      serial_errors: Number(snapshot.counters?.serial_errors ?? 0),
      last_frame_at: snapshot.health?.last_frame_at ?? null,
      last_status_at: snapshot.health?.last_status_at ?? null,
    },
    music_state: snapshot.music_state,
  };
}

export function normalizePlayState(value: unknown): string {
  if (typeof value === "string") {
    return value;
  }
  if (typeof value === "number") {
    switch (value) {
      case 0:
        return "IDLE";
      case 1:
        return "PLAYING";
      case 2:
        return "PAUSED";
      default:
        return String(value);
    }
  }
  return "IDLE";
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
