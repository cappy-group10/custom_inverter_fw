import { Link } from "react-router-dom";

import { InfoHint, UiIcon } from "./UiChrome";
import { formatFixed, formatSigned, formatTimestamp, formatTripFlag, normalizeCtrlState } from "../lib/selectors";
import type { SessionSnapshot, TelemetrySample } from "../lib/types";

interface MotorControlPanelProps {
  snapshot: SessionSnapshot;
  loadingBrake: boolean;
  onBrake: () => void;
  onBrakeRelease: () => void;
  dedicatedPage?: boolean;
  detailPath?: string;
}

function clamp(value: number, min: number, max: number) {
  return Math.max(min, Math.min(max, value));
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

function createSparklinePath(values: number[], width: number, height: number, padding: number, min: number, range: number) {
  return values
    .map((value, index) => {
      const x = padding + (index / Math.max(values.length - 1, 1)) * (width - padding * 2);
      const y = height - padding - ((value - min) / range) * (height - padding * 2);
      return `${index === 0 ? "M" : "L"}${x.toFixed(1)} ${y.toFixed(1)}`;
    })
    .join(" ");
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

function getDirectionMeta(valuePu: number) {
  if (valuePu > 1e-4) {
    return { label: "FWD", className: "is-forward" };
  }
  if (valuePu < -1e-4) {
    return { label: "REV", className: "is-reverse" };
  }
  return { label: "ZERO", className: "is-zero" };
}

function getVoltageTone(vdcBus: number, minV: number, maxV: number) {
  const span = Math.max(maxV - minV, 0);
  const margin = span * 0.1;
  if (vdcBus < minV || vdcBus > maxV) {
    return { label: "Out of range", className: "danger" };
  }
  if (span > 0 && (vdcBus <= minV + margin || vdcBus >= maxV - margin)) {
    return { label: "Near limit", className: "warn" };
  }
  return { label: "Nominal", className: "good" };
}

function formatRpm(valuePu: number, baseSpeedRpm: number) {
  return Math.round(valuePu * baseSpeedRpm);
}

function formatTemperature(value: number | null | undefined) {
  if (value == null) {
    return "Pending source";
  }
  return `${value.toFixed(1)} C`;
}

function createEmptyTelemetrySample(): TelemetrySample {
  return {
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
  };
}

function TrendChart({
  samples,
  title,
  refKey,
  fbkKey,
  compact = false,
}: {
  samples: TelemetrySample[];
  title: string;
  refKey: keyof TelemetrySample;
  fbkKey: keyof TelemetrySample;
  compact?: boolean;
}) {
  const safeSamples = samples.length ? samples : [createEmptyTelemetrySample()];
  const values = safeSamples.flatMap((sample) => [Number(sample[refKey] || 0), Number(sample[fbkKey] || 0)]);
  const width = 320;
  const height = compact ? 104 : 140;
  const padding = compact ? 10 : 14;
  const min = Math.min(...values, 0);
  const max = Math.max(...values, 0);
  const range = max - min || 1;
  const baselineY = height - padding - ((0 - min) / range) * (height - padding * 2);
  const latest = safeSamples[safeSamples.length - 1] || createEmptyTelemetrySample();

  return (
    <article className={`motor-trend-card${compact ? " compact" : ""}`}>
      <div className="motor-card-head">
        <div>
          <span className="motor-card-kicker">{title}</span>
          <strong>{title} Ref vs Feedback</strong>
        </div>
        <div className="motor-trend-legend">
          <span className="legend-chip ref">Reference</span>
          <span className="legend-chip fbk">Feedback</span>
        </div>
      </div>
      <svg
        viewBox={`0 0 ${width} ${height}`}
        preserveAspectRatio="none"
        className="motor-trend-svg"
        role="img"
        aria-label={`${title} trend`}
      >
        <rect x="1" y="1" width={width - 2} height={height - 2} rx="18" ry="18" className="motor-trend-shell" />
        <line x1={padding} y1={baselineY} x2={width - padding} y2={baselineY} className="motor-trend-baseline" />
        <path
          d={createSparklinePath(
            safeSamples.map((sample) => Number(sample[refKey] || 0)),
            width,
            height,
            padding,
            min,
            range,
          )}
          className="motor-trend-line ref"
        />
        <path
          d={createSparklinePath(
            safeSamples.map((sample) => Number(sample[fbkKey] || 0)),
            width,
            height,
            padding,
            min,
            range,
          )}
          className="motor-trend-line fbk"
        />
      </svg>
      <div className={`motor-trend-values${compact ? " compact" : ""}`}>
        <span>{`Ref ${formatSigned(Number(latest[refKey] || 0), 3)} pu`}</span>
        <span>{`Fbk ${formatSigned(Number(latest[fbkKey] || 0), 3)} pu`}</span>
      </div>
    </article>
  );
}

function MotorSpeedometer({
  speedRefPu,
  speedFbkPu,
  baseSpeedRpm,
  hostState,
  mcuState,
  brakeLatched,
}: {
  speedRefPu: number;
  speedFbkPu: number;
  baseSpeedRpm: number;
  hostState: string;
  mcuState: string;
  brakeLatched: boolean;
}) {
  const speedRefMeta = getDirectionMeta(speedRefPu);
  const speedFbkMeta = getDirectionMeta(speedFbkPu);
  const commandAngle = -120 + ((clamp(speedRefPu, -1, 1) + 1) / 2) * 240;
  const feedbackAngle = -120 + ((clamp(speedFbkPu, -1, 1) + 1) / 2) * 240;
  const commandNeedle = polarToCartesian(170, 168, 102, commandAngle);
  const feedbackNeedle = polarToCartesian(170, 168, 82, feedbackAngle);
  const track = describeArc(170, 168, 110, -120, 120);

  return (
    <article className="speedometer-card" data-testid="speedometer">
      <div className="speedometer-copy">
        <span className="speedometer-label">Dual Speedometer</span>
        <strong>{`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}</strong>
        <small>{`Feedback ${formatSigned(speedFbkPu, 3)} pu`}</small>
      </div>
      <div className="speedometer-status-strip">
        <span className={`direction-chip ${speedRefMeta.className}`}>{`Command ${speedRefMeta.label}`}</span>
        <span className={`direction-chip ${speedFbkMeta.className}`}>{`Feedback ${speedFbkMeta.label}`}</span>
      </div>
      <svg id="motor-speedometer" viewBox="0 0 340 248" preserveAspectRatio="xMidYMid meet" role="img" aria-label="Command and motor speed gauge">
        <path d={track} className="speedometer-track" />
        <line x1="170" y1="168" x2={commandNeedle.x} y2={commandNeedle.y} className="speedometer-needle command" />
        <line x1="170" y1="168" x2={feedbackNeedle.x} y2={feedbackNeedle.y} className="speedometer-needle feedback" />
        <circle cx="170" cy="168" r="10" className="speedometer-hub" />
        <text x="54" y="190" textAnchor="middle" className="speedometer-tick">
          {`${Math.round(-baseSpeedRpm)}`}
        </text>
        <text x="170" y="44" textAnchor="middle" className="speedometer-tick">
          0
        </text>
        <text x="286" y="190" textAnchor="middle" className="speedometer-tick">
          {`${Math.round(baseSpeedRpm)}`}
        </text>
        <text x="170" y="106" textAnchor="middle" className="speedometer-value">
          {`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}
        </text>
        <text x="170" y="128" textAnchor="middle" className="speedometer-unit">
          {`Command ${formatSigned(speedRefPu, 3)} pu | Feedback ${formatSigned(speedFbkPu, 3)} pu`}
        </text>
        <text x="78" y="84" textAnchor="middle" className={`speedometer-legend ${speedRefMeta.className}`}>
          CMD
        </text>
        <text x="262" y="84" textAnchor="middle" className={`speedometer-legend ${speedFbkMeta.className}`}>
          FBK
        </text>
        <text x="170" y="214" textAnchor="middle" className="speedometer-subcopy">
          {`Host ${hostState} | MCU ${mcuState}`}
        </text>
        {brakeLatched ? (
          <text x="170" y="66" textAnchor="middle" className="speedometer-override">
            BRAKE LATCHED
          </text>
        ) : null}
      </svg>
      <div className="speedometer-value-row">
        <div className={`speedometer-chip ${speedRefMeta.className}`}>
          <span>Command</span>
          <strong>{`${formatRpm(speedRefPu, baseSpeedRpm)} RPM`}</strong>
          <small>{`${formatSigned(speedRefPu, 3)} pu`}</small>
        </div>
        <div className={`speedometer-chip ${speedFbkMeta.className}`}>
          <span>Feedback</span>
          <strong>{`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}</strong>
          <small>{`${formatSigned(speedFbkPu, 3)} pu`}</small>
        </div>
      </div>
    </article>
  );
}

function CompactSpeedBand({
  speedRefPu,
  speedFbkPu,
  baseSpeedRpm,
  hostState,
  mcuState,
  brakeLatched,
}: {
  speedRefPu: number;
  speedFbkPu: number;
  baseSpeedRpm: number;
  hostState: string;
  mcuState: string;
  brakeLatched: boolean;
}) {
  const speedRefMeta = getDirectionMeta(speedRefPu);
  const speedFbkMeta = getDirectionMeta(speedFbkPu);
  const commandPct = clamp(50 + speedRefPu * 50, 0, 100);
  const feedbackPct = clamp(50 + speedFbkPu * 50, 0, 100);

  return (
    <article className="motor-speed-band-card" data-testid="compact-speed-band">
      <div className="motor-card-head">
        <div>
          <span className="motor-card-kicker">Speed Command vs Feedback</span>
          <strong>{`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}</strong>
        </div>
        <div className="motor-speed-band-badges">
          <span className={`direction-chip ${speedRefMeta.className}`}>{`CMD ${speedRefMeta.label}`}</span>
          <span className={`direction-chip ${speedFbkMeta.className}`}>{`FBK ${speedFbkMeta.label}`}</span>
          {brakeLatched ? <span className="badge danger">Brake latched</span> : null}
        </div>
      </div>

      <div className="motor-speed-band-summary">
        <div className={`motor-speed-band-metric ${speedRefMeta.className}`}>
          <span>Command</span>
          <strong>{`${formatRpm(speedRefPu, baseSpeedRpm)} RPM`}</strong>
          <small>{`${formatSigned(speedRefPu, 3)} pu`}</small>
        </div>
        <div className="motor-speed-band-center">
          <span>Live Motor Speed</span>
          <strong>{`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}</strong>
          <small>{`Host ${hostState} | MCU ${mcuState}`}</small>
        </div>
        <div className={`motor-speed-band-metric ${speedFbkMeta.className}`}>
          <span>Feedback</span>
          <strong>{`${formatRpm(speedFbkPu, baseSpeedRpm)} RPM`}</strong>
          <small>{`${formatSigned(speedFbkPu, 3)} pu`}</small>
        </div>
      </div>

      <div className="motor-speed-band-scale">
        <span>{`${Math.round(-baseSpeedRpm)}`}</span>
        <span>0</span>
        <span>{`${Math.round(baseSpeedRpm)}`}</span>
      </div>

      <div className="motor-speed-band-track">
        <div className="motor-speed-band-zero" />
        <div className={`motor-speed-band-marker command ${speedRefMeta.className}`} style={{ left: `${commandPct}%` }}>
          <span>CMD</span>
        </div>
        <div className={`motor-speed-band-marker feedback ${speedFbkMeta.className}`} style={{ left: `${feedbackPct}%` }}>
          <span>FBK</span>
        </div>
      </div>
    </article>
  );
}

function SharedMotorPanel({
  snapshot,
  detailPath,
  telemetryChip,
  brakeLatched,
  canBrake,
  canReleaseBrake,
  loadingBrake,
  onBrake,
  onBrakeRelease,
  hostState,
  mcuState,
  speedRefPu,
  speedFbkPu,
  idRefPu,
  idFbkPu,
  iqRefPu,
  iqFbkPu,
  inputCurrentA,
  inputPowerW,
  powerPercent,
  powerFillPercent,
  voltageTone,
  latestFault,
  health,
  status,
  motorConfig,
}: {
  snapshot: SessionSnapshot;
  detailPath: string;
  telemetryChip: { text: string; className: string };
  brakeLatched: boolean;
  canBrake: boolean;
  canReleaseBrake: boolean;
  loadingBrake: boolean;
  onBrake: () => void;
  onBrakeRelease: () => void;
  hostState: string;
  mcuState: string;
  speedRefPu: number;
  speedFbkPu: number;
  idRefPu: number;
  idFbkPu: number;
  iqRefPu: number;
  iqFbkPu: number;
  inputCurrentA: number;
  inputPowerW: number;
  powerPercent: number;
  powerFillPercent: number;
  voltageTone: { label: string; className: string };
  latestFault: SessionSnapshot["recent_faults"][number] | null;
  health: SessionSnapshot["health"];
  status: NonNullable<SessionSnapshot["latest_mcu_status"]> | Record<string, never>;
  motorConfig: SessionSnapshot["motor_config"];
}) {
  const phaseMagnitudePu = Math.hypot(idFbkPu, iqFbkPu);

  return (
    <div className="motor-workspace">
      <section className="panel motor-panel">
        <div className="panel-heading">
          <div>
            <div className="heading-line">
              <UiIcon name="motor" className="heading-icon" />
              <h2>Motor Control</h2>
              <InfoHint text="Operator view for transmitted command vs MCU feedback, d/q-axis behavior, safety override state, and electrical loading." />
            </div>
            <p className="panel-copy">
              A drive-focused operator surface for commanded vs actual motion, d/q-axis current tracking, safety state, and electrical load.
            </p>
          </div>
          <span className={`badge ${brakeLatched ? "danger" : "muted"}`}>{brakeLatched ? "BRAKE latched" : "No override"}</span>
        </div>

        <div className="motor-status-grid">
          <article className="motor-status-card">
            <span>Host Control State</span>
            <strong>{hostState}</strong>
            <small>{`Port ${snapshot.port || "demo"}`}</small>
          </article>
          <article className="motor-status-card">
            <span>MCU Control State</span>
            <strong>{mcuState}</strong>
            <small>{`runMotor ${Number(status.run_motor ?? 0)}`}</small>
          </article>
          <article className="motor-status-card">
            <span>Trip Flag</span>
            <strong>{formatTripFlag(status.trip_flag)}</strong>
            <small>{latestFault ? `Fault count ${latestFault.trip_count ?? 0}` : "No recent fault frame"}</small>
          </article>
          <article className="motor-status-card">
            <span>Brake Override</span>
            <strong>{brakeLatched ? "Latched" : "Released"}</strong>
            <small>{`Last frame ${formatTimestamp(health.last_frame_at)}`}</small>
          </article>
        </div>

        <div className="motor-hero-grid">
          <MotorSpeedometer
            speedRefPu={speedRefPu}
            speedFbkPu={speedFbkPu}
            baseSpeedRpm={Number(motorConfig.base_speed_rpm || 0)}
            hostState={hostState}
            mcuState={mcuState}
            brakeLatched={brakeLatched}
          />

          <article className="motor-safety-card">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">Safety</span>
                <strong>Brake Latch & Unlatch</strong>
              </div>
              <span className={telemetryChip.className}>{telemetryChip.text}</span>
            </div>
            <p className="panel-copy">
              Emergency brake latches above the controller mapping. Releasing it sends an immediate STOP with zeroed speed, Id, and Iq references.
            </p>
            <div className="button-row motor-safety-actions">
              <button
                className="primary danger-button"
                disabled={!canBrake || brakeLatched || loadingBrake}
                onClick={onBrake}
              >
                <UiIcon name="shield" />
                {loadingBrake ? "Updating Brake..." : brakeLatched ? "Brake Latched" : "Emergency Brake"}
              </button>
              <button className="panel-link-button" disabled={!canReleaseBrake || loadingBrake} onClick={onBrakeRelease}>
                <UiIcon name="cancel" />
                Unlatch Brake (Send STOP)
              </button>
            </div>
            <p className="control-help">
              {brakeLatched
                ? "BRAKE is latched. Releasing it clears the override and immediately transmits STOP."
                : canBrake
                  ? "Brake controls are available while the runtime is active."
                  : "Start the drive session before using brake controls."}
            </p>
            <dl className="meta-list compact">
              <div>
                <dt>Session</dt>
                <dd>{snapshot.session_state || "idle"}</dd>
              </div>
              <div>
                <dt>Last Status</dt>
                <dd>{formatTimestamp(health.last_status_at)}</dd>
              </div>
              <div>
                <dt>Fault Frames</dt>
                <dd>{String(snapshot.counters.fault_frames ?? 0)}</dd>
              </div>
            </dl>
            <div className="button-row compact">
              <Link className="panel-link-button" to={detailPath}>
                <UiIcon name="open" />
                Open Dedicated Motor Page
              </Link>
            </div>
          </article>
        </div>

        <div className="motor-trend-grid">
          <TrendChart samples={snapshot.telemetry_samples || []} title="Id" refKey="id_ref" fbkKey="id_fbk" />
          <TrendChart samples={snapshot.telemetry_samples || []} title="Iq" refKey="iq_ref" fbkKey="iq_fbk" />
        </div>

        <div className="panel-heading motor-stats-heading">
          <div>
            <div className="heading-line">
              <UiIcon name="telemetry" className="heading-icon" />
              <h3>Electrical Snapshot</h3>
              <InfoHint text="Color-coded DC bus visibility, phase currents, future temperature slots, and an approximate electrical input-power bar from Vdc and d/q current magnitude." />
            </div>
            <p className="panel-copy">Organized live feedback for bus health, phase currents, thermal sources, and estimated electrical loading.</p>
          </div>
          <span className={telemetryChip.className}>{telemetryChip.text}</span>
        </div>

        <div className="motor-electrical-grid">
          <article className={`motor-emphasis-card voltage-card ${voltageTone.className}`}>
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">DC Bus</span>
                <strong>{`${formatFixed(status.vdc_bus, 1)} V`}</strong>
              </div>
              <span className={`badge ${voltageTone.className}`}>{voltageTone.label}</span>
            </div>
            <p className="panel-copy">
              {`Configured band ${formatFixed(motorConfig.vdcbus_min_v, 1)} V to ${formatFixed(motorConfig.vdcbus_max_v, 1)} V`}
            </p>
          </article>

          <article className="motor-data-card">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">d/q Axis</span>
                <strong>Reference vs Feedback</strong>
              </div>
            </div>
            <dl className="motor-pair-list">
              <div>
                <dt>Id Ref</dt>
                <dd>{`${formatSigned(idRefPu, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Id Fbk</dt>
                <dd>{`${formatSigned(idFbkPu, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Iq Ref</dt>
                <dd>{`${formatSigned(iqRefPu, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Iq Fbk</dt>
                <dd>{`${formatSigned(iqFbkPu, 3)} pu`}</dd>
              </div>
            </dl>
          </article>

          <article className="motor-data-card">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">Phase Currents</span>
                <strong>Instantaneous Snapshot</strong>
              </div>
            </div>
            <dl className="motor-pair-list">
              <div>
                <dt>Phase A</dt>
                <dd>{`${formatSigned(status.current_as, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Phase B</dt>
                <dd>{`${formatSigned(status.current_bs, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Phase C</dt>
                <dd>{`${formatSigned(status.current_cs, 3)} pu`}</dd>
              </div>
              <div>
                <dt>Rotor Theta</dt>
                <dd>{`${formatSigned(status.pos_mech_theta, 4)} pu`}</dd>
              </div>
            </dl>
          </article>

          <article className="motor-data-card">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">Temperature</span>
                <strong>Future Thermal Sources</strong>
              </div>
            </div>
            <div className="motor-temperature-grid">
              <div className="temperature-pill">
                <span>Motor Winding</span>
                <strong>{formatTemperature(status.temp_motor_winding_c)}</strong>
              </div>
              <div className="temperature-pill">
                <span>MCU</span>
                <strong>{formatTemperature(status.temp_mcu_c)}</strong>
              </div>
              <div className="temperature-pill">
                <span>IGBTs</span>
                <strong>{formatTemperature(status.temp_igbts_c)}</strong>
              </div>
            </div>
          </article>

          <article className="motor-data-card power-card">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">Input Electrical Power</span>
                <strong>{`${Math.round(inputPowerW)} W`}</strong>
              </div>
              <span className="badge muted">{`${Math.round(powerPercent)}% of base`}</span>
            </div>
            <div className="power-bar-shell" aria-label="Input electrical power">
              <div className="power-bar-fill" style={{ width: `${powerFillPercent}%` }} />
            </div>
            <dl className="motor-pair-list compact">
              <div>
                <dt>|I_dq|</dt>
                <dd>{`${phaseMagnitudePu.toFixed(3)} pu`}</dd>
              </div>
              <div>
                <dt>Base Current</dt>
                <dd>{`${formatFixed(motorConfig.base_current_a, 2)} A`}</dd>
              </div>
              <div>
                <dt>Estimated Input Current</dt>
                <dd>{`${inputCurrentA.toFixed(2)} A`}</dd>
              </div>
              <div>
                <dt>Rated Base Power</dt>
                <dd>{`${Math.round(motorConfig.rated_input_power_w || 0)} W`}</dd>
              </div>
            </dl>
          </article>
        </div>
      </section>
    </div>
  );
}

function DedicatedMotorPanel({
  snapshot,
  telemetryChip,
  brakeLatched,
  canBrake,
  canReleaseBrake,
  loadingBrake,
  onBrake,
  onBrakeRelease,
  hostState,
  mcuState,
  speedRefPu,
  speedFbkPu,
  idRefPu,
  idFbkPu,
  iqRefPu,
  iqFbkPu,
  inputCurrentA,
  inputPowerW,
  powerPercent,
  powerFillPercent,
  voltageTone,
  latestFault,
  health,
  status,
  motorConfig,
}: {
  snapshot: SessionSnapshot;
  telemetryChip: { text: string; className: string };
  brakeLatched: boolean;
  canBrake: boolean;
  canReleaseBrake: boolean;
  loadingBrake: boolean;
  onBrake: () => void;
  onBrakeRelease: () => void;
  hostState: string;
  mcuState: string;
  speedRefPu: number;
  speedFbkPu: number;
  idRefPu: number;
  idFbkPu: number;
  iqRefPu: number;
  iqFbkPu: number;
  inputCurrentA: number;
  inputPowerW: number;
  powerPercent: number;
  powerFillPercent: number;
  voltageTone: { label: string; className: string };
  latestFault: SessionSnapshot["recent_faults"][number] | null;
  health: SessionSnapshot["health"];
  status: NonNullable<SessionSnapshot["latest_mcu_status"]> | Record<string, never>;
  motorConfig: SessionSnapshot["motor_config"];
}) {
  const phaseMagnitudePu = Math.hypot(idFbkPu, iqFbkPu);

  return (
    <div className="motor-workspace dedicated-mode">
      <section className="panel motor-panel motor-panel-dedicated">
        <div className="panel-heading motor-panel-heading-compact">
          <div>
            <div className="heading-line">
              <UiIcon name="motor" className="heading-icon" />
              <h2>Motor Control</h2>
              <InfoHint text="Compact operator view tuned for the dedicated page so the full speed, safety, current, and power picture stays visible without normal scrolling on a laptop viewport." />
            </div>
          </div>
          <div className="motor-panel-heading-badges">
            <span className={telemetryChip.className}>{telemetryChip.text}</span>
            <span className={`badge ${brakeLatched ? "danger" : "muted"}`}>{brakeLatched ? "Brake latched" : "No override"}</span>
          </div>
        </div>

        <div className="motor-status-grid compact">
          <article className="motor-status-card compact">
            <span>Host Control</span>
            <strong>{hostState}</strong>
            <small>{`Port ${snapshot.port || "demo"}`}</small>
          </article>
          <article className="motor-status-card compact">
            <span>MCU Control</span>
            <strong>{mcuState}</strong>
            <small>{`runMotor ${Number(status.run_motor ?? 0)}`}</small>
          </article>
          <article className="motor-status-card compact">
            <span>Trip / Faults</span>
            <strong>{formatTripFlag(status.trip_flag)}</strong>
            <small>{latestFault ? `Latest count ${latestFault.trip_count ?? 0}` : "No recent fault frame"}</small>
          </article>
          <article className="motor-status-card compact">
            <span>Brake Override</span>
            <strong>{brakeLatched ? "Latched" : "Released"}</strong>
            <small>{`Last frame ${formatTimestamp(health.last_frame_at)}`}</small>
          </article>
        </div>

        <div className="motor-dedicated-main-grid">
          <CompactSpeedBand
            speedRefPu={speedRefPu}
            speedFbkPu={speedFbkPu}
            baseSpeedRpm={Number(motorConfig.base_speed_rpm || 0)}
            hostState={hostState}
            mcuState={mcuState}
            brakeLatched={brakeLatched}
          />

          <article className="motor-safety-card motor-safety-card-compact">
            <div className="motor-card-head">
              <div>
                <span className="motor-card-kicker">Safety</span>
                <strong>Brake Latch & Unlatch</strong>
              </div>
              <span className={telemetryChip.className}>{telemetryChip.text}</span>
            </div>
            <p className="motor-compact-copy">
              Emergency brake latches above the controller mapping. Releasing it sends STOP immediately with zeroed references.
            </p>
            <div className="motor-safety-stack">
              <button
                className="primary danger-button"
                disabled={!canBrake || brakeLatched || loadingBrake}
                onClick={onBrake}
              >
                <UiIcon name="shield" />
                {loadingBrake ? "Updating Brake..." : brakeLatched ? "Brake Latched" : "Emergency Brake"}
              </button>
              <button className="panel-link-button" disabled={!canReleaseBrake || loadingBrake} onClick={onBrakeRelease}>
                <UiIcon name="cancel" />
                Unlatch Brake (Send STOP)
              </button>
            </div>
            <div className="motor-compact-meta-grid">
              <div>
                <span>Session</span>
                <strong>{snapshot.session_state || "idle"}</strong>
              </div>
              <div>
                <span>Last Status</span>
                <strong>{formatTimestamp(health.last_status_at)}</strong>
              </div>
              <div>
                <span>Fault Frames</span>
                <strong>{String(snapshot.counters.fault_frames ?? 0)}</strong>
              </div>
              <div>
                <span>Serial Errors</span>
                <strong>{String(snapshot.counters.serial_errors ?? 0)}</strong>
              </div>
            </div>
          </article>
        </div>

        <div className="motor-dedicated-lower-shell">
          <div className="motor-trend-stack">
            <TrendChart compact samples={snapshot.telemetry_samples || []} title="Id" refKey="id_ref" fbkKey="id_fbk" />
            <TrendChart compact samples={snapshot.telemetry_samples || []} title="Iq" refKey="iq_ref" fbkKey="iq_fbk" />
          </div>

          <div className="motor-electrical-rail">
            <article className={`motor-emphasis-card voltage-card compact motor-voltage-hero ${voltageTone.className}`}>
              <div className="motor-card-head">
                <div>
                  <span className="motor-card-kicker">DC Bus</span>
                  <strong>{`${formatFixed(status.vdc_bus, 1)} V`}</strong>
                </div>
                <span className={`badge ${voltageTone.className}`}>{voltageTone.label}</span>
              </div>
              <p className="motor-compact-copy">
                {`${formatFixed(motorConfig.vdcbus_min_v, 1)} V to ${formatFixed(motorConfig.vdcbus_max_v, 1)} V`}
              </p>
            </article>

            <div className="motor-electrical-mini-grid">
              <article className="motor-data-card compact">
                <div className="motor-card-head">
                  <div>
                    <span className="motor-card-kicker">d/q Snapshot</span>
                    <strong>Reference vs Feedback</strong>
                  </div>
                </div>
                <dl className="motor-pair-list compact dense">
                  <div>
                    <dt>Id Ref</dt>
                    <dd>{`${formatSigned(idRefPu, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Id Fbk</dt>
                    <dd>{`${formatSigned(idFbkPu, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Iq Ref</dt>
                    <dd>{`${formatSigned(iqRefPu, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Iq Fbk</dt>
                    <dd>{`${formatSigned(iqFbkPu, 3)} pu`}</dd>
                  </div>
                </dl>
              </article>

              <article className="motor-data-card compact">
                <div className="motor-card-head">
                  <div>
                    <span className="motor-card-kicker">Phase Currents</span>
                    <strong>Instantaneous Snapshot</strong>
                  </div>
                </div>
                <dl className="motor-pair-list compact dense">
                  <div>
                    <dt>Phase A</dt>
                    <dd>{`${formatSigned(status.current_as, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Phase B</dt>
                    <dd>{`${formatSigned(status.current_bs, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Phase C</dt>
                    <dd>{`${formatSigned(status.current_cs, 3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Rotor Theta</dt>
                    <dd>{`${formatSigned(status.pos_mech_theta, 4)} pu`}</dd>
                  </div>
                </dl>
              </article>

              <article className="motor-data-card compact temperature-compact-card">
                <div className="motor-card-head">
                  <div>
                    <span className="motor-card-kicker">Temperature</span>
                    <strong>Future Sources</strong>
                  </div>
                </div>
                <div className="motor-temperature-stack">
                  <div className="temperature-pill compact">
                    <span>Motor Winding</span>
                    <strong>{formatTemperature(status.temp_motor_winding_c)}</strong>
                  </div>
                  <div className="temperature-pill compact">
                    <span>MCU</span>
                    <strong>{formatTemperature(status.temp_mcu_c)}</strong>
                  </div>
                  <div className="temperature-pill compact">
                    <span>IGBTs</span>
                    <strong>{formatTemperature(status.temp_igbts_c)}</strong>
                  </div>
                </div>
              </article>

              <article className="motor-data-card power-card compact">
                <div className="motor-card-head">
                  <div>
                    <span className="motor-card-kicker">Input Electrical Power</span>
                    <strong>{`${Math.round(inputPowerW)} W`}</strong>
                  </div>
                  <span className="badge muted">{`${Math.round(powerPercent)}% of base`}</span>
                </div>
                <div className="power-bar-shell" aria-label="Input electrical power">
                  <div className="power-bar-fill" style={{ width: `${powerFillPercent}%` }} />
                </div>
                <dl className="motor-pair-list compact dense">
                  <div>
                    <dt>|I_dq|</dt>
                    <dd>{`${phaseMagnitudePu.toFixed(3)} pu`}</dd>
                  </div>
                  <div>
                    <dt>Base Current</dt>
                    <dd>{`${formatFixed(motorConfig.base_current_a, 2)} A`}</dd>
                  </div>
                  <div>
                    <dt>Input Current</dt>
                    <dd>{`${inputCurrentA.toFixed(2)} A`}</dd>
                  </div>
                  <div>
                    <dt>Base Power</dt>
                    <dd>{`${Math.round(motorConfig.rated_input_power_w || 0)} W`}</dd>
                  </div>
                </dl>
              </article>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
}

export function MotorControlPanel({
  snapshot,
  loadingBrake,
  onBrake,
  onBrakeRelease,
  dedicatedPage = false,
  detailPath = "/mcu/primary",
}: MotorControlPanelProps) {
  const health = snapshot.health || {};
  const status = snapshot.latest_mcu_status || {};
  const command = snapshot.last_host_command || {};
  const motorConfig = snapshot.motor_config || {
    base_speed_rpm: 0,
    base_current_a: 0,
    vdcbus_min_v: 0,
    vdcbus_max_v: 0,
    rated_input_power_w: 0,
  };

  const hostState = normalizeCtrlState(command.ctrl_state ?? "STOP");
  const mcuState = normalizeCtrlState(status.ctrl_state ?? command.ctrl_state ?? "STOP");
  const brakeLatched = snapshot.active_override === "BRAKE";
  const telemetryChip = getTelemetryChip(health);
  const canBrake = snapshot.session_state === "running";
  const canReleaseBrake = canBrake && brakeLatched;
  const speedRefPu = Number(command.speed_ref ?? status.speed_ref ?? 0);
  const speedFbkPu = Number(status.speed_fbk ?? 0);
  const idRefPu = Number(command.id_ref ?? 0);
  const idFbkPu = Number(status.id_fbk ?? 0);
  const iqRefPu = Number(command.iq_ref ?? 0);
  const iqFbkPu = Number(status.iq_fbk ?? 0);
  const inputCurrentA = Math.hypot(idFbkPu, iqFbkPu) * Number(motorConfig.base_current_a || 0);
  const inputPowerW = Number(status.vdc_bus ?? 0) * inputCurrentA;
  const powerPercent = motorConfig.rated_input_power_w > 0 ? (inputPowerW / motorConfig.rated_input_power_w) * 100 : 0;
  const powerFillPercent = clamp(powerPercent, 0, 100);
  const voltageTone = getVoltageTone(
    Number(status.vdc_bus ?? 0),
    Number(motorConfig.vdcbus_min_v || 0),
    Number(motorConfig.vdcbus_max_v || 0),
  );
  const latestFault = snapshot.recent_faults[snapshot.recent_faults.length - 1] || null;

  if (dedicatedPage) {
    return (
      <DedicatedMotorPanel
        snapshot={snapshot}
        telemetryChip={telemetryChip}
        brakeLatched={brakeLatched}
        canBrake={canBrake}
        canReleaseBrake={canReleaseBrake}
        loadingBrake={loadingBrake}
        onBrake={onBrake}
        onBrakeRelease={onBrakeRelease}
        hostState={hostState}
        mcuState={mcuState}
        speedRefPu={speedRefPu}
        speedFbkPu={speedFbkPu}
        idRefPu={idRefPu}
        idFbkPu={idFbkPu}
        iqRefPu={iqRefPu}
        iqFbkPu={iqFbkPu}
        inputCurrentA={inputCurrentA}
        inputPowerW={inputPowerW}
        powerPercent={powerPercent}
        powerFillPercent={powerFillPercent}
        voltageTone={voltageTone}
        latestFault={latestFault}
        health={health}
        status={status}
        motorConfig={motorConfig}
      />
    );
  }

  return (
    <SharedMotorPanel
      snapshot={snapshot}
      detailPath={detailPath}
      telemetryChip={telemetryChip}
      brakeLatched={brakeLatched}
      canBrake={canBrake}
      canReleaseBrake={canReleaseBrake}
      loadingBrake={loadingBrake}
      onBrake={onBrake}
      onBrakeRelease={onBrakeRelease}
      hostState={hostState}
      mcuState={mcuState}
      speedRefPu={speedRefPu}
      speedFbkPu={speedFbkPu}
      idRefPu={idRefPu}
      idFbkPu={idFbkPu}
      iqRefPu={iqRefPu}
      iqFbkPu={iqFbkPu}
      inputCurrentA={inputCurrentA}
      inputPowerW={inputPowerW}
      powerPercent={powerPercent}
      powerFillPercent={powerFillPercent}
      voltageTone={voltageTone}
      latestFault={latestFault}
      health={health}
      status={status}
      motorConfig={motorConfig}
    />
  );
}
