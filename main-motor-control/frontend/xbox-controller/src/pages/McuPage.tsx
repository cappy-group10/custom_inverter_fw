import { useEffect } from "react";
import { Link, useParams, useSearchParams } from "react-router-dom";

import { MotorControlPanel } from "../components/MotorControlPanel";
import { UiIcon } from "../components/UiChrome";
import { useDashboard } from "../context/DashboardContext";
import { frontendLogger } from "../lib/frontendLogger";
import { getConnectionInstance, getMostRecentlyOpenedInstance, markConnectionInstanceOpened, updateConnectionInstanceFromSnapshot } from "../lib/instances";
import { formatTimestamp } from "../lib/selectors";

function getHealthCopy(snapshot: ReturnType<typeof useDashboard>["snapshot"]) {
  const health = snapshot.health || {};
  if (snapshot.mode === "music") {
    return {
      title: "Music session active",
      message: "The dedicated motor page remains drive-only. Use the dashboard Music tab for song playback and compact music status.",
      level: "neutral",
    };
  }
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

function getHealthBadgeTone(level: string) {
  if (level === "good" || level === "warn" || level === "danger") {
    return level;
  }
  return "muted";
}

export function McuPage() {
  const { mcuId } = useParams();
  const [searchParams] = useSearchParams();
  const { snapshot, loading, engageBrake, releaseBrake } = useDashboard();
  const banner = getHealthCopy(snapshot);
  const instanceId = searchParams.get("instance");
  const activeInstance = instanceId ? getConnectionInstance(instanceId) : getMostRecentlyOpenedInstance();
  const dashboardPath = activeInstance ? `/configure?instance=${encodeURIComponent(activeInstance.id)}` : "/configure";
  const startedAtLabel = formatTimestamp(snapshot.started_at);
  const lastStatusLabel = formatTimestamp(snapshot.health.last_status_at);
  const healthTone = getHealthBadgeTone(banner.level);

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
    updateConnectionInstanceFromSnapshot(activeInstance?.id, {
      session_state: snapshot.session_state,
      port: snapshot.port,
      joystick_name: snapshot.joystick_name,
      health: {
        last_frame_at: snapshot.health.last_frame_at ?? null,
      },
    });
  }, [
    activeInstance?.id,
    snapshot.session_state,
    snapshot.port,
    snapshot.joystick_name,
    snapshot.health.last_frame_at,
  ]);

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

  if (snapshot.mode === "music") {
    return (
      <div className="page-shell">
        <section className="panel empty-panel">
          <h2>Drive-only page</h2>
          <p className="panel-copy">The active session is in music mode. Use the dashboard Music tab for playback, volume, and UART status.</p>
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
    <div className="page-shell motor-page-shell">
      <header className="panel motor-page-masthead">
        <div className="motor-page-title-row">
          <div className="motor-page-copy">
            <p className="eyebrow">Emission Impossible</p>
            <h1>Inverter OS Motor Control</h1>
            <p className="hero-copy">
              Compact operator view for speed tracking, d/q currents, fault state, and brake control without leaving the primary motor surface.
            </p>
            <p className="motor-page-health-note">
              <strong>{banner.title}</strong>
              <span>{banner.message}</span>
            </p>
          </div>

          <div className="motor-page-chip-grid">
            <Link className="status-chip muted page-link-chip" to={dashboardPath}>
              <UiIcon name="back" />
              Back to Dashboard
            </Link>
            <span className={`status-chip ${activeInstance ? "good" : "muted"}`}>
              <UiIcon name="instances" />
              {activeInstance?.name || "No instance selected"}
            </span>
            <span className="status-chip">
              <UiIcon name="session" />
              {snapshot.session_state || "Idle"}
            </span>
            <span className="status-chip muted">
              <UiIcon name="port" />
              {snapshot.port || "demo"}
            </span>
            <span className={`badge motor-inline-health ${healthTone}`}>
              <UiIcon name="shield" />
              {banner.title}
            </span>
            <span className="status-chip muted">
              <UiIcon name="controller-pad" />
              {snapshot.joystick_name || "Controller offline"}
            </span>
            <span className="status-chip muted">
              <UiIcon name="clock" />
              {`Started ${startedAtLabel}`}
            </span>
            <span className="status-chip muted">
              <UiIcon name="telemetry" />
              {`Last status ${lastStatusLabel}`}
            </span>
          </div>
        </div>
      </header>

      <main className="workspace motor-page-workspace">
        <MotorControlPanel
          snapshot={snapshot}
          loadingBrake={loading.brake}
          dedicatedPage
          onBrake={() => {
            void engageBrake().catch(() => undefined);
          }}
          onBrakeRelease={() => {
            void releaseBrake().catch(() => undefined);
          }}
        />
      </main>
    </div>
  );
}
