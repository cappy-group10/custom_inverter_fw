import { useEffect, useMemo, useRef, useState } from "react";
import { Link, useLocation, useNavigate, useSearchParams } from "react-router-dom";

import { MotorControlPanel } from "../components/MotorControlPanel";
import { MusicControlPanel } from "../components/MusicControlPanel";
import { InfoHint, UiIcon, type IconName } from "../components/UiChrome";
import { UartDebugTerminal } from "../components/UartDebugTerminal";
import { useDashboard } from "../context/DashboardContext";
import { frontendLogger } from "../lib/frontendLogger";
import {
  getConnectionInstance,
  getMostRecentlyOpenedInstance,
  markConnectionInstanceOpened,
  updateConnectionInstanceFromSnapshot,
} from "../lib/instances";
import {
  formatFixed,
  formatSigned,
  formatTimestamp,
  formatTripFlag,
} from "../lib/selectors";
import type {
  ControllerLayoutDescriptor,
  ControllerState,
  EventRecord,
  PortOption,
  SessionSnapshot,
  SessionMode,
  TelemetrySample,
} from "../lib/types";

const buttonNames = [
  "a",
  "b",
  "x",
  "y",
  "lb",
  "rb",
  "back",
  "start",
  "guide",
  "left_stick",
  "right_stick",
  "dpad_up",
  "dpad_down",
  "dpad_left",
  "dpad_right",
] as const;

const controllerGroupMeta = {
  face_buttons: {
    title: "Face Buttons",
    description: "Primary drive-state actions on the right side of the controller.",
  },
  dpad: {
    title: "D-Pad",
    description: "Discrete reference trim controls for `iq_ref` and `id_ref`.",
  },
  sticks: {
    title: "Sticks",
    description: "Continuous axis inputs and stick-click placeholders.",
  },
  shoulders: {
    title: "Shoulders & Triggers",
    description: "Top-edge controls, currently reserved in drive mode unless noted.",
  },
  center: {
    title: "Center Controls",
    description: "Start/Back/Guide actions near the middle of the gamepad.",
  },
} satisfies Record<string, { title: string; description: string }>;

const controllerGroupOrder = ["face_buttons", "dpad", "sticks", "shoulders", "center"];
const controllerControlOrder = [
  "left_trigger",
  "lb",
  "rb",
  "right_trigger",
  "left_stick",
  "right_stick",
  "dpad_up",
  "dpad_down",
  "dpad_left",
  "dpad_right",
  "back",
  "guide",
  "start",
  "x",
  "y",
  "a",
  "b",
];
const tabs = [
  { id: "overview", title: "Overview", copy: "Session, transport, and host command", icon: "overview" },
  { id: "music", title: "Music", copy: "Song playback, volume, and compact status", icon: "music" },
  { id: "controller", title: "Controller", copy: "Sticks, triggers, and button state", icon: "controller" },
  { id: "telemetry", title: "Telemetry", copy: "MCU status and rolling charts", icon: "telemetry" },
  { id: "motor", title: "Motor Control", copy: "Speedometer, electrical stats, and emergency brake", icon: "motor" },
  { id: "uart", title: "UART Debug", copy: "Live TX/RX terminal and frame inspector", icon: "uart" },
  { id: "events", title: "Events", copy: "Faults and runtime activity", icon: "events" },
] as const;

type TabId = (typeof tabs)[number]["id"];

function getTabFromHash(hash: string): TabId {
  const safe = hash.replace(/^#/, "").trim().toLowerCase();
  if (safe === "frames") {
    return "uart";
  }
  return tabs.some((tab) => tab.id === safe) ? (safe as TabId) : "overview";
}

function normalizeTrigger(value: number | undefined | null) {
  return Math.max(0, Math.min(100, ((Number(value || 0) + 1) / 2) * 100));
}

function HeadingLine({
  icon,
  title,
  hint,
  level = "h2",
}: {
  icon: IconName;
  title: string;
  hint: string;
  level?: "h2" | "h3";
}) {
  const Tag = level;
  return (
    <div className="heading-line">
      <UiIcon name={icon} className="heading-icon" />
      <Tag>{title}</Tag>
      <InfoHint text={hint} />
    </div>
  );
}

function getHealthBanner(snapshot: SessionSnapshot) {
  const health = snapshot.health || {};
  let title = "Waiting for session";
  let message = "Start the runtime to begin streaming controller and UART activity.";
  let level = "neutral";

  if (snapshot.session_state === "running") {
    title = snapshot.mode === "music" ? "Music runtime active" : "Drive runtime active";
    message = health.terminal_only
      ? snapshot.mode === "music"
        ? "Music commands are being staged in demo mode. No MCU telemetry is expected."
        : "Controller and host TX are running in demo mode. No MCU telemetry is expected."
      : snapshot.mode === "music"
        ? "Song commands, music status frames, and UART diagnostics are streaming."
        : "Controller, host UART, and MCU telemetry are streaming.";
    level = "good";
  }

  if (health.telemetry_stale) {
    title = "MCU telemetry is stale";
    message = "Frames are no longer arriving from the MCU. Check the UART wiring, board state, or firmware telemetry path.";
    level = "danger";
  }

  if (!health.terminal_only && snapshot.session_state === "running" && !health.has_mcu_telemetry) {
    title = "TX active, awaiting MCU telemetry";
    message = "Host commands are being sent, but no status frames have been received yet.";
    level = "warn";
  }

  if (snapshot.session_state === "error" || health.last_error) {
    title = "Runtime error";
    message = snapshot.last_error || health.last_error || "Unknown runtime error";
    level = "danger";
  }

  if (snapshot.session_state === "stopped") {
    title = "Runtime stopped";
    message = "The drive loop is idle. You can start a new session at any time.";
    level = "neutral";
  }

  return { title, message, level };
}

function ModePlaceholderPanel({
  title,
  copy,
}: {
  title: string;
  copy: string;
}) {
  return (
    <section className="panel mode-placeholder-panel">
      <div className="panel-heading">
        <div>
          <HeadingLine icon="music" title={title} hint={copy} />
          <p className="panel-copy">{copy}</p>
        </div>
      </div>
      <div className="placeholder">This view is drive-only. Switch to the Music tab for the active music session.</div>
    </section>
  );
}

function getTelemetryChip(health: SessionSnapshot["health"]) {
  if (health?.terminal_only) {
    return { text: "TX only", className: "badge warn" };
  }
  if (health?.has_mcu_telemetry && !health?.telemetry_stale) {
    return { text: "Telemetry live", className: "badge good" };
  }
  if (health?.telemetry_stale) {
    return { text: "Telemetry stale", className: "badge danger" };
  }
  return { text: "No telemetry", className: "badge" };
}

function getPortHelp(selectedValue: string, ports: PortOption[]) {
  const selectedPort = ports.find((portInfo) => portInfo.port === selectedValue) || null;
  const recommendedPort = ports.find((portInfo) => portInfo.recommended) || null;
  const possiblePorts = ports.filter((portInfo) => portInfo.possible_mcu_uart && !portInfo.is_demo);

  let message = "Choose the LaunchXL App UART when it appears. Demo mode skips MCU serial traffic.";
  let tone = "";

  if (selectedPort?.is_demo) {
    message = "Demo mode runs the controller and host command path without opening the MCU UART.";
  } else if (selectedPort?.recommended) {
    message = `${selectedPort.label} is the best match for the MCU telemetry UART. ${selectedPort.usage}`;
    tone = "good";
  } else if (selectedPort?.category === "debug_uart") {
    message = recommendedPort
      ? `${selectedPort.label} looks like the debug console. For motor-control telemetry, use ${recommendedPort.label} instead.`
      : `${selectedPort.label} looks like the debug console. The MCU telemetry stream is usually on the LaunchXL App UART.`;
    tone = "warn";
  } else if (selectedPort?.category === "system") {
    message = `${selectedPort.label} is a macOS system/debug port, not the MCU UART. Plug in the board and look for a /dev/cu.usbmodem* device.`;
    tone = "warn";
  } else if (selectedPort?.possible_mcu_uart) {
    message = `${selectedPort.label} could be the MCU UART. If you are using an XDS110 LaunchXL, choose the App UART when two usbmodem ports appear.`;
    tone = "good";
  } else if (!recommendedPort && !possiblePorts.length) {
    message = "No likely MCU UART is currently detected. On macOS, expect a /dev/cu.usbmodem* device after the board enumerates.";
    tone = "warn";
  } else if (recommendedPort) {
    message = `This port is not the first MCU choice. Prefer ${recommendedPort.label} for live motor-control telemetry.`;
    tone = "warn";
  }

  return { message, tone };
}

function groupControllerLayout(layout: ControllerLayoutDescriptor[]) {
  const grouped: Record<string, ControllerLayoutDescriptor[]> = {};
  const safeLayout = [...layout].sort(
    (left, right) => controllerControlOrder.indexOf(left.control_id) - controllerControlOrder.indexOf(right.control_id),
  );

  safeLayout.forEach((item) => {
    const groupName = item.group || "other";
    grouped[groupName] = [...(grouped[groupName] || []), item];
  });
  return grouped;
}

function createSparklinePath(values: number[], width: number, height: number, padding: number, min: number, range: number) {
  return values
    .map((value, index) => {
      const x = padding + (index / Math.max(values.length - 1, 1)) * (width - padding * 2);
      const y = height - padding - ((value - min) / range) * (height - padding * 2);
      return `${index === 0 ? "M" : "L"}${x.toFixed(1)} ${y.toFixed(1)}`;
    })
    .join(" ");
}

function Sparkline({
  samples,
  series,
}: {
  samples: TelemetrySample[];
  series: Array<{ key: keyof TelemetrySample; color: string }>;
}) {
  const safeSamples = samples.length
    ? samples
    : [
        {
          timestamp: 0,
          speed_ref: 0,
          speed_fbk: 0,
          id_ref: 0,
          id_fbk: 0,
          iq_ref: 0,
          iq_fbk: 0,
          vdc_bus: 0,
          current_as: 0,
          current_bs: 0,
          current_cs: 0,
        },
      ];
  const width = 320;
  const height = 120;
  const padding = 12;
  const values = series.flatMap((item) => safeSamples.map((sample) => Number(sample[item.key] || 0)));
  const min = Math.min(...values, 0);
  const max = Math.max(...values, 0);
  const range = max - min || 1;
  const baselineY = height - padding - ((0 - min) / range) * (height - padding * 2);

  return (
    <svg viewBox="0 0 320 120" preserveAspectRatio="none">
      <rect
        x="1"
        y="1"
        width={width - 2}
        height={height - 2}
        rx="18"
        ry="18"
        fill="rgba(255,255,255,0.02)"
        stroke="rgba(255,255,255,0.06)"
      />
      <line
        x1={padding}
        y1={baselineY}
        x2={width - padding}
        y2={baselineY}
        stroke="rgba(255,255,255,0.16)"
        strokeDasharray="4 6"
      />
      {series.map((item) => (
        <path
          key={item.key}
          d={createSparklinePath(
            safeSamples.map((sample) => Number(sample[item.key] || 0)),
            width,
            height,
            padding,
            min,
            range,
          )}
          fill="none"
          stroke={item.color}
          strokeWidth="3"
          strokeLinecap="round"
        />
      ))}
    </svg>
  );
}

function OverviewTab({
  snapshot,
  ports,
  wsConnected,
  loadingSession,
  loadingPorts,
  startSession,
  stopSession,
  reloadPorts,
}: {
  snapshot: SessionSnapshot;
  ports: PortOption[];
  wsConnected: boolean;
  loadingSession: boolean;
  loadingPorts: boolean;
  startSession: (payload: { port: string | null; baudrate: number; joystick_index: number; mode: SessionMode }) => Promise<void>;
  stopSession: () => Promise<void>;
  reloadPorts: () => Promise<void>;
}) {
  const [selectedPort, setSelectedPort] = useState("demo");
  const [baudrate, setBaudrate] = useState(115200);
  const [joystickIndex, setJoystickIndex] = useState(0);
  const [selectedMode, setSelectedMode] = useState<SessionMode>("drive");

  useEffect(() => {
    setSelectedPort(snapshot.port || "demo");
    setBaudrate(snapshot.baudrate || 115200);
    setJoystickIndex(snapshot.joystick_index || 0);
    setSelectedMode(snapshot.mode === "music" ? "music" : "drive");
  }, [snapshot.port, snapshot.baudrate, snapshot.joystick_index, snapshot.mode]);

  useEffect(() => {
    if (!ports.length) {
      return;
    }
    const values = new Set(ports.map((item) => item.port));
    if (!values.has(selectedPort)) {
      setSelectedPort("demo");
    }
  }, [ports, selectedPort]);

  const portHelp = getPortHelp(selectedPort, ports);
  const telemetryChip = getTelemetryChip(snapshot.health || {});
  const isMusicMode = selectedMode === "music";

  return (
    <div className="overview-grid">
      <section className="panel controls-panel">
        <div className="panel-heading">
          <div>
            <HeadingLine
              icon="session"
              title="Session Control"
              hint="Choose drive or music mode, select the serial target, and start or stop the active runtime for this instance."
            />
            <p className="panel-copy">
              Choose the operating mode, select the UART target, and start the drive or musical-motor runtime.
            </p>
          </div>
          <span className={`badge ${wsConnected ? "online" : "offline"}`}>{wsConnected ? "Socket live" : "Socket offline"}</span>
        </div>
        <div className="session-form">
          <label className="control-field control-field-wide">
            Mode
            <select value={selectedMode} onChange={(event) => setSelectedMode(event.target.value as SessionMode)}>
              <option value="drive">Drive</option>
              <option value="music">Music</option>
            </select>
          </label>
          <label className="control-field control-field-wide">
            Port
            <select value={selectedPort} onChange={(event) => setSelectedPort(event.target.value)}>
              {ports.map((port) => (
                <option key={port.port} value={port.port} title={[port.usage, port.description].filter(Boolean).join("\n")}>
                  {port.label}
                </option>
              ))}
            </select>
          </label>
          <div className="session-input-row">
            <label className="control-field">
              Baud Rate
              <input
                type="number"
                value={baudrate}
                min={1}
                step={1}
                onChange={(event) => setBaudrate(Number(event.target.value))}
              />
            </label>
            {!isMusicMode ? (
              <label className="control-field">
                Joystick Index
                <input
                  type="number"
                  value={joystickIndex}
                  min={0}
                  step={1}
                  onChange={(event) => setJoystickIndex(Number(event.target.value))}
                />
              </label>
            ) : null}
          </div>
        </div>
        <p className={`control-help${portHelp.tone ? ` ${portHelp.tone}` : ""}`}>{portHelp.message}</p>
        <div className="button-row session-actions">
          <button
            className="primary"
            disabled={loadingSession}
            onClick={() =>
              void startSession({
                port: selectedPort,
                baudrate,
                joystick_index: isMusicMode ? 0 : joystickIndex,
                mode: selectedMode,
              }).catch(() => undefined)
            }
          >
            {loadingSession ? "Starting..." : `Start ${isMusicMode ? "Music" : "Drive"} Session`}
          </button>
          <button disabled={loadingSession} onClick={() => void stopSession().catch(() => undefined)}>
            Stop
          </button>
          <button disabled={loadingPorts} onClick={() => void reloadPorts().catch(() => undefined)}>
            {loadingPorts ? "Refreshing..." : "Refresh Ports"}
          </button>
        </div>
        <dl className="meta-list">
          <div>
            <dt>Port</dt>
            <dd>{snapshot.port || "demo"}</dd>
          </div>
          <div>
            <dt>Controller</dt>
            <dd>{isMusicMode ? "Not used in music mode" : snapshot.joystick_name || "Not connected"}</dd>
          </div>
          <div>
            <dt>Started</dt>
            <dd>{formatTimestamp(snapshot.started_at)}</dd>
          </div>
        </dl>
      </section>

      <section className="panel summary-panel">
        <div className="panel-heading">
          <div>
            <HeadingLine
              icon="transport"
              title="Transport Health"
              hint="Monitor frame counts, checksum health, and how recently the host or MCU exchanged serial data."
            />
            <p className="panel-copy">Live counters and link freshness for the host-to-MCU serial path.</p>
          </div>
          <span className={telemetryChip.className}>{telemetryChip.text}</span>
        </div>
        <div className="metric-grid">
          <div className="metric-card">
            <span>TX Frames</span>
            <strong>{snapshot.counters.tx_frames ?? 0}</strong>
          </div>
          <div className="metric-card">
            <span>RX Frames</span>
            <strong>{snapshot.counters.rx_frames ?? 0}</strong>
          </div>
          <div className="metric-card">
            <span>Checksum Errors</span>
            <strong>{snapshot.counters.checksum_errors ?? 0}</strong>
          </div>
          <div className="metric-card">
            <span>Serial Errors</span>
            <strong>{snapshot.counters.serial_errors ?? 0}</strong>
          </div>
        </div>
        <dl className="meta-list compact">
          <div>
            <dt>Last MCU Status</dt>
            <dd>{formatTimestamp(snapshot.health.last_status_at)}</dd>
          </div>
          <div>
            <dt>Last Frame</dt>
            <dd>{formatTimestamp(snapshot.health.last_frame_at)}</dd>
          </div>
        </dl>
      </section>

      <section className="panel command-panel">
        <div className="panel-heading">
          <div>
            <HeadingLine
              icon="command"
              title={snapshot.mode === "music" ? "Music Command" : "Host Command"}
              hint={
                snapshot.mode === "music"
                  ? "Shows the latest song, control, or volume command being encoded on the laptop before it is transmitted over UART."
                  : "Shows the latest command values being encoded on the laptop before they are transmitted over UART."
              }
            />
            <p className="panel-copy">
              {snapshot.mode === "music"
                ? "The latest musical-motor command being packed and sent from the laptop."
                : "The latest control command being packed and sent from the laptop."}
            </p>
          </div>
          <span className="badge">
            {snapshot.mode === "music"
              ? snapshot.music_state?.play_state || "IDLE"
              : String(snapshot.last_host_command?.ctrl_state || "STOP")}
          </span>
        </div>
        {snapshot.mode === "music" ? (
          <div className="command-grid">
            <div className="command-meter">
              <span>Song</span>
              <strong>{snapshot.music_state?.last_command?.song_label || "None"}</strong>
            </div>
            <div className="command-meter">
              <span>Action</span>
              <strong>{snapshot.music_state?.last_command?.action || snapshot.music_state?.play_state || "IDLE"}</strong>
            </div>
            <div className="command-meter">
              <span>Volume</span>
              <strong>{formatFixed(snapshot.music_state?.volume, 2)}</strong>
            </div>
          </div>
        ) : (
          <div className="command-grid">
            <div className="command-meter">
              <span>Speed Ref</span>
              <strong>{formatSigned(snapshot.last_host_command?.speed_ref, 4)}</strong>
            </div>
            <div className="command-meter">
              <span>Id Ref</span>
              <strong>{formatSigned(snapshot.last_host_command?.id_ref, 4)}</strong>
            </div>
            <div className="command-meter">
              <span>Iq Ref</span>
              <strong>{formatSigned(snapshot.last_host_command?.iq_ref, 4)}</strong>
            </div>
          </div>
        )}
      </section>
    </div>
  );
}

function ControllerTab({ snapshot }: { snapshot: SessionSnapshot }) {
  const controllerState = snapshot.controller_state || { buttons: {} };
  const buttons = controllerState.buttons || {};
  const leftX = Number(controllerState.left_x || 0);
  const leftY = Number(controllerState.left_y || 0);
  const rightX = Number(controllerState.right_x || 0);
  const rightY = Number(controllerState.right_y || 0);
  const leftTrigger = normalizeTrigger(controllerState.left_trigger);
  const rightTrigger = normalizeTrigger(controllerState.right_trigger);
  const layoutByControl = new Map(snapshot.controller_layout.map((item) => [item.control_id, item]));
  const grouped = groupControllerLayout(snapshot.controller_layout);

  function getStickStyle(x: number, y: number) {
    const safeX = Math.max(-1, Math.min(1, x));
    const safeY = Math.max(-1, Math.min(1, y));
    const dx = safeX * 34;
    const dy = safeY * 34;
    const length = Math.sqrt(dx ** 2 + dy ** 2);
    const angle = Math.atan2(dy, dx) * (180 / Math.PI);
    return {
      dot: {
        left: `${50 + safeX * 34}%`,
        top: `${50 + safeY * 34}%`,
      },
      vector: {
        width: `${length}%`,
        opacity: length > 1 ? 1 : 0,
        transform: `translateY(-50%) rotate(${angle}deg)`,
      },
    };
  }

  const leftStickStyle = getStickStyle(leftX, leftY);
  const rightStickStyle = getStickStyle(rightX, rightY);

  function controlClass(controlId: string, baseClass: string) {
    const layout = layoutByControl.get(controlId);
    const active =
      buttonNames.includes(controlId as (typeof buttonNames)[number])
        ? Boolean(buttons[controlId])
        : controlId === "left_trigger"
          ? leftTrigger > 1
          : controlId === "right_trigger"
            ? rightTrigger > 1
            : controlId === "left_stick"
              ? Math.abs(leftX) > 0.02 || Math.abs(leftY) > 0.02 || Boolean(buttons.left_stick)
              : controlId === "right_stick"
                ? Math.abs(rightX) > 0.02 || Math.abs(rightY) > 0.02 || Boolean(buttons.right_stick)
                : false;

    return `${baseClass}${active ? " active" : ""}${layout && !layout.mapped ? " unmapped" : ""}`;
  }

  return (
    <section className="panel controller-panel">
      <div className="panel-heading">
        <div>
          <HeadingLine
            icon="controller"
            title="Controller View"
            hint="Inspect stick motion, trigger position, face buttons, D-pad actions, and the exact drive-mode variable each control maps to."
          />
          <p className="panel-copy">
            Inspect the live Xbox layout, grouped mappings, and the exact drive variable each input affects.
          </p>
        </div>
        <span className={`badge ${snapshot.controller_connected ? "good" : "offline"}`}>
          {snapshot.controller_connected ? "Connected" : "Disconnected"}
        </span>
      </div>
      <div className="controller-layout">
        <section className="controller-stage">
          <div className="controller-trigger-strip">
            <div className={controlClass("left_trigger", "trigger-module")} title={layoutByControl.get("left_trigger")?.mapping_text}>
              <div className="trigger-header">
                <span>LT</span>
                <span>{formatSigned(controllerState.left_trigger, 2)}</span>
              </div>
              <div className="bar-track trigger-track">
                <div className="bar-fill" style={{ width: `${leftTrigger}%` }} />
              </div>
            </div>
            <div className={controlClass("lb", "shoulder-button")} title={layoutByControl.get("lb")?.mapping_text}>
              LB
            </div>
            <div className={controlClass("rb", "shoulder-button")} title={layoutByControl.get("rb")?.mapping_text}>
              RB
            </div>
            <div className={controlClass("right_trigger", "trigger-module")} title={layoutByControl.get("right_trigger")?.mapping_text}>
              <div className="trigger-header">
                <span>RT</span>
                <span>{formatSigned(controllerState.right_trigger, 2)}</span>
              </div>
              <div className="bar-track trigger-track">
                <div className="bar-fill" style={{ width: `${rightTrigger}%` }} />
              </div>
            </div>
          </div>

          <div className="controller-shell">
            <div className="controller-body">
              <div className={controlClass("left_stick", "thumb-module left")} title={layoutByControl.get("left_stick")?.mapping_text}>
                <span className="module-label">Left Stick</span>
                <div className="stick-surface">
                  <div className="stick-center"></div>
                  <div className="stick-vector" style={leftStickStyle.vector}></div>
                  <div className="stick-dot" style={leftStickStyle.dot}></div>
                </div>
                <p>{`X ${formatSigned(leftX, 2)} / Y ${formatSigned(leftY, 2)}`}</p>
              </div>

              <div className="cluster dpad-cluster">
                <span className="cluster-label">D-Pad</span>
                <button type="button" className={controlClass("dpad_up", "control-node dpad-node up")} title={layoutByControl.get("dpad_up")?.mapping_text}>
                  Up
                </button>
                <button type="button" className={controlClass("dpad_left", "control-node dpad-node left")} title={layoutByControl.get("dpad_left")?.mapping_text}>
                  Left
                </button>
                <button type="button" className={controlClass("dpad_right", "control-node dpad-node right")} title={layoutByControl.get("dpad_right")?.mapping_text}>
                  Right
                </button>
                <button type="button" className={controlClass("dpad_down", "control-node dpad-node down")} title={layoutByControl.get("dpad_down")?.mapping_text}>
                  Down
                </button>
              </div>

              <div className="center-cluster">
                <button type="button" className={controlClass("back", "control-node pill-button")} title={layoutByControl.get("back")?.mapping_text}>
                  Back
                </button>
                <button type="button" className={controlClass("guide", "control-node guide-button")} title={layoutByControl.get("guide")?.mapping_text}>
                  Guide
                </button>
                <button type="button" className={controlClass("start", "control-node pill-button")} title={layoutByControl.get("start")?.mapping_text}>
                  Start
                </button>
              </div>

              <div className="cluster face-cluster">
                <span className="cluster-label">A / B / X / Y</span>
                <button type="button" className={controlClass("y", "control-node face-node y")} title={layoutByControl.get("y")?.mapping_text}>
                  Y
                </button>
                <button type="button" className={controlClass("x", "control-node face-node x")} title={layoutByControl.get("x")?.mapping_text}>
                  X
                </button>
                <button type="button" className={controlClass("b", "control-node face-node b")} title={layoutByControl.get("b")?.mapping_text}>
                  B
                </button>
                <button type="button" className={controlClass("a", "control-node face-node a")} title={layoutByControl.get("a")?.mapping_text}>
                  A
                </button>
              </div>

              <div className={controlClass("right_stick", "thumb-module right")} title={layoutByControl.get("right_stick")?.mapping_text}>
                <span className="module-label">Right Stick</span>
                <div className="stick-surface">
                  <div className="stick-center"></div>
                  <div className="stick-vector" style={rightStickStyle.vector}></div>
                  <div className="stick-dot" style={rightStickStyle.dot}></div>
                </div>
                <p>{`X ${formatSigned(rightX, 2)} / Y ${formatSigned(rightY, 2)}`}</p>
              </div>
            </div>
          </div>

          <div className="controller-status-strip">
            <article className="status-mini-card">
              <span>Host Ctrl State</span>
              <strong>{String(snapshot.last_host_command?.ctrl_state || "STOP")}</strong>
            </article>
            <article className="status-mini-card">
              <span>Mapped Speed Variable</span>
              <strong>{`speed_ref ${formatSigned(snapshot.last_host_command?.speed_ref, 4)}`}</strong>
            </article>
            <article className="status-mini-card">
              <span>Mapped D-Axis Variable</span>
              <strong>{`id_ref ${formatSigned(snapshot.last_host_command?.id_ref, 4)}`}</strong>
            </article>
            <article className="status-mini-card">
              <span>Mapped Q-Axis Variable</span>
              <strong>{`iq_ref ${formatSigned(snapshot.last_host_command?.iq_ref, 4)}`}</strong>
            </article>
          </div>
          <section className="controller-groups">
            {controllerGroupOrder
              .filter((groupName) => grouped[groupName]?.length)
              .map((groupName) => (
                <article key={groupName} className="controller-group-card">
                  <h3>{controllerGroupMeta[groupName].title}</h3>
                  <p>{controllerGroupMeta[groupName].description}</p>
                  <div className="controller-group-list">
                    {grouped[groupName].map((item) => (
                      <div key={item.control_id} className="controller-group-row">
                        <div>
                          <strong>{item.label}</strong>
                          <small>{item.mapping_text}</small>
                        </div>
                        <span className={`mapping-chip ${item.mapped ? "mapped" : "unmapped"}`}>{item.mapping_target}</span>
                      </div>
                    ))}
                  </div>
                </article>
              ))}
          </section>
        </section>
      </div>
    </section>
  );
}

function TelemetryTab({ snapshot }: { snapshot: SessionSnapshot }) {
  const status = snapshot.latest_mcu_status || {};
  const faults = snapshot.recent_faults || [];

  return (
    <div className="telemetry-workspace">
      <section className="panel telemetry-panel">
        <div className="panel-heading">
          <div>
            <HeadingLine
              icon="telemetry"
              title="MCU Telemetry"
              hint="Decoded status feedback from the MCU, including electrical values, state flags, and recent fault captures."
            />
            <p className="panel-copy">Decoded status feedback from the motor-control MCU.</p>
          </div>
          <span className="badge">{String(status.ctrl_state || "STOP")}</span>
        </div>
        <div className="metric-grid telemetry-grid">
          <div className="metric-card"><span>Speed Ref</span><strong>{formatSigned(status.speed_ref, 4)}</strong></div>
          <div className="metric-card"><span>Rotor Theta</span><strong>{formatSigned(status.pos_mech_theta, 4)}</strong></div>
          <div className="metric-card"><span>Vdc Bus</span><strong>{formatFixed(status.vdc_bus, 1)}</strong></div>
          <div className="metric-card"><span>Trip Flag</span><strong>{formatTripFlag(status.trip_flag)}</strong></div>
          <div className="metric-card"><span>Id Fbk</span><strong>{formatSigned(status.id_fbk, 4)}</strong></div>
          <div className="metric-card"><span>Iq Fbk</span><strong>{formatSigned(status.iq_fbk, 4)}</strong></div>
          <div className="metric-card"><span>Phase A</span><strong>{formatSigned(status.current_as, 3)}</strong></div>
          <div className="metric-card"><span>Phase B</span><strong>{formatSigned(status.current_bs, 3)}</strong></div>
          <div className="metric-card"><span>Phase C</span><strong>{formatSigned(status.current_cs, 3)}</strong></div>
          <div className="metric-card"><span>ISR Ticker</span><strong>{status.isr_ticker ?? 0}</strong></div>
        </div>
        <div className="fault-list">
          {faults.length ? (
            [...faults].reverse().map((fault, index) => (
              <article key={`${fault.trip_flag ?? 0}-${index}`} className="fault-card">
                <strong>{formatTripFlag(fault.trip_flag)}</strong>
                <p>{`Trip count ${fault.trip_count ?? 0}`}</p>
              </article>
            ))
          ) : (
            <div className="placeholder">No faults captured in this session.</div>
          )}
        </div>
      </section>

      <section className="panel charts-panel">
        <div className="panel-heading">
          <div>
            <HeadingLine
              icon="charts"
              title="Rolling Charts"
              hint="A compact 10 Hz trend view for the values that are most useful when validating live motion and electrical response."
            />
            <p className="panel-copy">A 10 Hz view of the values that change most during driving.</p>
          </div>
          <span className="badge muted">10 Hz UI samples</span>
        </div>
        <div className="chart-grid">
          <article className="chart-card">
            <h3>Speed Reference</h3>
            <Sparkline samples={snapshot.telemetry_samples || []} series={[{ key: "speed_ref", color: "#f18748" }]} />
          </article>
          <article className="chart-card">
            <h3>D/Q Currents</h3>
            <Sparkline
              samples={snapshot.telemetry_samples || []}
              series={[
                { key: "id_fbk", color: "#44d1c2" },
                { key: "iq_fbk", color: "#ffc857" },
              ]}
            />
          </article>
          <article className="chart-card">
            <h3>DC Bus</h3>
            <Sparkline samples={snapshot.telemetry_samples || []} series={[{ key: "vdc_bus", color: "#9de7ff" }]} />
          </article>
          <article className="chart-card">
            <h3>Phase Currents</h3>
            <Sparkline
              samples={snapshot.telemetry_samples || []}
              series={[
                { key: "current_as", color: "#44d1c2" },
                { key: "current_bs", color: "#f18748" },
                { key: "current_cs", color: "#ff6b6b" },
              ]}
            />
          </article>
        </div>
      </section>
    </div>
  );
}

function EventsTab({ events }: { events: EventRecord[] }) {
  const [paused, setPaused] = useState(false);
  const [frozenEvents, setFrozenEvents] = useState<EventRecord[] | null>(null);
  const [clearedBefore, setClearedBefore] = useState<number | null>(null);
  const baseEvents = paused && frozenEvents ? frozenEvents : events;
  const sourceEvents = useMemo(
    () => (clearedBefore === null ? baseEvents : baseEvents.filter((event) => event.timestamp > clearedBefore)),
    [baseEvents, clearedBefore],
  );

  return (
    <section className="panel events-panel wide-panel">
      <div className="panel-heading">
        <div>
          <HeadingLine
            icon="events"
            title="Event Feed"
            hint="Review session lifecycle events, button-edge records, runtime notices, and fault entries captured during operation."
          />
          <p className="panel-copy">Fault notifications, session lifecycle events, and runtime notices.</p>
        </div>
        <div className="button-row compact">
          <button
            onClick={() => {
              if (!paused) {
                setFrozenEvents(events);
              } else {
                setFrozenEvents(null);
              }
              setPaused((current) => !current);
            }}
          >
            {paused ? "Resume" : "Pause"}
          </button>
          <button
            onClick={() => {
              setClearedBefore(sourceEvents[sourceEvents.length - 1]?.timestamp ?? Date.now() / 1000);
              if (paused) {
                setFrozenEvents([]);
              }
            }}
          >
            Clear
          </button>
        </div>
      </div>
      <div className="log-list tall-log">
        {(paused ? sourceEvents : sourceEvents).length ? (
          [...sourceEvents].reverse().map((event, index) => (
            <article key={`${event.timestamp}-${event.kind}-${index}`} className="log-item">
              <div className="log-meta">
                <span className="log-title">{event.title || event.kind}</span>
                <span>{formatTimestamp(event.timestamp)}</span>
              </div>
              <div className="log-body">{event.message || ""}</div>
              {Object.keys(event.data || {}).length ? (
                <div className="log-code">{JSON.stringify(event.data)}</div>
              ) : null}
            </article>
          ))
        ) : (
          <div className="placeholder">No events yet.</div>
        )}
      </div>
    </section>
  );
}

export function DashboardPage() {
  const {
    snapshot,
    ports,
    wsConnected,
    loading,
    startSession,
    stopSession,
    reloadPorts,
    engageBrake,
    releaseBrake,
    playMusic,
    pauseMusic,
    resumeMusic,
    stopMusic,
    setMusicVolume,
  } = useDashboard();
  const location = useLocation();
  const navigate = useNavigate();
  const [searchParams] = useSearchParams();
  const [activeTab, setActiveTab] = useState<TabId>(getTabFromHash(location.hash));
  const [activeInstanceName, setActiveInstanceName] = useState("No instance selected");
  const previousSessionRef = useRef({ mode: snapshot.mode, sessionState: snapshot.session_state });
  const instanceId = searchParams.get("instance");

  useEffect(() => {
    setActiveTab(getTabFromHash(location.hash));
  }, [location.hash]);

  useEffect(() => {
    if (instanceId) {
      const instance = markConnectionInstanceOpened(instanceId) || getConnectionInstance(instanceId);
      setActiveInstanceName(instance?.name || "No instance selected");
      return;
    }
    const mostRecent = getMostRecentlyOpenedInstance();
    if (mostRecent) {
      navigate(
        {
          pathname: location.pathname,
          search: `?instance=${encodeURIComponent(mostRecent.id)}`,
          hash: location.hash,
        },
        { replace: true },
      );
      return;
    }
    setActiveInstanceName("No instance selected");
  }, [instanceId, location.hash, location.pathname, navigate]);

  useEffect(() => {
    updateConnectionInstanceFromSnapshot(instanceId, {
      session_state: snapshot.session_state,
      port: snapshot.port,
      joystick_name: snapshot.joystick_name,
      health: {
        last_frame_at: snapshot.health.last_frame_at ?? null,
      },
    });
  }, [
    instanceId,
    snapshot.session_state,
    snapshot.port,
    snapshot.joystick_name,
    snapshot.health.last_frame_at,
  ]);

  useEffect(() => {
    frontendLogger.info("tabs", "Dashboard tab changed", { tab: activeTab });
  }, [activeTab]);

  useEffect(() => {
    const previous = previousSessionRef.current;
    const startedMusicSession =
      snapshot.mode === "music" &&
      snapshot.session_state === "running" &&
      (previous.mode !== "music" || previous.sessionState !== "running");

    if (startedMusicSession && activeTab === "overview") {
      setTab("music");
    }
    previousSessionRef.current = { mode: snapshot.mode, sessionState: snapshot.session_state };
  }, [activeTab, snapshot.mode, snapshot.session_state]);

  const healthBanner = getHealthBanner(snapshot);
  const motorPagePath = instanceId ? `/mcu/primary?instance=${encodeURIComponent(instanceId)}` : "/mcu/primary";

  function setTab(tab: TabId) {
    setActiveTab(tab);
      navigate(
        {
          pathname: location.pathname,
          search: location.search,
          hash: `#${tab}`,
        },
        { replace: true },
      );
  }

  return (
    <div className="page-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Emission Impossible</p>
          <h1>Inverter OS</h1>
          <p className="hero-copy">
            A professional operator console for drive control, musical-motor playback, host command frames, and MCU telemetry.
          </p>
        </div>
        <div className="hero-status">
          <Link className="status-chip muted page-link-chip" to="/">
            <UiIcon name="back" />
            Back to Instances
          </Link>
          <span className={`status-chip ${instanceId ? "good" : "muted"}`}>
            <UiIcon name="instances" />
            {activeInstanceName}
          </span>
          <span className="status-chip">
            <UiIcon name="shield" />
            {snapshot.session_state || "Idle"}
          </span>
          <span className="status-chip muted">
            <UiIcon name="controller-pad" />
            {snapshot.mode === "drive" ? "Drive Mode" : snapshot.mode || "Mode"}
          </span>
        </div>
      </header>

      <section className={`health-banner ${healthBanner.level}`}>
        <strong>{healthBanner.title}</strong>
        <span>{healthBanner.message}</span>
      </section>

      <main className="workspace">
        <nav className="tab-strip" aria-label="Dashboard sections">
          {tabs.map((tab) => (
            <button
              key={tab.id}
              id={`tab-button-${tab.id}`}
              className={`tab-button${activeTab === tab.id ? " active" : ""}`}
              type="button"
              role="tab"
              aria-selected={activeTab === tab.id}
              aria-controls={`tab-panel-${tab.id}`}
              onClick={() => setTab(tab.id)}
            >
              <strong className="tab-title-line">
                <UiIcon name={tab.icon} />
                {tab.title}
                <InfoHint text={tab.copy} interactive={false} />
              </strong>
              <span>{tab.copy}</span>
            </button>
          ))}
        </nav>

        <section
          id="tab-panel-overview"
          className={`tab-panel${activeTab === "overview" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-overview"
          hidden={activeTab !== "overview"}
        >
          <OverviewTab
            snapshot={snapshot}
            ports={ports}
            wsConnected={wsConnected}
            loadingSession={loading.session}
            loadingPorts={loading.ports}
            startSession={startSession}
            stopSession={stopSession}
            reloadPorts={reloadPorts}
          />
        </section>

        <section
          id="tab-panel-music"
          className={`tab-panel${activeTab === "music" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-music"
          hidden={activeTab !== "music"}
        >
          <MusicControlPanel
            snapshot={snapshot}
            loadingMusic={loading.music}
            onPlay={(songId, amplitude) => {
              void playMusic(songId, amplitude).catch(() => undefined);
            }}
            onPause={() => {
              void pauseMusic().catch(() => undefined);
            }}
            onResume={() => {
              void resumeMusic().catch(() => undefined);
            }}
            onStop={() => {
              void stopMusic().catch(() => undefined);
            }}
            onVolumeChange={(volume) => {
              void setMusicVolume(volume).catch(() => undefined);
            }}
            onOpenUart={() => setTab("uart")}
          />
        </section>

        <section
          id="tab-panel-controller"
          className={`tab-panel${activeTab === "controller" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-controller"
          hidden={activeTab !== "controller"}
        >
          {snapshot.mode === "music" ? (
            <ModePlaceholderPanel
              title="Controller View Unavailable"
              copy="The current session is running in music mode, so the controller visualization and drive mapping are intentionally left untouched."
            />
          ) : (
            <ControllerTab snapshot={snapshot} />
          )}
        </section>

        <section
          id="tab-panel-telemetry"
          className={`tab-panel${activeTab === "telemetry" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-telemetry"
          hidden={activeTab !== "telemetry"}
        >
          {snapshot.mode === "music" ? (
            <ModePlaceholderPanel
              title="Drive Telemetry Hidden"
              copy="Music sessions report song/status telemetry instead of the drive-mode electrical charts shown here."
            />
          ) : (
            <TelemetryTab snapshot={snapshot} />
          )}
        </section>

        <section
          id="tab-panel-motor"
          className={`tab-panel${activeTab === "motor" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-motor"
          hidden={activeTab !== "motor"}
        >
          {snapshot.mode === "music" ? (
            <ModePlaceholderPanel
              title="Drive Motor Controls Hidden"
              copy="Emergency brake, speedometer, and d/q current surfaces remain drive-only so the musical-motor path stays non-invasive."
            />
          ) : (
            <MotorControlPanel
              snapshot={snapshot}
              loadingBrake={loading.brake}
              detailPath={motorPagePath}
              onBrake={() => {
                void engageBrake().catch(() => undefined);
              }}
              onBrakeRelease={() => {
                void releaseBrake().catch(() => undefined);
              }}
            />
          )}
        </section>

        <section
          id="tab-panel-uart"
          className={`tab-panel${activeTab === "uart" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-uart"
          hidden={activeTab !== "uart"}
        >
          <UartDebugTerminal snapshot={snapshot} />
        </section>

        <section
          id="tab-panel-events"
          className={`tab-panel${activeTab === "events" ? " active" : ""}`}
          role="tabpanel"
          aria-labelledby="tab-button-events"
          hidden={activeTab !== "events"}
        >
          <EventsTab events={snapshot.recent_events || []} />
        </section>
      </main>
    </div>
  );
}
