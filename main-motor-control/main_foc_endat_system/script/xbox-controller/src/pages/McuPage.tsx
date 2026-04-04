import { useEffect } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";

import { MotorControlPanel } from "../components/MotorControlPanel";
import { useDashboard } from "../context/DashboardContext";
import { frontendLogger } from "../lib/frontendLogger";
import { getConnectionInstance, getMostRecentlyOpenedInstance, markConnectionInstanceOpened, updateConnectionInstanceFromSnapshot } from "../lib/instances";
import { formatTimestamp } from "../lib/selectors";

function getHealthCopy(snapshot: ReturnType<typeof useDashboard>["snapshot"]) {
  const health = snapshot.health || {};
  if (snapshot.session_state === "running") {
    if (health.terminal_only) {
      return {
        title: "Drive runtime active",
        message: "Controller and host TX are running in demo mode. No MCU telemetry is expected.",
        level: "good",
      };
    }
    if (health.telemetry_stale) {
      return {
        title: "MCU telemetry is stale",
        message: "Frames are no longer arriving from the MCU. Check the UART path before relying on this operator page.",
        level: "danger",
      };
    }
    if (!health.has_mcu_telemetry) {
      return {
        title: "Awaiting MCU telemetry",
        message: "Host commands are running, but status feedback has not started yet.",
        level: "warn",
      };
    }
    return {
      title: "Motor operator page live",
      message: "The dedicated motor page is tracking the current session and brake override state.",
      level: "good",
    };
  }

  if (snapshot.session_state === "error" || health.last_error) {
    return {
      title: "Runtime error",
      message: snapshot.last_error || health.last_error || "Unknown runtime error",
      level: "danger",
    };
  }

  return {
    title: "No active motor session",
    message: "Start the drive runtime from the dashboard before using the dedicated motor operator page.",
    level: "neutral",
  };
}

export function McuPage() {
  const { mcuId } = useParams();
  const [searchParams] = useSearchParams();
  const { snapshot, loading, engageBrake } = useDashboard();
  const banner = getHealthCopy(snapshot);
  const instanceId = searchParams.get("instance");
  const activeInstance = instanceId ? getConnectionInstance(instanceId) : getMostRecentlyOpenedInstance();
  const dashboardPath = activeInstance ? `/configure?instance=${encodeURIComponent(activeInstance.id)}` : "/configure";

  useEffect(() => {
    if (instanceId) {
      markConnectionInstanceOpened(instanceId);
    }
  }, [instanceId]);

  useEffect(() => {
    frontendLogger.info("motor_page", "Motor page viewed", {
      mcu_id: mcuId || "unknown",
      instance_id: activeInstance?.id || null,
    });
  }, [activeInstance?.id, mcuId]);

  useEffect(() => {
    updateConnectionInstanceFromSnapshot(activeInstance?.id, snapshot);
  }, [activeInstance?.id, snapshot]);

  if (mcuId !== "primary") {
    return (
      <div className="page-shell">
        <section className="panel empty-panel">
          <h2>Unknown MCU route</h2>
          <p className="panel-copy">The current backend only exposes one live MCU page at `/mcu/primary`.</p>
          <div className="button-row">
            <Link className="panel-link-button" to={dashboardPath}>
              Back to Dashboard
            </Link>
          </div>
        </section>
      </div>
    );
  }

  return (
    <div className="page-shell">
      <header className="hero">
        <div>
          <p className="eyebrow">Emission Impossible</p>
          <h1>Inverter OS Motor Control</h1>
          <p className="hero-copy">
            A dedicated operator view for motor speed, electrical feedback, and the emergency brake override.
          </p>
        </div>
        <div className="hero-status">
          <span className={`status-chip ${activeInstance ? "good" : "muted"}`}>{activeInstance?.name || "No instance selected"}</span>
          <span className="status-chip">{snapshot.session_state || "Idle"}</span>
          <span className="status-chip muted">{snapshot.port || "demo"}</span>
          <Link className="status-chip muted page-link-chip" to={dashboardPath}>
            Back to Dashboard
          </Link>
        </div>
      </header>

      <section className={`health-banner ${banner.level}`}>
        <strong>{banner.title}</strong>
        <span>{banner.message}</span>
      </section>

      <main className="workspace">
        <MotorControlPanel
          snapshot={snapshot}
          loadingBrake={loading.brake}
          dedicatedPage
          onBrake={() => {
            void engageBrake().catch(() => undefined);
          }}
        />

        <section className="panel dedicated-meta-panel">
          <div className="panel-heading">
            <div>
              <h2>Session Context</h2>
              <p className="panel-copy">A compact reference strip so the dedicated motor page still shows the session essentials.</p>
            </div>
            <span className="badge muted">Primary MCU</span>
          </div>
          <dl className="meta-list">
            <div>
              <dt>Controller</dt>
              <dd>{snapshot.joystick_name || "Not connected"}</dd>
            </div>
            <div>
              <dt>Started</dt>
              <dd>{formatTimestamp(snapshot.started_at)}</dd>
            </div>
            <div>
              <dt>Last Status</dt>
              <dd>{formatTimestamp(snapshot.health.last_status_at)}</dd>
            </div>
          </dl>
        </section>
      </main>
    </div>
  );
}
