import { formatSigned, getActiveButtons, getControlMappings, getOverrideLabel } from "../lib/selectors";
import type { SessionSnapshot } from "../lib/types";

interface ControllerMonitorProps {
  snapshot: SessionSnapshot;
}

function StickCard({
  label,
  x,
  y,
}: {
  label: string;
  x: number;
  y: number;
}) {
  const clampedX = Math.max(-1, Math.min(1, x));
  const clampedY = Math.max(-1, Math.min(1, y));
  const magnitude = Math.min(1, Math.sqrt(clampedX ** 2 + clampedY ** 2));
  const angle = Math.atan2(clampedY, clampedX) * (180 / Math.PI);

  return (
    <article className="stick-card">
      <div className="stick-card-header">
        <span>{label}</span>
        <strong>
          {formatSigned(x, 2)} / {formatSigned(y, 2)}
        </strong>
      </div>
      <div className="stick-surface-react">
        <div className="stick-crosshair" />
        <div className="stick-center-dot" />
        <div
          className="stick-vector-react"
          style={{
            width: `${magnitude * 42}%`,
            transform: `translateY(-50%) rotate(${angle}deg)`,
          }}
        />
        <div
          className="stick-dot-react"
          style={{
            left: `${50 + clampedX * 34}%`,
            top: `${50 + clampedY * 34}%`,
          }}
        />
      </div>
    </article>
  );
}

export function ControllerMonitor({ snapshot }: ControllerMonitorProps) {
  const controllerState = snapshot.controller_state;
  const activeButtons = getActiveButtons(controllerState);
  const mappedControls = getControlMappings(snapshot.controller_layout);

  return (
    <section className="panel">
      <div className="panel-headline">
        <div>
          <p className="eyebrow">Configure & Test</p>
          <h2>Controller and link validation</h2>
          <p className="panel-copy">Verify the controller surface, mapped actions, and live host command before driving the MCU page.</p>
        </div>
        <span className={`status-chip ${snapshot.controller_connected ? "good" : "muted"}`}>
          {snapshot.controller_connected ? "Controller connected" : "Controller disconnected"}
        </span>
      </div>

      <div className="controller-grid-react">
        <StickCard label="Left Stick" x={Number(controllerState?.left_x || 0)} y={Number(controllerState?.left_y || 0)} />
        <StickCard label="Right Stick" x={Number(controllerState?.right_x || 0)} y={Number(controllerState?.right_y || 0)} />
      </div>

      <div className="trigger-row-react">
        <article className="trigger-card-react">
          <div className="stick-card-header">
            <span>LT</span>
            <strong>{formatSigned(controllerState?.left_trigger, 2)}</strong>
          </div>
          <div className="progress-track">
            <div className="progress-fill" style={{ width: `${((Number(controllerState?.left_trigger || 0) + 1) / 2) * 100}%` }} />
          </div>
        </article>
        <article className="trigger-card-react">
          <div className="stick-card-header">
            <span>RT</span>
            <strong>{formatSigned(controllerState?.right_trigger, 2)}</strong>
          </div>
          <div className="progress-track">
            <div className="progress-fill" style={{ width: `${((Number(controllerState?.right_trigger || 0) + 1) / 2) * 100}%` }} />
          </div>
        </article>
      </div>

      <div className="stat-ribbon">
        <article className="mini-stat">
          <span>Host Command</span>
          <strong>{String(snapshot.last_host_command?.ctrl_state || "STOP")}</strong>
        </article>
        <article className="mini-stat">
          <span>Override</span>
          <strong>{getOverrideLabel(snapshot.active_override)}</strong>
        </article>
        <article className="mini-stat">
          <span>Speed Ref</span>
          <strong>{formatSigned(snapshot.last_host_command?.speed_ref, 4)}</strong>
        </article>
        <article className="mini-stat">
          <span>DC Bus</span>
          <strong>{formatSigned(snapshot.latest_mcu_status?.vdc_bus, 1)} V</strong>
        </article>
      </div>

      <div className="mapping-layout">
        <article className="mapping-card">
          <h3>Active Buttons</h3>
          <div className="chip-wrap">
            {activeButtons.length ? activeButtons.map((button) => <span key={button} className="button-chip active">{button}</span>) : <span className="button-chip muted">No active buttons</span>}
          </div>
        </article>
        <article className="mapping-card">
          <h3>Mapped Inputs</h3>
          <div className="mapping-list">
            {mappedControls.slice(0, 8).map((mapping) => (
              <span key={mapping} className="mapping-item">
                {mapping}
              </span>
            ))}
          </div>
        </article>
      </div>
    </section>
  );
}
