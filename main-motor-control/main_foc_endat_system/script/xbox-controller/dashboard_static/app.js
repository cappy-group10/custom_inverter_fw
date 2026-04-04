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
];

const LOG_RENDER_INTERVAL_MS = 140;
const FRAME_SCROLLBACK_LIMIT = 1000;
const EVENT_SCROLLBACK_LIMIT = 50;

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
};

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

const state = {
  snapshot: null,
  portOptions: [],
  activeTab: "overview",
  controlNodes: {},
  uartFilters: {
    tx: true,
    rx: true,
    errorsOnly: false,
    search: "",
  },
  uartAutoScroll: true,
  uartNeedsFullRender: true,
  uartRenderedFrameCount: 0,
  uartRenderedFirstKey: null,
  uartSelectedFrameKey: null,
  framePaused: false,
  eventPaused: false,
  frameListDirty: false,
  eventListDirty: false,
  frameRenderTimer: null,
  eventRenderTimer: null,
  reconnectTimer: null,
  socket: null,
  chartSampleMs: 0,
};

const els = {};

document.addEventListener("DOMContentLoaded", async () => {
  bindElements();
  cacheControlNodes();
  bindTabs();
  bindControls();
  await loadPorts();
  await refreshState();
  connectWebSocket();
});

function bindElements() {
  const ids = [
    "session-chip",
    "mode-chip",
    "health-banner",
    "health-title",
    "health-message",
    "ws-indicator",
    "port-select",
    "port-help",
    "baudrate-input",
    "joystick-input",
    "start-button",
    "stop-button",
    "refresh-ports",
    "meta-port",
    "meta-controller",
    "meta-started",
    "telemetry-chip",
    "counter-tx",
    "counter-rx",
    "counter-checksum",
    "counter-serial",
    "meta-last-status",
    "meta-last-frame",
    "command-state",
    "cmd-speed",
    "cmd-id",
    "cmd-iq",
    "controller-badge",
    "left-stick-dot",
    "left-stick-vector",
    "right-stick-dot",
    "right-stick-vector",
    "left-stick-value",
    "right-stick-value",
    "lt-bar",
    "rt-bar",
    "lt-value",
    "rt-value",
    "controller-status-strip",
    "controller-groups",
    "mcu-state",
    "mcu-speed",
    "mcu-theta",
    "mcu-vdc",
    "mcu-trip",
    "mcu-id",
    "mcu-iq",
    "mcu-ia",
    "mcu-ib",
    "mcu-ic",
    "mcu-isr",
    "fault-list",
    "chart-speed",
    "chart-dq",
    "chart-vdc",
    "chart-phase",
    "uart-summary-port",
    "uart-summary-baud",
    "uart-summary-tx",
    "uart-summary-rx",
    "uart-summary-checksum",
    "uart-summary-serial",
    "uart-summary-last-frame",
    "uart-search",
    "uart-filter-tx",
    "uart-filter-rx",
    "uart-errors-only",
    "uart-autoscroll",
    "uart-terminal",
    "uart-inspector",
    "event-list",
    "toggle-uart-pause",
    "toggle-event-pause",
    "clear-uart",
    "clear-events",
  ];

  ids.forEach((id) => {
    els[id] = document.getElementById(id);
  });
}

function cacheControlNodes() {
  state.controlNodes = {};
  document.querySelectorAll("[data-control]").forEach((node) => {
    state.controlNodes[node.dataset.control] = node;
  });
}

function bindControls() {
  els["refresh-ports"].addEventListener("click", loadPorts);
  els["port-select"].addEventListener("change", renderPortHelp);
  els["start-button"].addEventListener("click", startSession);
  els["stop-button"].addEventListener("click", stopSession);
  els["toggle-uart-pause"].addEventListener("click", () => {
    state.framePaused = !state.framePaused;
    els["toggle-uart-pause"].textContent = state.framePaused ? "Resume" : "Pause";
    if (!state.framePaused) {
      queueFrameRender({ forceFull: true });
    }
  });
  els["uart-autoscroll"].checked = state.uartAutoScroll;
  els["uart-autoscroll"].addEventListener("change", () => {
    state.uartAutoScroll = Boolean(els["uart-autoscroll"].checked);
    if (state.uartAutoScroll) {
      scrollUartTerminalToBottom();
    }
  });
  ["uart-filter-tx", "uart-filter-rx", "uart-errors-only"].forEach((id) => {
    els[id].addEventListener("change", () => {
      updateUartFiltersFromControls();
      state.uartNeedsFullRender = true;
      if (state.activeTab === "uart") {
        renderUartTerminal(state.snapshot?.recent_frames || [], { forceFull: true });
      } else {
        state.frameListDirty = true;
      }
    });
  });
  els["uart-search"].addEventListener("input", () => {
    updateUartFiltersFromControls();
    state.uartNeedsFullRender = true;
    if (state.activeTab === "uart") {
      renderUartTerminal(state.snapshot?.recent_frames || [], { forceFull: true });
    } else {
      state.frameListDirty = true;
    }
  });
  els["uart-terminal"].addEventListener("click", (event) => {
    const row = event.target.closest("[data-frame-key]");
    if (!row) {
      return;
    }
    selectUartFrame(row.dataset.frameKey);
  });
  els["toggle-event-pause"].addEventListener("click", () => {
    state.eventPaused = !state.eventPaused;
    els["toggle-event-pause"].textContent = state.eventPaused ? "Resume" : "Pause";
    if (!state.eventPaused) {
      queueEventRender();
    }
  });
  els["clear-uart"].addEventListener("click", () => {
    if (state.snapshot) {
      state.snapshot.recent_frames = [];
    }
    if (state.frameRenderTimer !== null) {
      window.clearTimeout(state.frameRenderTimer);
      state.frameRenderTimer = null;
    }
    state.frameListDirty = false;
    state.uartNeedsFullRender = true;
    state.uartRenderedFrameCount = 0;
    state.uartRenderedFirstKey = null;
    state.uartSelectedFrameKey = null;
    renderUartTerminal([]);
    renderUartInspector(null);
    if (state.snapshot) {
      renderSummary(state.snapshot);
    }
  });
  els["clear-events"].addEventListener("click", () => {
    if (state.snapshot) {
      state.snapshot.recent_events = [];
    }
    if (state.eventRenderTimer !== null) {
      window.clearTimeout(state.eventRenderTimer);
      state.eventRenderTimer = null;
    }
    state.eventListDirty = false;
    renderEvents([]);
  });

  updateUartFiltersFromControls();
}

function updateUartFiltersFromControls() {
  state.uartFilters = {
    tx: Boolean(els["uart-filter-tx"]?.checked),
    rx: Boolean(els["uart-filter-rx"]?.checked),
    errorsOnly: Boolean(els["uart-errors-only"]?.checked),
    search: (els["uart-search"]?.value || "").trim().toLowerCase(),
  };
}

function bindTabs() {
  const tabButtons = [...document.querySelectorAll("[data-tab-target]")];
  tabButtons.forEach((button) => {
    button.addEventListener("click", () => {
      setActiveTab(button.dataset.tabTarget || "overview");
    });
  });

  window.addEventListener("hashchange", () => {
    setActiveTab(getTabFromHash(), { updateHash: false });
  });

  setActiveTab(getTabFromHash(), { updateHash: false });
}

function getTabFromHash() {
  const hash = window.location.hash.replace(/^#/, "").trim().toLowerCase();
  if (hash === "frames") {
    return "uart";
  }
  const validTabs = new Set(["overview", "controller", "telemetry", "uart", "events"]);
  return validTabs.has(hash) ? hash : "overview";
}

function setActiveTab(tabName, { updateHash = true } = {}) {
  state.activeTab = tabName;

  document.querySelectorAll("[data-tab-target]").forEach((button) => {
    const isActive = button.dataset.tabTarget === tabName;
    button.classList.toggle("active", isActive);
    button.setAttribute("aria-selected", String(isActive));
    button.tabIndex = isActive ? 0 : -1;
  });

  document.querySelectorAll("[data-tab-panel]").forEach((panel) => {
    const isActive = panel.dataset.tabPanel === tabName;
    panel.classList.toggle("active", isActive);
    panel.hidden = !isActive;
  });

  if (updateHash) {
    const nextHash = `#${tabName}`;
    if (window.location.hash !== nextHash) {
      window.history.replaceState(null, "", nextHash);
    }
  }

  if (tabName === "uart" && state.frameListDirty) {
    renderUartTerminal(state.snapshot?.recent_frames || [], { forceFull: true });
  }
  if (tabName === "events" && state.eventListDirty && !state.eventPaused) {
    renderEvents(state.snapshot?.recent_events || []);
  }
}

async function fetchJSON(url, options = {}) {
  const response = await fetch(url, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({ detail: "Request failed" }));
    throw new Error(error.detail || "Request failed");
  }
  return response.json();
}

async function loadPorts() {
  const payload = await fetchJSON("/api/ports");
  state.portOptions = Array.isArray(payload.ports) ? payload.ports : [];
  const current = els["port-select"].value;
  els["port-select"].innerHTML = "";
  state.portOptions.forEach((portInfo) => {
    const option = document.createElement("option");
    option.value = portInfo.port;
    option.textContent = portInfo.label;
    option.title = [portInfo.usage, portInfo.description].filter(Boolean).join("\n");
    els["port-select"].append(option);
  });
  if (current && [...els["port-select"].options].some((option) => option.value === current)) {
    els["port-select"].value = current;
  }
  renderPortHelp();
}

async function refreshState() {
  const snapshot = await fetchJSON("/api/state");
  applySnapshot(snapshot);
}

async function startSession() {
  try {
    const snapshot = await fetchJSON("/api/session/start", {
      method: "POST",
      body: JSON.stringify({
        port: els["port-select"].value,
        baudrate: Number(els["baudrate-input"].value),
        joystick_index: Number(els["joystick-input"].value),
      }),
    });
    applySnapshot(snapshot);
  } catch (error) {
    showTransientError(error.message);
  }
}

async function stopSession() {
  try {
    const snapshot = await fetchJSON("/api/session/stop", { method: "POST" });
    applySnapshot(snapshot);
  } catch (error) {
    showTransientError(error.message);
  }
}

function connectWebSocket() {
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const socket = new WebSocket(`${protocol}://${window.location.host}/api/stream`);
  state.socket = socket;

  socket.addEventListener("open", () => {
    setSocketIndicator(true);
  });

  socket.addEventListener("message", (event) => {
    const parsed = JSON.parse(event.data);
    handleStreamEvent(parsed.type, parsed.payload);
  });

  socket.addEventListener("close", () => {
    setSocketIndicator(false);
    window.clearTimeout(state.reconnectTimer);
    state.reconnectTimer = window.setTimeout(connectWebSocket, 1500);
  });

  socket.addEventListener("error", () => {
    setSocketIndicator(false);
  });
}

function setSocketIndicator(online) {
  els["ws-indicator"].textContent = online ? "Socket live" : "Socket offline";
  els["ws-indicator"].className = `badge ${online ? "online" : "offline"}`;
}

function handleStreamEvent(type, payload) {
  switch (type) {
    case "snapshot":
      applySnapshot(payload);
      break;
    case "controller_state":
      ensureSnapshot();
      state.snapshot.controller_state = payload;
      renderController(payload);
      break;
    case "host_command":
      ensureSnapshot();
      state.snapshot.last_host_command = payload;
      renderCommand(payload);
      break;
    case "tx_frame":
      ensureSnapshot();
      appendFrame(payload);
      break;
    case "mcu_status":
      ensureSnapshot();
      state.snapshot.latest_mcu_status = payload.status;
      if (payload.frame) {
        appendFrame(payload.frame);
      }
      appendTelemetrySample(payload.status);
      renderTelemetry(payload.status);
      renderSummary(state.snapshot);
      break;
    case "fault":
      ensureSnapshot();
      if (!Array.isArray(state.snapshot.recent_faults)) {
        state.snapshot.recent_faults = [];
      }
      state.snapshot.recent_faults = limitList([...state.snapshot.recent_faults, payload.fault], 50);
      renderFaults(state.snapshot.recent_faults);
      if (payload.frame) {
        appendFrame(payload.frame);
      }
      break;
    case "health":
      ensureSnapshot();
      state.snapshot.health = payload;
      renderHealth(state.snapshot);
      renderSummary(state.snapshot);
      break;
    case "event":
      ensureSnapshot();
      appendEvent(payload);
      break;
    default:
      break;
  }
}

function ensureSnapshot() {
  if (!state.snapshot) {
    state.snapshot = {};
  }
}

function applySnapshot(snapshot) {
  state.snapshot = snapshot;
  state.chartSampleMs = 0;
  renderSession(snapshot);
  renderSummary(snapshot);
  renderUartSummary(snapshot);
  renderCommand(snapshot.last_host_command || {});
  renderControllerLayout(snapshot.controller_layout || []);
  renderController(snapshot.controller_state || {});
  renderTelemetry(snapshot.latest_mcu_status || null);
  renderFaults(snapshot.recent_faults || []);
  if (state.activeTab === "uart") {
    renderUartTerminal(snapshot.recent_frames || [], { forceFull: true });
  } else {
    state.frameListDirty = true;
    state.uartNeedsFullRender = true;
  }
  if (state.activeTab === "events") {
    renderEvents(snapshot.recent_events || []);
  } else {
    state.eventListDirty = true;
  }
  renderCharts(snapshot.telemetry_samples || []);
  renderHealth(snapshot);
}

function renderSession(snapshot) {
  els["session-chip"].textContent = snapshot.session_state || "idle";
  els["mode-chip"].textContent = snapshot.mode === "drive" ? "Drive Mode" : snapshot.mode || "Mode";
  els["meta-port"].textContent = snapshot.port || "demo";
  els["meta-controller"].textContent = snapshot.joystick_name || "Not connected";
  els["meta-started"].textContent = formatTimestamp(snapshot.started_at);
  els["command-state"].textContent = snapshot.last_host_command?.ctrl_state || "STOP";
  els["mcu-state"].textContent = snapshot.latest_mcu_status?.ctrl_state || "STOP";
  els["controller-badge"].textContent = snapshot.controller_connected ? "Connected" : "Disconnected";
  els["controller-badge"].className = `badge ${snapshot.controller_connected ? "good" : "offline"}`;
  els["port-select"].value = snapshot.port || "demo";
  els["baudrate-input"].value = snapshot.baudrate || 115200;
  els["joystick-input"].value = snapshot.joystick_index || 0;
  renderPortHelp();
}

function renderPortHelp() {
  const ports = Array.isArray(state.portOptions) ? state.portOptions : [];
  const selectedValue = els["port-select"].value || "demo";
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

  els["port-help"].textContent = message;
  els["port-help"].className = `control-help${tone ? ` ${tone}` : ""}`;
}

function renderSummary(snapshot) {
  const counters = snapshot.counters || {};
  const health = snapshot.health || {};
  els["counter-tx"].textContent = counters.tx_frames ?? 0;
  els["counter-rx"].textContent = counters.rx_frames ?? 0;
  els["counter-checksum"].textContent = counters.checksum_errors ?? 0;
  els["counter-serial"].textContent = counters.serial_errors ?? 0;
  els["meta-last-status"].textContent = formatTimestamp(health.last_status_at);
  els["meta-last-frame"].textContent = formatTimestamp(health.last_frame_at);

  let telemetryText = "No telemetry";
  let telemetryClass = "badge";
  if (health.terminal_only) {
    telemetryText = "TX only";
    telemetryClass = "badge warn";
  } else if (health.has_mcu_telemetry && !health.telemetry_stale) {
    telemetryText = "Telemetry live";
    telemetryClass = "badge good";
  } else if (health.telemetry_stale) {
    telemetryText = "Telemetry stale";
    telemetryClass = "badge danger";
  }
  els["telemetry-chip"].textContent = telemetryText;
  els["telemetry-chip"].className = telemetryClass;
  renderUartSummary(snapshot);
}

function renderUartSummary(snapshot) {
  const counters = snapshot?.counters || {};
  const health = snapshot?.health || {};
  els["uart-summary-port"].textContent = snapshot?.port || "demo";
  els["uart-summary-baud"].textContent = String(snapshot?.baudrate || 115200);
  els["uart-summary-tx"].textContent = counters.tx_frames ?? 0;
  els["uart-summary-rx"].textContent = counters.rx_frames ?? 0;
  els["uart-summary-checksum"].textContent = counters.checksum_errors ?? 0;
  els["uart-summary-serial"].textContent = counters.serial_errors ?? 0;
  els["uart-summary-last-frame"].textContent = formatTimestamp(health.last_frame_at);
}

function renderHealth(snapshot) {
  const health = snapshot.health || {};
  let title = "Waiting for session";
  let message = "Start the runtime to begin streaming controller and UART activity.";
  let level = "neutral";

  if (snapshot.session_state === "running") {
    title = "Drive runtime active";
    message = health.terminal_only
      ? "Controller and host TX are running in demo mode. No MCU telemetry is expected."
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
    message = snapshot.last_error || health.last_error;
    level = "danger";
  }

  if (snapshot.session_state === "stopped") {
    title = "Runtime stopped";
    message = "The drive loop is idle. You can start a new session at any time.";
    level = "neutral";
  }

  els["health-title"].textContent = title;
  els["health-message"].textContent = message;
  els["health-banner"].className = `health-banner ${level}`;
}

function renderCommand(command) {
  els["command-state"].textContent = command?.ctrl_state || "STOP";
  els["cmd-speed"].textContent = formatSigned(command?.speed_ref, 4);
  els["cmd-id"].textContent = formatSigned(command?.id_ref, 4);
  els["cmd-iq"].textContent = formatSigned(command?.iq_ref, 4);
  renderControllerStatusStrip({
    ctrlState: command?.ctrl_state || "STOP",
    speedRef: command?.speed_ref || 0,
    idRef: command?.id_ref || 0,
    iqRef: command?.iq_ref || 0,
  });
}

function renderControllerLayout(layout) {
  const groups = groupControllerLayout(layout);
  const markup = controllerGroupOrder
    .filter((groupName) => groups[groupName]?.length)
    .map((groupName) => {
      const groupInfo = controllerGroupMeta[groupName];
      const rows = groups[groupName]
        .map((item) => `
          <div class="controller-group-row">
            <div>
              <strong>${escapeHtml(item.label)}</strong>
              <small>${escapeHtml(item.mapping_text)}</small>
            </div>
            <span class="mapping-chip ${item.mapped ? "mapped" : "unmapped"}">${escapeHtml(item.mapping_target)}</span>
          </div>
        `)
        .join("");

      return `
        <article class="controller-group-card">
          <h3>${escapeHtml(groupInfo?.title || groupName)}</h3>
          <p>${escapeHtml(groupInfo?.description || "")}</p>
          <div class="controller-group-list">${rows}</div>
        </article>
      `;
    })
    .join("");

  els["controller-groups"].innerHTML = markup || `<div class="placeholder">Controller layout metadata is unavailable.</div>`;
}

function renderController(controllerState) {
  const leftX = Number(controllerState?.left_x || 0);
  const leftY = Number(controllerState?.left_y || 0);
  const rightX = Number(controllerState?.right_x || 0);
  const rightY = Number(controllerState?.right_y || 0);
  positionStick(els["left-stick-dot"], els["left-stick-vector"], leftX, leftY);
  positionStick(els["right-stick-dot"], els["right-stick-vector"], rightX, rightY);
  els["left-stick-value"].textContent = `X ${formatSigned(leftX, 2)} / Y ${formatSigned(leftY, 2)}`;
  els["right-stick-value"].textContent = `X ${formatSigned(rightX, 2)} / Y ${formatSigned(rightY, 2)}`;

  const lt = normalizeTrigger(controllerState?.left_trigger);
  const rt = normalizeTrigger(controllerState?.right_trigger);
  els["lt-bar"].style.width = `${lt}%`;
  els["rt-bar"].style.width = `${rt}%`;
  els["lt-value"].textContent = formatSigned(controllerState?.left_trigger || 0, 2);
  els["rt-value"].textContent = formatSigned(controllerState?.right_trigger || 0, 2);

  const buttons = controllerState?.buttons || {};
  buttonNames.forEach((name) => setControlActive(name, Boolean(buttons[name])));
  setControlActive("left_trigger", lt > 1);
  setControlActive("right_trigger", rt > 1);
  setControlActive("left_stick", Math.abs(leftX) > 0.02 || Math.abs(leftY) > 0.02 || Boolean(buttons.left_stick));
  setControlActive("right_stick", Math.abs(rightX) > 0.02 || Math.abs(rightY) > 0.02 || Boolean(buttons.right_stick));

}

function renderControllerStatusStrip({ ctrlState, speedRef, idRef, iqRef }) {
  els["controller-status-strip"].innerHTML = `
    <article class="status-mini-card">
      <span>Host Ctrl State</span>
      <strong>${escapeHtml(String(ctrlState || "STOP"))}</strong>
    </article>
    <article class="status-mini-card">
      <span>Mapped Speed Variable</span>
      <strong>speed_ref ${formatSigned(speedRef, 4)}</strong>
    </article>
    <article class="status-mini-card">
      <span>Mapped D-Axis Variable</span>
      <strong>id_ref ${formatSigned(idRef, 4)}</strong>
    </article>
    <article class="status-mini-card">
      <span>Mapped Q-Axis Variable</span>
      <strong>iq_ref ${formatSigned(iqRef, 4)}</strong>
    </article>
  `;
}

function setControlActive(controlId, active) {
  const node = state.controlNodes[controlId];
  if (!node) {
    return;
  }
  node.classList.toggle("active", Boolean(active));
}

function groupControllerLayout(layout) {
  const grouped = {};
  const safeLayout = [...layout].sort((left, right) => {
    return controllerControlOrder.indexOf(left.control_id) - controllerControlOrder.indexOf(right.control_id);
  });

  safeLayout.forEach((item) => {
    const groupName = item.group || "other";
    if (!grouped[groupName]) {
      grouped[groupName] = [];
    }
    grouped[groupName].push(item);
    const node = state.controlNodes[item.control_id];
    if (node) {
      node.classList.toggle("unmapped", !item.mapped);
      node.title = `${item.label}: ${item.mapping_text}`;
    }
  });

  return grouped;
}

function renderTelemetry(status) {
  const safe = status || {};
  els["mcu-state"].textContent = safe.ctrl_state || "STOP";
  els["mcu-speed"].textContent = formatSigned(safe.speed_ref, 4);
  els["mcu-theta"].textContent = formatSigned(safe.pos_mech_theta, 4);
  els["mcu-vdc"].textContent = formatFixed(safe.vdc_bus, 1);
  els["mcu-trip"].textContent = formatTripFlag(safe.trip_flag);
  els["mcu-id"].textContent = formatSigned(safe.id_fbk, 4);
  els["mcu-iq"].textContent = formatSigned(safe.iq_fbk, 4);
  els["mcu-ia"].textContent = formatSigned(safe.current_as, 3);
  els["mcu-ib"].textContent = formatSigned(safe.current_bs, 3);
  els["mcu-ic"].textContent = formatSigned(safe.current_cs, 3);
  els["mcu-isr"].textContent = safe.isr_ticker ?? 0;
}

function renderFaults(faults) {
  if (!faults.length) {
    els["fault-list"].innerHTML = `<div class="placeholder">No faults captured in this session.</div>`;
    return;
  }
  const items = faults
    .slice()
    .reverse()
    .map(
      (fault) => `
        <article class="fault-card">
          <strong>${formatTripFlag(fault.trip_flag)}</strong>
          <p>Trip count ${fault.trip_count ?? 0}</p>
        </article>
      `,
    )
    .join("");
  els["fault-list"].innerHTML = items;
}

function renderUartTerminal(frames, { forceFull = false } = {}) {
  state.frameListDirty = false;
  const safeFrames = Array.isArray(frames) ? frames : [];
  let selectedFrame = findSelectedUartFrame(safeFrames);

  if (!safeFrames.length) {
    els["uart-terminal"].innerHTML = `<div class="placeholder">No UART activity yet.</div>`;
    state.uartNeedsFullRender = false;
    state.uartRenderedFrameCount = 0;
    state.uartRenderedFirstKey = null;
    renderUartInspector(null);
    return;
  }

  if (!forceFull && canIncrementallyAppendUart(safeFrames)) {
    appendUartRows(safeFrames.slice(state.uartRenderedFrameCount), safeFrames);
    updateUartRenderTracking(safeFrames);
  } else {
    rebuildUartTerminal(safeFrames);
  }

  if (!selectedFrame && state.uartSelectedFrameKey) {
    state.uartSelectedFrameKey = null;
  }
  selectedFrame = findSelectedUartFrame(safeFrames);
  state.uartNeedsFullRender = false;
  renderUartInspector(selectedFrame);
  syncSelectedUartRow();
  if (state.uartAutoScroll) {
    scrollUartTerminalToBottom();
  }
}

function renderEvents(events) {
  state.eventListDirty = false;
  if (!events.length) {
    els["event-list"].innerHTML = `<div class="placeholder">No events yet.</div>`;
    return;
  }
  els["event-list"].innerHTML = events
    .slice()
    .reverse()
    .map((event) => createLogItem({
      title: event.title || event.kind,
      timestamp: event.timestamp,
      body: event.message || "",
      extra: Object.keys(event.data || {}).length ? JSON.stringify(event.data) : "",
    }))
    .join("");
}

function appendFrame(frame) {
  if (!state.snapshot) {
    return;
  }
  const frames = Array.isArray(state.snapshot.recent_frames) ? state.snapshot.recent_frames : [];
  state.snapshot.recent_frames = limitList([...frames, frame], FRAME_SCROLLBACK_LIMIT);

  const counters = { ...(state.snapshot.counters || {}) };
  if (frame.direction === "tx") {
    counters.tx_frames = (counters.tx_frames ?? 0) + 1;
  } else {
    counters.rx_frames = (counters.rx_frames ?? 0) + 1;
  }
  if (!frame.checksum_ok) {
    counters.checksum_errors = (counters.checksum_errors ?? 0) + 1;
  }
  state.snapshot.counters = counters;
  state.snapshot.health = {
    ...(state.snapshot.health || {}),
    last_frame_at: frame.timestamp,
  };

  renderSummary(state.snapshot);
  queueFrameRender({ forceFull: !isDefaultUartView() });
}

function appendEvent(event) {
  if (!state.snapshot) {
    return;
  }
  const events = Array.isArray(state.snapshot.recent_events) ? state.snapshot.recent_events : [];
  state.snapshot.recent_events = limitList([...events, event], EVENT_SCROLLBACK_LIMIT);
  queueEventRender();
}

function appendTelemetrySample(status) {
  if (!state.snapshot) {
    return;
  }
  const now = Date.now();
  if (now - state.chartSampleMs < 100) {
    return;
  }
  state.chartSampleMs = now;
  const samples = Array.isArray(state.snapshot.telemetry_samples) ? state.snapshot.telemetry_samples : [];
  state.snapshot.telemetry_samples = limitList(
    [
      ...samples,
      {
        timestamp: now / 1000,
        speed_ref: Number(status.speed_ref || 0),
        id_fbk: Number(status.id_fbk || 0),
        iq_fbk: Number(status.iq_fbk || 0),
        vdc_bus: Number(status.vdc_bus || 0),
        current_as: Number(status.current_as || 0),
        current_bs: Number(status.current_bs || 0),
        current_cs: Number(status.current_cs || 0),
      },
    ],
    300,
  );
  renderCharts(state.snapshot.telemetry_samples);
}

function queueFrameRender({ forceFull = false } = {}) {
  state.frameListDirty = true;
  if (forceFull) {
    state.uartNeedsFullRender = true;
  }
  if (state.framePaused || state.activeTab !== "uart") {
    return;
  }
  if (state.frameRenderTimer !== null) {
    return;
  }
  state.frameRenderTimer = window.setTimeout(() => {
    state.frameRenderTimer = null;
    if (!state.framePaused && state.activeTab === "uart") {
      renderUartTerminal(state.snapshot?.recent_frames || [], { forceFull: state.uartNeedsFullRender });
    }
  }, LOG_RENDER_INTERVAL_MS);
}

function queueEventRender() {
  state.eventListDirty = true;
  if (state.eventPaused || state.activeTab !== "events") {
    return;
  }
  if (state.eventRenderTimer !== null) {
    return;
  }
  state.eventRenderTimer = window.setTimeout(() => {
    state.eventRenderTimer = null;
    if (!state.eventPaused && state.activeTab === "events") {
      renderEvents(state.snapshot?.recent_events || []);
    }
  }, LOG_RENDER_INTERVAL_MS);
}

function renderCharts(samples) {
  const safeSamples = samples.length ? samples : [{ speed_ref: 0, id_fbk: 0, iq_fbk: 0, vdc_bus: 0, current_as: 0, current_bs: 0, current_cs: 0 }];
  renderSparkline(els["chart-speed"], [
    { values: safeSamples.map((sample) => Number(sample.speed_ref || 0)), color: "#f18748" },
  ]);
  renderSparkline(els["chart-dq"], [
    { values: safeSamples.map((sample) => Number(sample.id_fbk || 0)), color: "#44d1c2" },
    { values: safeSamples.map((sample) => Number(sample.iq_fbk || 0)), color: "#ffc857" },
  ]);
  renderSparkline(els["chart-vdc"], [
    { values: safeSamples.map((sample) => Number(sample.vdc_bus || 0)), color: "#9de7ff" },
  ]);
  renderSparkline(els["chart-phase"], [
    { values: safeSamples.map((sample) => Number(sample.current_as || 0)), color: "#44d1c2" },
    { values: safeSamples.map((sample) => Number(sample.current_bs || 0)), color: "#f18748" },
    { values: safeSamples.map((sample) => Number(sample.current_cs || 0)), color: "#ff6b6b" },
  ]);
}

function renderSparkline(svg, series) {
  const width = 320;
  const height = 120;
  const padding = 12;
  const allValues = series.flatMap((item) => item.values);
  const min = Math.min(...allValues, 0);
  const max = Math.max(...allValues, 0);
  const range = max - min || 1;

  const baselineY = height - padding - ((0 - min) / range) * (height - padding * 2);
  const paths = series
    .map(({ values, color }) => {
      const path = values
        .map((value, index) => {
          const x = padding + (index / Math.max(values.length - 1, 1)) * (width - padding * 2);
          const y = height - padding - ((value - min) / range) * (height - padding * 2);
          return `${index === 0 ? "M" : "L"}${x.toFixed(1)} ${y.toFixed(1)}`;
        })
        .join(" ");
      return `<path d="${path}" fill="none" stroke="${color}" stroke-width="3" stroke-linecap="round" />`;
    })
    .join("");

  svg.innerHTML = `
    <rect x="1" y="1" width="${width - 2}" height="${height - 2}" rx="18" ry="18" fill="rgba(255,255,255,0.02)" stroke="rgba(255,255,255,0.06)" />
    <line x1="${padding}" y1="${baselineY.toFixed(1)}" x2="${width - padding}" y2="${baselineY.toFixed(1)}" stroke="rgba(255,255,255,0.16)" stroke-dasharray="4 6" />
    ${paths}
  `;
}

function createLogItem({ title, timestamp, body, extra }) {
  return `
    <article class="log-item">
      <div class="log-meta">
        <span class="log-title">${title}</span>
        <span>${formatTimestamp(timestamp)}</span>
      </div>
      <div class="log-body">${escapeHtml(body)}</div>
      ${extra ? `<div class="log-code">${escapeHtml(extra)}</div>` : ""}
    </article>
  `;
}

function rebuildUartTerminal(frames) {
  const filteredFrames = getFilteredUartFrames(frames);
  if (!filteredFrames.length) {
    els["uart-terminal"].innerHTML = `<div class="placeholder">No UART frames match the current filters.</div>`;
    updateUartRenderTracking(frames);
    return;
  }

  els["uart-terminal"].innerHTML = filteredFrames
    .map((item) => createUartLine(item.frame, item.previousFrame))
    .join("");
  updateUartRenderTracking(frames);
}

function appendUartRows(newFrames, allFrames) {
  const fragment = newFrames
    .map((frame, offset) => {
      const absoluteIndex = state.uartRenderedFrameCount + offset;
      const previousFrame = absoluteIndex > 0 ? allFrames[absoluteIndex - 1] : null;
      return createUartLine(frame, previousFrame);
    })
    .join("");

  if (els["uart-terminal"].querySelector(".placeholder")) {
    els["uart-terminal"].innerHTML = "";
  }
  els["uart-terminal"].insertAdjacentHTML("beforeend", fragment);
}

function createUartLine(frame, previousFrame) {
  const deltaMs = previousFrame ? Math.max(0, (Number(frame.timestamp || 0) - Number(previousFrame.timestamp || 0)) * 1000) : null;
  const rawBytes = getRawBytes(frame.raw_hex);
  const checksumLabel = frame.checksum_ok ? "Checksum OK" : "Checksum error";
  const direction = String(frame.direction || "?").toLowerCase();
  const frameKey = createFrameKey(frame);
  const lineClasses = ["uart-line", direction];
  if (!frame.checksum_ok || frame.decoded?.error) {
    lineClasses.push("error");
  }

  return `
    <button type="button" class="${lineClasses.join(" ")}" data-frame-key="${escapeHtml(frameKey)}">
      <div class="uart-line-meta">
        <div class="uart-line-header">
          <span class="uart-line-title">${escapeHtml(frame.frame_name || `frame_${frame.frame_id}`)}</span>
          <span class="uart-line-badge ${direction}">${escapeHtml(direction.toUpperCase())}</span>
          <span class="uart-line-badge ${frame.checksum_ok ? "ok" : "error"}">${escapeHtml(checksumLabel)}</span>
          <span class="uart-line-badge">${escapeHtml(`0x${Number(frame.frame_id || 0).toString(16).padStart(2, "0")}`)}</span>
          <span class="uart-line-badge">${escapeHtml(`${rawBytes.length} bytes`)}</span>
        </div>
        <div class="uart-line-time">${escapeHtml(`${formatTimestamp(frame.timestamp)}${deltaMs === null ? "" : ` · +${deltaMs.toFixed(1)} ms`}`)}</div>
      </div>
      <div class="uart-line-preview raw">${escapeHtml(compactRawHex(frame.raw_hex))}</div>
      <div class="uart-line-preview decoded">${escapeHtml(compactDecoded(frame.decoded))}</div>
    </button>
  `;
}

function getFilteredUartFrames(frames) {
  return frames
    .map((frame, index) => ({
      frame,
      previousFrame: index > 0 ? frames[index - 1] : null,
    }))
    .filter(({ frame }) => frameMatchesUartFilters(frame));
}

function frameMatchesUartFilters(frame) {
  const filters = state.uartFilters;
  const direction = String(frame.direction || "").toLowerCase();
  if (direction === "tx" && !filters.tx) {
    return false;
  }
  if (direction === "rx" && !filters.rx) {
    return false;
  }
  if (filters.errorsOnly && frame.checksum_ok && !frame.decoded?.error) {
    return false;
  }
  if (!filters.search) {
    return true;
  }
  const haystack = JSON.stringify([
    frame.frame_name || "",
    frame.frame_id || "",
    frame.raw_hex || "",
    frame.decoded || {},
  ]).toLowerCase();
  return haystack.includes(filters.search);
}

function canIncrementallyAppendUart(frames) {
  if (state.uartNeedsFullRender || !isDefaultUartView()) {
    return false;
  }
  if (!state.uartRenderedFrameCount) {
    return false;
  }
  if (frames.length <= state.uartRenderedFrameCount) {
    return false;
  }
  return createFrameKey(frames[0]) === state.uartRenderedFirstKey;
}

function isDefaultUartView() {
  const filters = state.uartFilters;
  return filters.tx && filters.rx && !filters.errorsOnly && !filters.search;
}

function updateUartRenderTracking(frames) {
  state.uartRenderedFrameCount = frames.length;
  state.uartRenderedFirstKey = frames.length ? createFrameKey(frames[0]) : null;
}

function selectUartFrame(frameKey) {
  state.uartSelectedFrameKey = frameKey || null;
  renderUartInspector(findSelectedUartFrame(state.snapshot?.recent_frames || []));
  syncSelectedUartRow();
}

function findSelectedUartFrame(frames) {
  if (!state.uartSelectedFrameKey) {
    return null;
  }
  return frames.find((frame) => createFrameKey(frame) === state.uartSelectedFrameKey) || null;
}

function syncSelectedUartRow() {
  els["uart-terminal"].querySelectorAll("[data-frame-key]").forEach((node) => {
    node.classList.toggle("selected", node.dataset.frameKey === state.uartSelectedFrameKey);
  });
}

function renderUartInspector(frame) {
  if (!frame) {
    els["uart-inspector"].innerHTML = `<div class="placeholder">Select a UART frame to inspect its decoded fields and byte layout.</div>`;
    return;
  }

  const rawBytes = getRawBytes(frame.raw_hex);
  const ascii = rawBytes.map((byte) => (byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : ".")).join("");
  const payloadBytes = rawBytes.slice(2, -1);
  const direction = String(frame.direction || "?").toUpperCase();

  els["uart-inspector"].innerHTML = `
    <div class="uart-inspector-meta">
      <article class="uart-inspector-card">
        <span>Direction</span>
        <strong>${escapeHtml(direction)}</strong>
      </article>
      <article class="uart-inspector-card">
        <span>Frame</span>
        <strong>${escapeHtml(`${frame.frame_name || "frame"} (0x${Number(frame.frame_id || 0).toString(16).padStart(2, "0")})`)}</strong>
      </article>
      <article class="uart-inspector-card">
        <span>Timestamp</span>
        <strong>${escapeHtml(formatTimestamp(frame.timestamp))}</strong>
      </article>
      <article class="uart-inspector-card">
        <span>Status</span>
        <strong>${escapeHtml(frame.checksum_ok ? "Checksum OK" : "Checksum error")}</strong>
      </article>
    </div>

    <article class="uart-inspector-block">
      <h3>Byte Breakdown</h3>
      <div class="byte-breakdown">
        <div class="byte-breakdown-row">
          <span>Sync</span>
          <div class="byte-segment">${renderByteTokens(rawBytes.slice(0, 1), "sync")}</div>
        </div>
        <div class="byte-breakdown-row">
          <span>Frame ID</span>
          <div class="byte-segment">${renderByteTokens(rawBytes.slice(1, 2), "frame-id")}</div>
        </div>
        <div class="byte-breakdown-row">
          <span>Payload</span>
          <div class="byte-segment">${renderByteTokens(payloadBytes, "")}</div>
        </div>
        <div class="byte-breakdown-row">
          <span>Checksum</span>
          <div class="byte-segment">${renderByteTokens(rawBytes.slice(-1), "checksum")}</div>
        </div>
      </div>
    </article>

    <article class="uart-inspector-block">
      <h3>Decoded JSON</h3>
      <pre>${escapeHtml(JSON.stringify(frame.decoded || {}, null, 2))}</pre>
    </article>

    <article class="uart-inspector-block">
      <h3>Raw Hex</h3>
      <pre>${escapeHtml(frame.raw_hex || "")}</pre>
    </article>

    <article class="uart-inspector-block">
      <h3>ASCII Preview</h3>
      <pre>${escapeHtml(ascii || "(non-printable)")}</pre>
    </article>
  `;
}

function renderByteTokens(bytes, tokenClass) {
  if (!bytes.length) {
    return `<span class="mono-muted">none</span>`;
  }
  return bytes
    .map((byte) => `<span class="byte-token ${tokenClass}">${escapeHtml(byte.toString(16).padStart(2, "0"))}</span>`)
    .join("");
}

function createFrameKey(frame) {
  return [
    Number(frame.timestamp || 0).toFixed(6),
    frame.direction || "?",
    frame.frame_id || 0,
    frame.raw_hex || "",
  ].join("|");
}

function compactRawHex(rawHex, limit = 56) {
  const safe = String(rawHex || "").trim();
  return safe.length > limit ? `${safe.slice(0, limit)}...` : safe;
}

function compactDecoded(decoded, limit = 120) {
  const safe = JSON.stringify(decoded || {});
  return safe.length > limit ? `${safe.slice(0, limit)}...` : safe;
}

function getRawBytes(rawHex) {
  if (!rawHex) {
    return [];
  }
  return String(rawHex)
    .trim()
    .split(/\s+/)
    .filter(Boolean)
    .map((token) => Number.parseInt(token, 16))
    .filter((value) => Number.isFinite(value));
}

function scrollUartTerminalToBottom() {
  els["uart-terminal"].scrollTop = els["uart-terminal"].scrollHeight;
}

function positionStick(node, vectorNode, x, y) {
  const safeX = Math.max(-1, Math.min(1, Number(x || 0)));
  const safeY = Math.max(-1, Math.min(1, Number(y || 0)));
  node.style.left = `${50 + safeX * 34}%`;
  node.style.top = `${50 + safeY * 34}%`;

  if (!vectorNode) {
    return;
  }

  const surface = node.parentElement;
  const radius = Math.min(surface?.clientWidth || 0, surface?.clientHeight || 0) * 0.34;
  const dx = safeX * radius;
  const dy = safeY * radius;
  const length = Math.sqrt(dx ** 2 + dy ** 2);
  const angle = Math.atan2(dy, dx) * (180 / Math.PI);

  vectorNode.style.width = `${length}px`;
  vectorNode.style.opacity = length > 3 ? "1" : "0";
  vectorNode.style.transform = `translateY(-50%) rotate(${angle}deg)`;
}

function normalizeTrigger(value) {
  return Math.max(0, Math.min(100, ((Number(value || 0) + 1) / 2) * 100));
}

function formatTimestamp(timestamp) {
  if (!timestamp) {
    return "-";
  }
  return new Date(timestamp * 1000).toLocaleTimeString();
}

function formatSigned(value, digits = 2) {
  const safe = Number(value || 0);
  return `${safe >= 0 ? "+" : ""}${safe.toFixed(digits)}`;
}

function formatFixed(value, digits = 2) {
  return Number(value || 0).toFixed(digits);
}

function formatTripFlag(value) {
  const safe = Number(value || 0);
  return `0x${safe.toString(16).padStart(4, "0")}`;
}

function limitList(list, size) {
  return list.slice(Math.max(list.length - size, 0));
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}

function showTransientError(message) {
  if (!state.snapshot) {
    state.snapshot = { health: {} };
  }
  state.snapshot.last_error = message;
  state.snapshot.health = { ...(state.snapshot.health || {}), last_error: message };
  renderHealth(state.snapshot);
}
