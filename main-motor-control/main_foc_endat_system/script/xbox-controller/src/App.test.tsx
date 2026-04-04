import { cleanup, render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { afterEach, describe, expect, test, vi } from "vitest";

import { App } from "./App";
import { DashboardContext, type DashboardContextValue } from "./context/DashboardContext";
import { createEmptySnapshot, derivePrimaryMcuDetail, derivePrimaryMcuSummary } from "./lib/selectors";
import type { SessionSnapshot } from "./lib/types";

function buildSnapshot(overrides: Partial<SessionSnapshot> = {}): SessionSnapshot {
  const base = createEmptySnapshot();
  return {
    ...base,
    ...overrides,
    health: {
      ...base.health,
      ...(overrides.health || {}),
    },
    counters: {
      ...base.counters,
      ...(overrides.counters || {}),
    },
    controller_layout: overrides.controller_layout ?? base.controller_layout,
    recent_faults: overrides.recent_faults ?? base.recent_faults,
    recent_frames: overrides.recent_frames ?? base.recent_frames,
    recent_events: overrides.recent_events ?? base.recent_events,
    telemetry_samples: overrides.telemetry_samples ?? base.telemetry_samples,
  };
}

function buildContext(snapshot: SessionSnapshot): DashboardContextValue {
  return {
    snapshot,
    ports: [
      {
        port: "demo",
        label: "Demo Mode (No MCU Serial)",
        description: "Use the Xbox controller without connecting the MCU UART",
        is_demo: true,
        category: "demo",
        recommended: false,
        possible_mcu_uart: false,
        usage: "Controller-only mode. Host TX runs without connecting the MCU serial port.",
      },
    ],
    wsConnected: true,
    loading: {
      ports: false,
      session: false,
      brake: false,
    },
    error: null,
    primaryMcu: derivePrimaryMcuSummary(snapshot),
    primaryMcuDetail: derivePrimaryMcuDetail(snapshot),
    reloadPorts: vi.fn(async () => undefined),
    refreshState: vi.fn(async () => undefined),
    startSession: vi.fn(async () => undefined),
    stopSession: vi.fn(async () => undefined),
    engageBrake: vi.fn(async () => undefined),
    clearError: vi.fn(),
  };
}

function renderRoute(route: string, snapshot: SessionSnapshot) {
  return render(
    <DashboardContext.Provider value={buildContext(snapshot)}>
      <MemoryRouter
        initialEntries={[route]}
        future={{
          v7_startTransition: true,
          v7_relativeSplatPath: true,
        }}
      >
        <App />
      </MemoryRouter>
    </DashboardContext.Provider>,
  );
}

describe("React dashboard routes", () => {
  afterEach(() => {
    cleanup();
  });

  test("renders the landing page on /", () => {
    renderRoute("/", buildSnapshot());

    expect(screen.getByRole("heading", { name: /inverter os/i })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /create connection instance/i })).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: /demo flow/i })).toBeInTheDocument();
  });

  test("renders the dashboard page on /configure", () => {
    renderRoute("/configure", buildSnapshot());

    expect(screen.getByRole("heading", { name: /inverter os/i })).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: /session control/i })).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: /transport health/i })).toBeInTheDocument();
  });

  test("renders the dedicated motor control page on /mcu/primary", () => {
    const snapshot = buildSnapshot({
      session_state: "running",
      started_at: 100,
      port: "/dev/cu.usbmodem123401",
      controller_connected: true,
      joystick_name: "Xbox One S Controller",
      last_host_command: {
        ctrl_state: "RUN",
        speed_ref: 0.16,
        id_ref: 0.01,
        iq_ref: 0.02,
      },
      latest_mcu_status: {
        ctrl_state: "RUN",
        speed_ref: 0.15,
        current_as: 1.2,
        current_bs: -0.6,
        current_cs: -0.6,
        vdc_bus: 38.4,
      },
      counters: {
        tx_frames: 24,
        rx_frames: 18,
        checksum_errors: 0,
        serial_errors: 0,
      },
      health: {
        terminal_only: false,
        has_mcu_telemetry: true,
        telemetry_stale: false,
        last_frame_at: 120,
        last_status_at: 120,
      },
    });

    renderRoute("/mcu/primary", snapshot);

    expect(screen.getByRole("heading", { level: 1, name: /inverter os motor control/i })).toBeInTheDocument();
    expect(screen.getByRole("heading", { level: 2, name: /motor control/i })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /emergency brake/i })).toBeInTheDocument();
    expect(screen.getByTestId("speedometer")).toBeInTheDocument();
  });
});
