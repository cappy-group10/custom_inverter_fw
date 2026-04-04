import { Link, useNavigate } from "react-router-dom";

import { useDashboard } from "../context/DashboardContext";
import { formatTimestamp } from "../lib/selectors";

export function LandingPage() {
  const navigate = useNavigate();
  const { primaryMcu, snapshot } = useDashboard();

  return (
    <div className="page-shell">
      <header className="hero landing-hero">
        <div>
          <p className="eyebrow">Emission Impossible</p>
          <h1>Inverter OS</h1>
          <p className="hero-copy">
            A polished controller-to-MCU operator experience for expo demonstrations. Start from one entry point, create
            a connection instance, and continue into the live dashboard for controller validation, UART diagnostics, and
            motor telemetry.
          </p>
        </div>
        <div className="hero-status">
          <span className="status-chip muted">Drive Mode</span>
          <span className={`status-chip ${snapshot.session_state === "running" ? "good" : "muted"}`}>
            {snapshot.session_state === "running" ? "Session available" : "Ready to launch"}
          </span>
        </div>
      </header>

      <section className="health-banner neutral">
        <strong>Start here</strong>
        <span>Create the connection instance first, then continue into the operator dashboard.</span>
      </section>

      <main className="landing-workspace">
        <section className="panel landing-panel landing-panel-primary">
          <div className="panel-heading">
            <div>
              <h2>Connection Instance</h2>
              <p className="panel-copy">
                This opens the live operator workspace where you configure the controller, choose the UART port, and
                supervise the drive session.
              </p>
            </div>
            <span className="badge muted">Step 1</span>
          </div>

          <button className="primary landing-cta-button" onClick={() => navigate("/configure")}>
            Create connection instance
          </button>

          <div className="landing-note-grid">
            <article className="landing-note-card">
              <span>Controller</span>
              <strong>{snapshot.joystick_name || "Awaiting connection"}</strong>
            </article>
            <article className="landing-note-card">
              <span>Port</span>
              <strong>{snapshot.port || "demo"}</strong>
            </article>
            <article className="landing-note-card">
              <span>Last frame</span>
              <strong>{formatTimestamp(snapshot.health.last_frame_at)}</strong>
            </article>
          </div>

          {primaryMcu ? (
            <div className="landing-resume-row">
              <span className="badge good">Active session detected</span>
              <div className="landing-link-row">
                <Link className="panel-link-button" to="/configure">
                  Resume dashboard
                </Link>
                <Link className="panel-link-button" to={primaryMcu.detail_path}>
                  Open motor page
                </Link>
              </div>
            </div>
          ) : null}
        </section>

        <section className="panel landing-panel">
          <div className="panel-heading">
            <div>
              <h2>Demo Flow</h2>
              <p className="panel-copy">A simple operator sequence that feels clean and intentional during your expo walkthrough.</p>
            </div>
            <span className="badge muted">Step 2+</span>
          </div>

          <div className="landing-stage-grid">
            <article className="landing-stage-card">
              <span className="landing-stage-index">01</span>
              <h3>Create the connection</h3>
              <p>Enter the operator dashboard and select the controller and UART configuration for the session.</p>
            </article>
            <article className="landing-stage-card">
              <span className="landing-stage-index">02</span>
              <h3>Validate the link</h3>
              <p>Use the controller, transport health, and UART Debug tabs to confirm the host-to-MCU path is operating correctly.</p>
            </article>
            <article className="landing-stage-card">
              <span className="landing-stage-index">03</span>
              <h3>Show the motor view</h3>
              <p>Open the dedicated motor page to present speed, electrical telemetry, and the emergency brake override.</p>
            </article>
          </div>
        </section>
      </main>
    </div>
  );
}
