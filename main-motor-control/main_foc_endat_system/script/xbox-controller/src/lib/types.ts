export type SessionState = "idle" | "starting" | "running" | "stopped" | "error";
export type ActiveOverride = "BRAKE" | null;

export interface PortOption {
  port: string;
  label: string;
  description: string;
  hwid?: string;
  is_demo: boolean;
  category: string;
  recommended: boolean;
  possible_mcu_uart: boolean;
  usage: string;
}

export interface ButtonMap {
  [key: string]: boolean;
}

export interface ControllerState {
  left_x?: number;
  left_y?: number;
  right_x?: number;
  right_y?: number;
  left_trigger?: number;
  right_trigger?: number;
  buttons: ButtonMap;
}

export interface ControllerLayoutDescriptor {
  control_id: string;
  label: string;
  group: string;
  mapped: boolean;
  mapping_target: string;
  mapping_text: string;
}

export interface HostCommand {
  ctrl_state?: string | number;
  speed_ref?: number;
  id_ref?: number;
  iq_ref?: number;
}

export interface MCUStatus {
  run_motor?: number;
  ctrl_state?: string | number;
  trip_flag?: number;
  speed_ref?: number;
  pos_mech_theta?: number;
  vdc_bus?: number;
  id_fbk?: number;
  iq_fbk?: number;
  current_as?: number;
  current_bs?: number;
  current_cs?: number;
  isr_ticker?: number;
}

export interface MCUFault {
  trip_flag?: number;
  trip_count?: number;
}

export interface FrameRecord {
  direction: "tx" | "rx" | string;
  frame_id: number;
  frame_name: string;
  raw_hex: string;
  decoded: Record<string, unknown>;
  checksum_ok: boolean;
  timestamp: number;
}

export interface EventRecord {
  kind: string;
  title: string;
  message: string;
  timestamp: number;
  data: Record<string, unknown>;
}

export interface TelemetrySample {
  timestamp: number;
  speed_ref: number;
  id_fbk: number;
  iq_fbk: number;
  vdc_bus: number;
  current_as: number;
  current_bs: number;
  current_cs: number;
}

export interface HealthState {
  controller_connected?: boolean;
  port_open?: boolean;
  terminal_only?: boolean;
  has_mcu_telemetry?: boolean;
  telemetry_stale?: boolean;
  last_error?: string | null;
  last_frame_at?: number | null;
  last_status_at?: number | null;
}

export interface SessionSnapshot {
  session_state: SessionState;
  mode?: string;
  port: string | null;
  baudrate: number;
  joystick_index: number;
  joystick_name: string;
  mapping_name?: string;
  controller_connected: boolean;
  controller_state: ControllerState | null;
  controller_layout: ControllerLayoutDescriptor[];
  last_host_command: HostCommand | null;
  latest_mcu_status: MCUStatus | null;
  active_override: ActiveOverride;
  recent_faults: MCUFault[];
  recent_frames: FrameRecord[];
  recent_events: EventRecord[];
  telemetry_samples: TelemetrySample[];
  counters: Record<string, number>;
  health: HealthState;
  updated_at: number;
  started_at: number | null;
  stopped_at: number | null;
  last_error: string | null;
}

export interface StartSessionPayload {
  port: string | null;
  baudrate: number;
  joystick_index: number;
}

export interface McuSummary {
  id: string;
  name: string;
  detail_path: string;
  configure_path: string;
  port: string;
  session_state: SessionState;
  controller_connected: boolean;
  is_demo: boolean;
  has_mcu_telemetry: boolean;
  telemetry_stale: boolean;
  last_frame_at: number | null;
  ctrl_state: string;
  active_override: ActiveOverride;
}

export interface McuDetail extends McuSummary {
  baudrate: number;
  joystick_index: number;
  joystick_name: string;
  command: HostCommand | null;
  status: MCUStatus | null;
  telemetry: {
    speed_ref: number;
    current_as: number;
    current_bs: number;
    current_cs: number;
    vdc_bus: number;
    temperature_c: number | null;
    temperature_available: boolean;
  };
  transport: {
    tx_frames: number;
    rx_frames: number;
    checksum_errors: number;
    serial_errors: number;
    last_frame_at: number | null;
    last_status_at: number | null;
  };
}
