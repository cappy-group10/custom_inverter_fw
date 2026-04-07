import { useEffect, useState } from "react";

import { getPortHint } from "../lib/selectors";
import { useDashboard } from "../context/DashboardContext";

interface SessionLauncherProps {
  title: string;
  description: string;
  submitLabel: string;
  compact?: boolean;
  onStarted?: () => void;
}

export function SessionLauncher({
  title,
  description,
  submitLabel,
  compact = false,
  onStarted,
}: SessionLauncherProps) {
  const { ports, snapshot, wsConnected, loading, reloadPorts, startSession, stopSession } = useDashboard();
  const [port, setPort] = useState<string>("demo");
  const [baudrate, setBaudrate] = useState<number>(115200);
  const [joystickIndex, setJoystickIndex] = useState<number>(0);

  useEffect(() => {
    setPort(snapshot.port || "demo");
    setBaudrate(snapshot.baudrate || 115200);
    setJoystickIndex(snapshot.joystick_index || 0);
  }, [snapshot.port, snapshot.baudrate, snapshot.joystick_index]);

  useEffect(() => {
    if (!ports.length) {
      return;
    }
    const values = new Set(ports.map((item) => item.port));
    if (!values.has(port)) {
      setPort("demo");
    }
  }, [ports, port]);

  const hint = getPortHint(port, ports);

  return (
    <section className={`panel connection-panel${compact ? " compact" : ""}`}>
      <div className="panel-headline">
        <div>
          <p className="eyebrow">Connection Launcher</p>
          <h2>{title}</h2>
          <p className="panel-copy">{description}</p>
        </div>
        <span className={`status-chip ${wsConnected ? "good" : "muted"}`}>
          {wsConnected ? "Socket live" : "Socket offline"}
        </span>
      </div>

      <div className="form-grid">
        <label className="field field-wide">
          <span>Port</span>
          <select value={port} onChange={(event) => setPort(event.target.value)}>
            {ports.map((item) => (
              <option key={item.port} value={item.port}>
                {item.label}
              </option>
            ))}
          </select>
        </label>
        <label className="field">
          <span>Baud Rate</span>
          <input
            type="number"
            value={baudrate}
            min={1}
            step={1}
            onChange={(event) => setBaudrate(Number(event.target.value))}
          />
        </label>
        <label className="field">
          <span>Joystick Index</span>
          <input
            type="number"
            value={joystickIndex}
            min={0}
            step={1}
            onChange={(event) => setJoystickIndex(Number(event.target.value))}
          />
        </label>
      </div>

      <p className={`callout ${hint.tone}`}>{hint.message}</p>

      <div className="action-row">
        <button
          className="button primary"
          disabled={loading.session}
          onClick={async () => {
            try {
              await startSession({
                port: port === "demo" ? "demo" : port,
                baudrate,
                joystick_index: joystickIndex,
                mode: "drive",
              });
              onStarted?.();
            } catch {
              // The shared dashboard context already surfaces a visible error banner.
            }
          }}
        >
          {loading.session ? "Starting..." : submitLabel}
        </button>
        <button
          className="button"
          disabled={loading.session}
          onClick={() => {
            void stopSession().catch(() => undefined);
          }}
        >
          Stop
        </button>
        <button className="button ghost" disabled={loading.ports} onClick={() => void reloadPorts()}>
          {loading.ports ? "Refreshing..." : "Refresh Ports"}
        </button>
      </div>

      <dl className="meta-grid">
        <div>
          <dt>Session</dt>
          <dd>{snapshot.session_state}</dd>
        </div>
        <div>
          <dt>Controller</dt>
          <dd>{snapshot.joystick_name || "Not connected"}</dd>
        </div>
        <div>
          <dt>Port</dt>
          <dd>{snapshot.port || "demo"}</dd>
        </div>
      </dl>
    </section>
  );
}
