import { Link } from "react-router-dom";

import { ControllerMonitor } from "../components/ControllerMonitor";
import { SessionLauncher } from "../components/SessionLauncher";
import { UartDebugTerminal } from "../components/UartDebugTerminal";
import { formatSigned, formatTimestamp, normalizeCtrlState } from "../lib/selectors";
import { useDashboard } from "../context/DashboardContext";

export function ConfigurePage() {
  const { primaryMcu, snapshot } = useDashboard();

  return (
    <div className="page-stack">
      <div className="dashboard-grid">
        <SessionLauncher
          title="Configure controller and UART"
          description="Use this page to start or stop the runtime, validate link health, and confirm the controller mapping before opening the motor page."
          submitLabel="Start drive runtime"
          compact
        />

        <section className="panel">
          <div className="panel-headline">
            <div>
              <p className="eyebrow">Transport Health</p>
              <h2>Live session summary</h2>
              <p className="panel-copy">Compact health view for the controller, UART path, and latest command/telemetry state.</p>
            </div>
            <span className={`status-chip ${snapshot.health.telemetry_stale ? "danger" : snapshot.health.has_mcu_telemetry ? "good" : "muted"}`}>
              {snapshot.health.telemetry_stale ? "Telemetry stale" : snapshot.health.has_mcu_telemetry ? "Telemetry live" : "No telemetry"}
            </span>
          </div>

          <div className="stat-ribbon">
            <article className="mini-stat">
              <span>TX Frames</span>
              <strong>{snapshot.counters.tx_frames ?? 0}</strong>
            </article>
            <article className="mini-stat">
              <span>RX Frames</span>
              <strong>{snapshot.counters.rx_frames ?? 0}</strong>
            </article>
            <article className="mini-stat">
              <span>Host State</span>
              <strong>{normalizeCtrlState(snapshot.last_host_command?.ctrl_state)}</strong>
            </article>
            <article className="mini-stat">
              <span>MCU State</span>
              <strong>{normalizeCtrlState(snapshot.latest_mcu_status?.ctrl_state)}</strong>
            </article>
            <article className="mini-stat">
              <span>Last frame</span>
              <strong>{formatTimestamp(snapshot.health.last_frame_at)}</strong>
            </article>
            <article className="mini-stat">
              <span>Speed Ref</span>
              <strong>{formatSigned(snapshot.last_host_command?.speed_ref, 4)}</strong>
            </article>
          </div>

          <div className="action-row">
            <Link className={`button ${primaryMcu ? "primary" : "ghost"}`} to={primaryMcu?.detail_path || "/"}>
              {primaryMcu ? "Open motor page" : "Motor page unavailable"}
            </Link>
          </div>
        </section>
      </div>

      <ControllerMonitor snapshot={snapshot} />
      <UartDebugTerminal snapshot={snapshot} />
    </div>
  );
}
