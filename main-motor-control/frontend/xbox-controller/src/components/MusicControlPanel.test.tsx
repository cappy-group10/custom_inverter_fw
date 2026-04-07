import { cleanup, fireEvent, render, screen } from "@testing-library/react";
import { afterEach, describe, expect, test, vi } from "vitest";

import { MusicControlPanel } from "./MusicControlPanel";
import { createEmptySnapshot } from "../lib/selectors";
import type { SessionSnapshot } from "../lib/types";

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
    music_state: overrides.music_state ?? base.music_state,
  };
}

describe("MusicControlPanel", () => {
  afterEach(() => {
    cleanup();
    vi.useRealTimers();
  });

  test("renders the music session controls and debounces volume updates", () => {
    vi.useFakeTimers();
    const onPlay = vi.fn();
    const onPause = vi.fn();
    const onResume = vi.fn();
    const onStop = vi.fn();
    const onVolumeChange = vi.fn();
    const onOpenUart = vi.fn();

    const snapshot = buildSnapshot({
      mode: "music",
      session_state: "running",
      health: {
        terminal_only: false,
        has_mcu_telemetry: true,
        telemetry_stale: false,
        last_frame_at: 10,
        last_status_at: 10,
      },
      music_state: {
        songs: [
          { song_id: 0, label: "Mario" },
          { song_id: 1, label: "Megalovania" },
        ],
        selected_song_id: 1,
        last_started_song_id: 1,
        volume: 0.2,
        last_command: { command_type: "song", song_id: 1, song_label: "Megalovania", amplitude: 0.2, timestamp: 1 },
        latest_status: {
          play_state: "PAUSED",
          play_mode: "SONG",
          song_id: 1,
          note_index: 2,
          note_total: 16,
          current_freq_hz: 440,
          amplitude: 0.2,
          isr_ticker: 11,
        },
        play_state: "PAUSED",
        play_mode: "SONG",
        note_index: 2,
        note_total: 16,
        current_freq_hz: 440,
        amplitude: 0.2,
        isr_ticker: 11,
      },
    });

    render(
      <MusicControlPanel
        snapshot={snapshot}
        loadingMusic={false}
        onPlay={onPlay}
        onPause={onPause}
        onResume={onResume}
        onStop={onStop}
        onVolumeChange={onVolumeChange}
        onOpenUart={onOpenUart}
      />,
    );

    fireEvent.click(screen.getByRole("button", { name: /resume/i }));
    expect(onResume).toHaveBeenCalledTimes(1);
    expect(onPlay).not.toHaveBeenCalled();

    fireEvent.click(screen.getByRole("button", { name: /pause/i }));
    expect(onPause).toHaveBeenCalledTimes(1);

    fireEvent.click(screen.getByRole("button", { name: /stop/i }));
    expect(onStop).toHaveBeenCalledTimes(1);

    fireEvent.click(screen.getByRole("button", { name: /open uart debug/i }));
    expect(onOpenUart).toHaveBeenCalledTimes(1);

    fireEvent.change(screen.getByRole("slider"), { target: { value: "0.55" } });
    vi.advanceTimersByTime(99);
    expect(onVolumeChange).not.toHaveBeenCalled();
    vi.advanceTimersByTime(1);
    expect(onVolumeChange).toHaveBeenCalledWith(0.55);
  });

  test("shows a drive-mode placeholder when the music runtime is not active", () => {
    const snapshot = buildSnapshot({ mode: "drive", session_state: "running" });

    render(
      <MusicControlPanel
        snapshot={snapshot}
        loadingMusic={false}
        onPlay={vi.fn()}
        onPause={vi.fn()}
        onResume={vi.fn()}
        onStop={vi.fn()}
        onVolumeChange={vi.fn()}
        onOpenUart={vi.fn()}
      />,
    );

    expect(screen.getByText("Start a music session from Overview.")).toBeInTheDocument();
  });
});
