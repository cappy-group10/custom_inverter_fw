import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";

import {
  createConnectionInstance,
  getConnectionInstance,
  updateConnectionInstanceFromSnapshot,
} from "./instances";

const STORAGE_KEY = "inverter-os.connection-instances.v1";

describe("connection instance persistence", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    window.localStorage.clear();
  });

  afterEach(() => {
    vi.useRealTimers();
    window.localStorage.clear();
  });

  test("throttles snapshot persistence and preserves last_opened_at", () => {
    vi.setSystemTime(new Date("2026-04-04T16:00:00.000Z"));
    const instance = createConnectionInstance("Bench Session");
    const initialLastOpenedAt = getConnectionInstance(instance.id)?.last_opened_at;

    vi.setSystemTime(new Date("2026-04-04T16:00:00.200Z"));
    updateConnectionInstanceFromSnapshot(instance.id, {
      session_state: "running",
      port: "/dev/cu.usbmodem123401",
      joystick_name: "Xbox Wireless Controller",
      health: {
        last_frame_at: 10,
      },
    });

    let stored = JSON.parse(window.localStorage.getItem(STORAGE_KEY) || "[]");
    expect(stored[0]?.session_state).toBeUndefined();
    expect(stored[0]?.port).toBeUndefined();

    vi.setSystemTime(new Date("2026-04-04T16:00:00.500Z"));
    updateConnectionInstanceFromSnapshot(instance.id, {
      session_state: "running",
      port: "/dev/cu.usbmodem123401",
      joystick_name: "Xbox Wireless Controller",
      health: {
        last_frame_at: 12,
      },
    });

    stored = JSON.parse(window.localStorage.getItem(STORAGE_KEY) || "[]");
    expect(stored[0]?.last_frame_at).toBeUndefined();

    vi.advanceTimersByTime(1000);

    const updated = getConnectionInstance(instance.id);
    expect(updated?.session_state).toBe("running");
    expect(updated?.port).toBe("/dev/cu.usbmodem123401");
    expect(updated?.controller_name).toBe("Xbox Wireless Controller");
    expect(updated?.last_frame_at).toBe(12);
    expect(updated?.last_opened_at).toBe(initialLastOpenedAt);
  });
});
