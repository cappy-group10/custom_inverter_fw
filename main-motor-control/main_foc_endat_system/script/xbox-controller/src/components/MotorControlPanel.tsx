import { Link } from "react-router-dom";

import { formatFixed, formatSigned, formatTimestamp } from "../lib/selectors";
import type { SessionSnapshot } from "../lib/types";

interface MotorControlPanelProps {
  snapshot: SessionSnapshot;
  loadingBrake: boolean;
  onBrake: () => void;
  dedicatedPage?: boolean;
}

function polarToCartesian(centerX: number, centerY: number, radius: number, angleInDegrees: number) {
  const angleInRadians = ((angleInDegrees - 90) * Math.PI) / 180;
  return {
    x: centerX + radius * Math.cos(angleInRadians),
    y: centerY + radius * Math.sin(angleInRadians),
  };
}

function describeArc(centerX: number, centerY: number, radius: number, startAngle: number, endAngle: number) {
  const start = polarToCartesian(centerX, centerY, radius, endAngle);
  const end = polarToCartesian(centerX, centerY, radius, startAngle);
  const largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
  return ["M", start.x, start.y, "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y].join(" ");
}

function getTelemetryChip(health: SessionSnapshot["health"]) {
  if (health?.terminal_only) {
    return { text: "TX only", className: "badge warn" };
  }
  if (health?.telemetry_stale) {
    return { text: "Telemetry stale", className: "badge danger" };
  }
  if (health?.has_mcu_telemetry) {
    return { text: "Telemetry live", className: "badge good" };
  }
  return { text: "No telemetry", className: "badge" };
}

function MotorSpeedometer({
  value,
  hostState,
  mcuState,
  brakeLatched,
}: {
  value: number;
  hostState: string;
  mcuState: string;
  brakeLatched: boolean;
}) {
  const min = -0.3;
  const max = 0.3;
  const clamped = Math.max(min, Math.min(max, Number(value || 0)));
  const ratio = (clamped - min) / (max - min);
  const angle = -120 + ratio * 240;
  const needle = polarToCartesian(170, 168, 94, angle);
  const track = describeArc(170, 168, 104, -120, 120);
  const progress = describeArc(170, 168, 104, -120, angle);

  return (
    <article className="speedometer-card" data-testid="speedometer">
      <div className="speedometer-copy">
        <span className="speedometer-label">Reference Speed</span>
        <strong>{formatSigned(clamped, 3)}</strong>
        <small>per-unit speed_ref</small>
      </div>
      <svg id="motor-speedometer" viewBox="0 0 340 240" preserveAspectRatio="xMidYMid meet" role="img" aria-label="Reference speed gauge">
        <path d={track} className="speedometer-track" />
        <path d={progress} className="speedometer-progress" />
        <line x1="170" y1="168" x2={needle.x} y2={needle.y} className="speedometer-needle" />
        <circle cx="170" cy="168" r="10" className="speedometer-hub" />
        <text x="170" y="112" textAnchor="middle" className="speedometer-value">
          {formatSigned(clamped, 3)}
        </text>
        <text x="170" y="136" textAnchor="middle" className="speedometer-unit">
          per-unit speed_ref
        </text>
        <text x="54" y="190" textAnchor="middle" className="speedometer-tick">
          {min.toFixed(2)}
        </text>
        <text x="170" y="44" textAnchor="middle" className="speedometer-tick">
          0.00
        </text>
        <text x="286" y="190" textAnchor="middle" className="speedometer-tick">
          {max.toFixed(2)}
        </text>
        <text x="170" y="208" textAnchor="middle" className="speedometer-subcopy">
          {`Host ${hostState} · MCU ${mcuState}`}
        </text>
        {brakeLatched ? (
          <text x="170" y="64" textAnchor="middle" className="speedometer-override">
            BRAKE LATCHED
          </text>
        ) : null}
      </svg>
    </article>
  );
}

export function MotorControlPanel({
  snapshot,
  loadingBrake,
  onBrake,
  dedicatedPage = false,
}: MotorControlPanelProps) {
  const health = snapshot.health || {};
  const status = snapshot.latest_mcu_status || {};
  const command = snapshot.last_host_command || {};
  const speedRef = Number(status.speed_ref ?? command.speed_ref ?? 0);
  const hostState = String(command.ctrl_state || "STOP");
  const mcuState = String(status.ctrl_state || command.ctrl_state || "STOP");
  const brakeLatched = snapshot.active_override === "BRAKE";
  const telemetryChip = getTelemetryChip(health);
  const canBrake = snapshot.session_state === "running";

  return (
    <div className="motor-workspace">
      <section className="panel motor-panel">
        <div className="panel-heading">
          <div>
            <h2>Motor Control</h2>
            <p className="panel-copy">
              A compact drive-focused view with a speedometer dial, live electrical stats, and a hard-stop brake
              override.
            </p>
          </div>
          <span className={`badge ${brakeLatched ? "danger" : "muted"}`}>{brakeLatched ? "BRAKE latched" : "No override"}</span>
        </div>

        <div className="motor-hero-grid">
          <MotorSpeedometer value={speedRef} hostState={hostState} mcuState={mcuState} brakeLatched={brakeLatched} />

          <article className="motor-safety-card">
            <div>
              <h3>Emergency Brake</h3>
              <p className="panel-copy">
                This latches a BRAKE override above the controller mapping. Stop or restart the session to clear it.
              </p>
            </div>
            <button
              className="primary danger-button"
              disabled={!canBrake || brakeLatched || loadingBrake}
              onClick={onBrake}
            >
              {loadingBrake ? "Engaging Brake..." : brakeLatched ? "Brake Latched" : "Emergency Brake"}
            </button>
            <p className="control-help">
              {brakeLatched
                ? "BRAKE is currently latched. Stop or restart the session to clear the override."
                : canBrake
                  ? "Available while the drive runtime is running."
                  : "Start the drive session to enable the brake override."}
            </p>
            <dl className="meta-list compact">
              <div>
                <dt>Session</dt>
                <dd>{snapshot.session_state || "idle"}</dd>
              </div>
              <div>
                <dt>Port</dt>
                <dd>{snapshot.port || "demo"}</dd>
              </div>
              <div>
                <dt>Last Frame</dt>
                <dd>{formatTimestamp(health.last_frame_at)}</dd>
              </div>
            </dl>
            {!dedicatedPage ? (
              <div className="button-row compact">
                <Link className="panel-link-button" to="/mcu/primary">
                  Open Dedicated Motor Page
                </Link>
              </div>
            ) : null}
          </article>
        </div>

        <div className="panel-heading motor-stats-heading">
          <div>
            <h3>Electrical Snapshot</h3>
            <p className="panel-copy">Fast-glance values for the motor, DC bus, and control state.</p>
          </div>
          <span className={telemetryChip.className}>{telemetryChip.text}</span>
        </div>

        <div className="metric-grid motor-metric-grid">
          <div className="metric-card">
            <span>Host State</span>
            <strong>{hostState}</strong>
          </div>
          <div className="metric-card">
            <span>MCU State</span>
            <strong>{mcuState}</strong>
          </div>
          <div className="metric-card">
            <span>Rotor Theta</span>
            <strong>{formatSigned(status.pos_mech_theta, 4)}</strong>
          </div>
          <div className="metric-card">
            <span>DC Bus</span>
            <strong>{formatFixed(status.vdc_bus, 1)}</strong>
          </div>
          <div className="metric-card">
            <span>Phase A</span>
            <strong>{formatSigned(status.current_as, 3)}</strong>
          </div>
          <div className="metric-card">
            <span>Phase B</span>
            <strong>{formatSigned(status.current_bs, 3)}</strong>
          </div>
          <div className="metric-card">
            <span>Phase C</span>
            <strong>{formatSigned(status.current_cs, 3)}</strong>
          </div>
          <div className="metric-card">
            <span>Temperature</span>
            <strong>N/A yet</strong>
          </div>
        </div>
      </section>
    </div>
  );
}
