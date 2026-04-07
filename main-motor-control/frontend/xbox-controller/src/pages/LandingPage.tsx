import { useEffect, useMemo, useState } from "react";
import { Link, useNavigate } from "react-router-dom";

import { InfoHint, UiIcon } from "../components/UiChrome";
import { useDashboard } from "../context/DashboardContext";
import { frontendLogger } from "../lib/frontendLogger";
import {
  createConnectionInstance,
  deleteConnectionInstance,
  loadConnectionInstances,
  markConnectionInstanceOpened,
  renameConnectionInstance,
} from "../lib/instances";
import { formatTimestamp } from "../lib/selectors";
import type { ConnectionInstance } from "../lib/types";

function buildInstanceConfigurePath(instanceId: string) {
  return `/configure?instance=${encodeURIComponent(instanceId)}`;
}

function buildInstanceMotorPath(instanceId: string) {
  return `/mcu/primary?instance=${encodeURIComponent(instanceId)}`;
}

function buildInstanceMusicPath(instanceId: string) {
  return `/configure?instance=${encodeURIComponent(instanceId)}#music`;
}

export function LandingPage() {
  const navigate = useNavigate();
  const { primaryMcu, snapshot } = useDashboard();
  const [instances, setInstances] = useState<ConnectionInstance[]>([]);
  const [newInstanceName, setNewInstanceName] = useState("");
  const [editingInstanceId, setEditingInstanceId] = useState<string | null>(null);
  const [editingName, setEditingName] = useState("");

  useEffect(() => {
    setInstances(loadConnectionInstances());
  }, []);

  const hasInstances = instances.length > 0;
  const latestInstance = useMemo(() => instances[0] || null, [instances]);

  function refreshInstances() {
    setInstances(loadConnectionInstances());
  }

  function openInstance(instanceId: string) {
    const updated = markConnectionInstanceOpened(instanceId);
    refreshInstances();
    frontendLogger.info("instances", "Connection instance opened", {
      instance_id: instanceId,
      instance_name: updated?.name || null,
    });
    navigate(buildInstanceConfigurePath(instanceId));
  }

  function createInstanceAndOpen() {
    const instance = createConnectionInstance(newInstanceName);
    refreshInstances();
    frontendLogger.info("instances", "Connection instance created", {
      instance_id: instance.id,
      instance_name: instance.name,
    });
    setNewInstanceName("");
    navigate(buildInstanceConfigurePath(instance.id));
  }

  function removeInstance(instanceId: string) {
    const instance = instances.find((item) => item.id === instanceId);
    deleteConnectionInstance(instanceId);
    refreshInstances();
    frontendLogger.warn("instances", "Connection instance deleted", {
      instance_id: instanceId,
      instance_name: instance?.name || null,
    });
  }

  function startRename(instance: ConnectionInstance) {
    setEditingInstanceId(instance.id);
    setEditingName(instance.name);
  }

  function saveRename(instanceId: string) {
    const updated = renameConnectionInstance(instanceId, editingName);
    refreshInstances();
    setEditingInstanceId(null);
    setEditingName("");
    frontendLogger.info("instances", "Connection instance renamed", {
      instance_id: instanceId,
      instance_name: updated?.name || null,
    });
  }

  return (
    <div className="page-shell">
      <header className="hero landing-hero">
        <div>
          <p className="eyebrow">Emission Impossible</p>
          <h1>Inverter OS</h1>
          <p className="hero-copy">
            A professional operator console for controller validation, UART diagnostics, and motor telemetry. Create a
            connection instance below to enter the live dashboard experience.
          </p>
        </div>
        <div className="hero-status">
          <span className="status-chip muted">
            <UiIcon name="controller-pad" />
            {snapshot.mode === "music" ? "Music Mode" : "Drive Mode"}
          </span>
          <span className={`status-chip ${hasInstances ? "good" : "muted"}`}>
            <UiIcon name="instances" />
            {hasInstances ? `${instances.length} instance${instances.length === 1 ? "" : "s"} ready` : "Ready to launch"}
          </span>
        </div>
      </header>

      <section className="health-banner neutral">
        <strong>Connection instance manager</strong>
        <span>Create, reopen, or remove demo instances before entering the operator dashboard.</span>
      </section>

      <main className="landing-workspace">
        <section className="panel landing-panel landing-panel-primary">
          <div className="panel-heading">
            <div>
              <div className="heading-line">
                <UiIcon name="instances" className="heading-icon" />
                <h2>Connection Instances</h2>
                <InfoHint text="Create a polished demo launcher entry before opening the live dashboard. These instances are UI-only and do not create separate backend runtimes." />
              </div>
              <p className="panel-copy">
                Each instance is a demo-only launcher record stored in the browser. Use it to keep the expo flow tidy
                without creating multiple real MCU runtimes.
              </p>
            </div>
            <span className="badge muted">
              <UiIcon name="session" />
              Operator Entry
            </span>
          </div>

          <div className="instance-create-form">
            <label className="control-field control-field-wide">
              Instance Name
              <input
                type="text"
                value={newInstanceName}
                onChange={(event) => setNewInstanceName(event.target.value)}
                placeholder="Connection Instance 03"
              />
            </label>
            <button className="primary landing-cta-button" onClick={createInstanceAndOpen}>
              <UiIcon name="create" />
              <span>Create connection instance</span>
            </button>
          </div>

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

          {primaryMcu && latestInstance ? (
            <div className="landing-resume-row">
              <span className="badge good">
                <UiIcon name="shield" />
                Active session detected
              </span>
              <div className="landing-link-row">
                <Link className="panel-link-button" to={buildInstanceConfigurePath(latestInstance.id)}>
                  <UiIcon name="open" />
                  Resume dashboard
                </Link>
                <Link
                  className="panel-link-button"
                  to={primaryMcu?.mode === "music" ? buildInstanceMusicPath(latestInstance.id) : buildInstanceMotorPath(latestInstance.id)}
                >
                  <UiIcon name="motor" />
                  {primaryMcu?.mode === "music" ? "Open music tab" : "Open motor page"}
                </Link>
              </div>
            </div>
          ) : null}
        </section>

        <section className="panel landing-panel">
          <div className="panel-heading">
            <div>
              <div className="heading-line">
                <UiIcon name="overview" className="heading-icon" />
                <h2>Saved Instances</h2>
                <InfoHint text="Open an existing demo instance, rename it for the next presenter, or delete it to keep the launcher clean." />
              </div>
              <p className="panel-copy">
                Reopen a prepared demo context, or delete it if you want a cleaner launcher view for the next audience.
              </p>
            </div>
            <span className={`badge ${hasInstances ? "good" : "muted"}`}>{hasInstances ? `${instances.length} saved` : "No instances yet"}</span>
          </div>

          <div className="instance-list">
            {hasInstances ? (
              instances.map((instance) => (
                <article key={instance.id} className="instance-card">
                  <div className="instance-card-header">
                    <div>
                      {editingInstanceId === instance.id ? (
                        <div className="instance-rename-row">
                          <input value={editingName} onChange={(event) => setEditingName(event.target.value)} aria-label="Rename connection instance" />
                          <button className="icon-button primary" onClick={() => saveRename(instance.id)} aria-label="Save instance name">
                            <UiIcon name="save" />
                          </button>
                          <button
                            className="icon-button"
                            onClick={() => {
                              setEditingInstanceId(null);
                              setEditingName("");
                            }}
                            aria-label="Cancel rename"
                          >
                            <UiIcon name="cancel" />
                          </button>
                        </div>
                      ) : (
                        <div className="instance-title-row">
                          <UiIcon name="instances" className="heading-icon" />
                          <h3>{instance.name}</h3>
                          <button className="icon-button" onClick={() => startRename(instance)} aria-label={`Rename ${instance.name}`}>
                            <UiIcon name="edit" />
                          </button>
                        </div>
                      )}
                      <p className="panel-copy">Created {formatTimestamp(instance.created_at)} · Last opened {formatTimestamp(instance.last_opened_at)}</p>
                    </div>
                    <span className={`badge ${instance.session_state === "running" ? "good" : "muted"}`}>
                      {instance.session_state || "idle"}
                    </span>
                  </div>

                  <div className="instance-card-meta">
                    <article className="landing-note-card">
                      <span>Port</span>
                      <strong>{instance.port || "demo"}</strong>
                    </article>
                    <article className="landing-note-card">
                      <span>Controller</span>
                      <strong>{instance.controller_name || "Not recorded"}</strong>
                    </article>
                    <article className="landing-note-card">
                      <span>Last frame</span>
                      <strong>{formatTimestamp(instance.last_frame_at)}</strong>
                    </article>
                  </div>

                  <div className="instance-action-row">
                    <button className="primary" onClick={() => openInstance(instance.id)}>
                      <UiIcon name="open" />
                      Open instance
                    </button>
                    <button onClick={() => removeInstance(instance.id)}>
                      <UiIcon name="delete" />
                      Delete
                    </button>
                  </div>
                </article>
              ))
            ) : (
              <div className="placeholder large-placeholder">
                No connection instances have been created yet. Create one to open the operator dashboard.
              </div>
            )}
          </div>

          <div className="landing-stage-grid">
            <article className="landing-stage-card">
              <span className="landing-stage-index">01</span>
              <h3>Create or open an instance</h3>
              <p>Use a saved demo entry to keep the launch flow consistent across repeated expo demonstrations.</p>
            </article>
            <article className="landing-stage-card">
              <span className="landing-stage-index">02</span>
              <h3>Configure controller and UART</h3>
              <p>Validate the controller path, choose the serial target, and confirm the transport health before driving.</p>
            </article>
            <article className="landing-stage-card">
              <span className="landing-stage-index">03</span>
              <h3>Present the motor page</h3>
              <p>Switch to the dedicated motor view for speed, electrical telemetry, and the emergency brake override.</p>
            </article>
          </div>
        </section>
      </main>
    </div>
  );
}
