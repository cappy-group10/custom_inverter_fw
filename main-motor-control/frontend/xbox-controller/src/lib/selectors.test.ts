import { describe, expect, test, vi } from "vitest";

import { createEmptySnapshot, mergeStreamEvent } from "./selectors";
import type { SessionSnapshot } from "./types";

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
    recent_faults: overrides.recent_faults ?? base.recent_faults,
    recent_frames: overrides.recent_frames ?? base.recent_frames,
    recent_events: overrides.recent_events ?? base.recent_events,
    telemetry_samples: overrides.telemetry_samples ?? base.telemetry_samples,
  };
}

describe("mergeStreamEvent", () => {
  test("merges ui_tick payloads without double-counting counters", () => {
    const lastChartSampleMsRef = { current: 0 };
    const previous = buildSnapshot({
      recent_frames: [
        {
          direction: "tx",
          frame_id: 0x01,
          frame_name: "motor_cmd",
          raw_hex: "aa 01",
          decoded: { ctrl_state: "RUN" },
          checksum_ok: true,
          timestamp: 10,
        },
      ],
      recent_faults: [{ trip_flag: 1, trip_count: 1 }],
      recent_events: [
        {
          kind: "session",
          title: "Running",
          message: "Drive control loop is active",
          timestamp: 10,
          data: {},
        },
      ],
      counters: {
        tx_frames: 4,
        rx_frames: 7,
        checksum_errors: 1,
        serial_errors: 0,
        loop_iterations: 11,
        status_frames: 6,
        fault_frames: 1,
      },
    });

    vi.spyOn(Date, "now").mockReturnValue(123400);

    const next = mergeStreamEvent(
      previous,
      "ui_tick",
      {
        controller_state: { buttons: { a: true } },
        last_host_command: { ctrl_state: "RUN", speed_ref: 0.05 },
        latest_mcu_status: { ctrl_state: "RUN", speed_ref: 0.05, iq_fbk: 0.12, vdc_bus: 36.5 },
        health: {
          terminal_only: false,
          has_mcu_telemetry: true,
          telemetry_stale: false,
          last_frame_at: 12.5,
          last_status_at: 12.5,
        },
        counters: {
          tx_frames: 5,
          rx_frames: 9,
          checksum_errors: 1,
          serial_errors: 0,
        },
        new_frames: [
          {
            direction: "tx",
            frame_id: 0x01,
            frame_name: "motor_cmd",
            raw_hex: "aa 01 01",
            decoded: { ctrl_state: "RUN" },
            checksum_ok: true,
            timestamp: 12.0,
          },
          {
            direction: "rx",
            frame_id: 0x10,
            frame_name: "status",
            raw_hex: "55 10",
            decoded: { ctrl_state: "RUN" },
            checksum_ok: true,
            timestamp: 12.5,
          },
        ],
        new_faults: [{ trip_flag: 2, trip_count: 2 }],
        new_events: [
          {
            kind: "fault",
            title: "MCU Fault",
            message: "trip",
            timestamp: 12.5,
            data: { trip_flag: 2 },
          },
        ],
      },
      lastChartSampleMsRef,
    );

    expect(next.counters.tx_frames).toBe(5);
    expect(next.counters.rx_frames).toBe(9);
    expect(next.counters.checksum_errors).toBe(1);
    expect(next.recent_frames).toHaveLength(3);
    expect(next.recent_faults).toHaveLength(2);
    expect(next.recent_events).toHaveLength(2);
    expect(next.health.last_frame_at).toBe(12.5);
    expect(next.telemetry_samples).toHaveLength(1);
    expect(next.telemetry_samples[0]?.speed_ref).toBeCloseTo(0.05);
  });
});
