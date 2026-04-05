import { cleanup, render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { afterEach, describe, expect, test, vi } from "vitest";

import { MotorControlPanel } from "./MotorControlPanel";
import { createEmptySnapshot } from "../lib/selectors";
import type { SessionSnapshot } from "../lib/types";

function buildSnapshot(overrides: Partial<SessionSnapshot> = {}): SessionSnapshot {
  const base = createEmptySnapshot();
  return {
    ...base,
    ...overrides,
    motor_config: {
      ...base.motor_config,
      ...(overrides.motor_config || {}),
    },
    health: {
      ...base.health,
      ...(overrides.health || {}),
    },
    counters: {
      ...base.counters,
      ...(overrides.counters || {}),
    },
    telemetry_samples: overrides.telemetry_samples ?? base.telemetry_samples,
  };
}

describe("MotorControlPanel", () => {
  afterEach(() => {
    cleanup();
  });

  test("renders the compact dedicated layout with the horizontal speed band", () => {
    const snapshot = buildSnapshot({
      session_state: "running",
      active_override: "BRAKE",
      port: "/dev/cu.usbmodem123401",
      motor_config: {
        base_speed_rpm: 3000,
        base_current_a: 5,
        vdcbus_min_v: 24,
        vdcbus_max_v: 600,
        rated_input_power_w: 3000,
      },
      last_host_command: {
        ctrl_state: "RUN",
        speed_ref: 0.05,
        id_ref: 0.2,
        iq_ref: 0.1,
      },
      latest_mcu_status: {
        run_motor: 1,
        ctrl_state: "RUN",
        trip_flag: 2,
        speed_ref: 0.05,
        speed_fbk: 0.04,
        vdc_bus: 650,
        id_fbk: 0.3,
        iq_fbk: 0.4,
        current_as: 0.7,
        current_bs: -0.2,
        current_cs: -0.5,
        pos_mech_theta: 0.12,
      },
      health: {
        terminal_only: false,
        has_mcu_telemetry: true,
        telemetry_stale: false,
        last_frame_at: 10,
        last_status_at: 10,
      },
      telemetry_samples: [
        {
          timestamp: 1,
          speed_ref: 0.05,
          speed_fbk: 0.04,
          id_ref: 0.2,
          id_fbk: 0.3,
          iq_ref: 0.1,
          iq_fbk: 0.4,
          vdc_bus: 650,
          current_as: 0.7,
          current_bs: -0.2,
          current_cs: -0.5,
        },
      ],
    });

    render(
      <MemoryRouter
        future={{
          v7_startTransition: true,
          v7_relativeSplatPath: true,
        }}
      >
        <MotorControlPanel
          snapshot={snapshot}
          loadingBrake={false}
          onBrake={vi.fn()}
          onBrakeRelease={vi.fn()}
          dedicatedPage
        />
      </MemoryRouter>,
    );

    expect(screen.getByTestId("compact-speed-band")).toBeInTheDocument();
    expect(screen.queryByTestId("speedometer")).not.toBeInTheDocument();
    expect(screen.getAllByText("150 RPM").length).toBeGreaterThan(0);
    expect(screen.getAllByText("120 RPM").length).toBeGreaterThan(0);
    expect(screen.getByRole("button", { name: /unlatch brake \(send stop\)/i })).toBeInTheDocument();
    expect(screen.getByText("Out of range")).toBeInTheDocument();
    expect(screen.getAllByText("Pending source")).toHaveLength(3);
    expect(screen.getByText("1625 W")).toBeInTheDocument();
  });

  test("keeps the circular speedometer in the shared dashboard layout", () => {
    const snapshot = buildSnapshot({
      session_state: "running",
      last_host_command: {
        ctrl_state: "RUN",
        speed_ref: 0.05,
        id_ref: 0.1,
        iq_ref: 0.08,
      },
      latest_mcu_status: {
        run_motor: 1,
        ctrl_state: "RUN",
        trip_flag: 0,
        speed_ref: 0.05,
        speed_fbk: 0.04,
        vdc_bus: 40,
        id_fbk: 0.02,
        iq_fbk: 0.03,
        current_as: 0.1,
        current_bs: -0.05,
        current_cs: -0.05,
        pos_mech_theta: 0.12,
      },
      health: {
        terminal_only: false,
        has_mcu_telemetry: true,
        telemetry_stale: false,
        last_frame_at: 10,
        last_status_at: 10,
      },
    });

    render(
      <MemoryRouter
        future={{
          v7_startTransition: true,
          v7_relativeSplatPath: true,
        }}
      >
        <MotorControlPanel snapshot={snapshot} loadingBrake={false} onBrake={vi.fn()} onBrakeRelease={vi.fn()} />
      </MemoryRouter>,
    );

    expect(screen.getByTestId("speedometer")).toBeInTheDocument();
    expect(screen.queryByTestId("compact-speed-band")).not.toBeInTheDocument();
  });
});
