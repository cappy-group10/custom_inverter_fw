import { useEffect, useRef, useState } from "react";

import { InfoHint, UiIcon } from "./UiChrome";
import { compactDecoded, formatFixed, formatTimestamp } from "../lib/selectors";
import type { SessionSnapshot } from "../lib/types";

interface MusicControlPanelProps {
  snapshot: SessionSnapshot;
  loadingMusic: boolean;
  onPlay: (songId: number, amplitude?: number) => void;
  onPause: () => void;
  onResume: () => void;
  onStop: () => void;
  onVolumeChange: (volume: number) => void;
  onOpenUart: () => void;
}

function songLabel(snapshot: SessionSnapshot, songId: number | null | undefined) {
  const songs = snapshot.music_state?.songs || [];
  const match = songs.find((song) => song.song_id === songId);
  return match?.label || (songId === null || songId === undefined ? "None" : `Song ${songId}`);
}

export function MusicControlPanel({
  snapshot,
  loadingMusic,
  onPlay,
  onPause,
  onResume,
  onStop,
  onVolumeChange,
  onOpenUart,
}: MusicControlPanelProps) {
  const musicState = snapshot.music_state;
  const isMusicSession = snapshot.mode === "music";
  const isActiveSession = isMusicSession && ["starting", "running", "error"].includes(snapshot.session_state);
  const [selectedSongId, setSelectedSongId] = useState<number>(musicState?.selected_song_id ?? musicState?.songs?.[0]?.song_id ?? 0);
  const [volume, setVolume] = useState<number>(musicState?.volume ?? 0.2);
  const pendingVolumeRef = useRef<number | null>(null);

  useEffect(() => {
    if (musicState?.selected_song_id !== undefined && musicState?.selected_song_id !== null) {
      setSelectedSongId(musicState.selected_song_id);
    }
  }, [musicState?.selected_song_id]);

  useEffect(() => {
    if (typeof musicState?.volume === "number") {
      setVolume(musicState.volume);
    }
  }, [musicState?.volume]);

  useEffect(() => {
    if (!isActiveSession || pendingVolumeRef.current === null) {
      return;
    }
    const nextVolume = pendingVolumeRef.current;
    const timer = window.setTimeout(() => {
      pendingVolumeRef.current = null;
      onVolumeChange(nextVolume);
    }, 100);
    return () => window.clearTimeout(timer);
  }, [isActiveSession, onVolumeChange, volume]);

  const playState = musicState?.play_state || "IDLE";
  const canResume =
    playState === "PAUSED" &&
    musicState?.last_started_song_id !== null &&
    musicState?.last_started_song_id === selectedSongId;
  const playButtonLabel = canResume ? "Resume" : "Play";
  const activeSongLabel = songLabel(snapshot, musicState?.last_started_song_id ?? musicState?.selected_song_id ?? null);
  const noteProgress = musicState?.note_total ? `${musicState.note_index}/${musicState.note_total}` : "0/0";

  return (
    <section className="panel music-panel">
      <div className="panel-heading">
        <div>
          <div className="heading-line">
            <UiIcon name="music" className="heading-icon" />
            <h2>Musical Motor Control</h2>
            <InfoHint text="Play predefined songs, adjust musical-motor volume, and monitor the host-to-MCU music status path without disturbing the existing drive runtime." />
          </div>
          <p className="panel-copy">
            Predefined-song control for the musical motor firmware, with compact TX/RX visibility and live playback status.
          </p>
        </div>
        <span className={`badge ${isMusicSession ? "good" : "muted"}`}>{isMusicSession ? "Music session" : "Drive session active"}</span>
      </div>

      {!isMusicSession ? (
        <div className="music-placeholder">
          <strong>Start a music session from Overview.</strong>
          <span>The music tab is ready, but the active session is still in drive mode.</span>
        </div>
      ) : null}

      <div className="music-grid">
        <section className="music-column">
          <div className="music-controls">
            <label className="control-field control-field-wide">
              Song
              <select
                value={String(selectedSongId)}
                disabled={!isActiveSession || !musicState?.songs?.length}
                onChange={(event) => setSelectedSongId(Number(event.target.value))}
              >
                {(musicState?.songs || []).map((song) => (
                  <option key={song.song_id} value={song.song_id}>
                    {song.label}
                  </option>
                ))}
              </select>
            </label>

            <label className="control-field control-field-wide">
              Volume
              <div className="music-volume-row">
                <input
                  type="range"
                  min={0}
                  max={1}
                  step={0.01}
                  value={volume}
                  disabled={!isActiveSession}
                  onChange={(event) => {
                    const nextVolume = Number(event.target.value);
                    setVolume(nextVolume);
                    pendingVolumeRef.current = nextVolume;
                  }}
                />
                <strong>{formatFixed(volume, 2)}</strong>
              </div>
            </label>

            <div className="button-row music-action-row">
              <button
                className="primary"
                disabled={!isActiveSession || loadingMusic}
                onClick={() => {
                  if (canResume) {
                    onResume();
                    return;
                  }
                  onPlay(selectedSongId, volume);
                }}
              >
                <UiIcon name="open" />
                {loadingMusic ? "Working..." : playButtonLabel}
              </button>
              <button disabled={!isActiveSession || loadingMusic} onClick={onPause}>
                <UiIcon name="events" />
                Pause
              </button>
              <button disabled={!isActiveSession || loadingMusic} onClick={onStop}>
                <UiIcon name="delete" />
                Stop
              </button>
              <button onClick={onOpenUart}>
                <UiIcon name="uart" />
                Open UART Debug
              </button>
            </div>
          </div>

          <div className="music-status-grid">
            <article className="metric-card">
              <span>Play State</span>
              <strong>{playState}</strong>
            </article>
            <article className="metric-card">
              <span>Play Mode</span>
              <strong>{musicState?.play_mode || "SONG"}</strong>
            </article>
            <article className="metric-card">
              <span>Song</span>
              <strong>{activeSongLabel}</strong>
            </article>
            <article className="metric-card">
              <span>Note Progress</span>
              <strong>{noteProgress}</strong>
            </article>
            <article className="metric-card">
              <span>Current Freq</span>
              <strong>{`${formatFixed(musicState?.current_freq_hz, 1)} Hz`}</strong>
            </article>
            <article className="metric-card">
              <span>Amplitude</span>
              <strong>{formatFixed(musicState?.amplitude, 2)}</strong>
            </article>
            <article className="metric-card">
              <span>ISR Ticker</span>
              <strong>{musicState?.isr_ticker ?? 0}</strong>
            </article>
            <article className="metric-card">
              <span>Last Status</span>
              <strong>{formatTimestamp(snapshot.health.last_status_at)}</strong>
            </article>
          </div>
        </section>

        <aside className="music-sidebar">
          <div className="uart-summary-grid compact-grid">
            <div className="uart-summary-card"><span>TX</span><strong>{snapshot.counters.tx_frames ?? 0}</strong></div>
            <div className="uart-summary-card"><span>RX</span><strong>{snapshot.counters.rx_frames ?? 0}</strong></div>
            <div className="uart-summary-card"><span>Checksum Errors</span><strong>{snapshot.counters.checksum_errors ?? 0}</strong></div>
            <div className="uart-summary-card"><span>Serial Errors</span><strong>{snapshot.counters.serial_errors ?? 0}</strong></div>
            <div className="uart-summary-card wide"><span>Last Frame</span><strong>{formatTimestamp(snapshot.health.last_frame_at)}</strong></div>
          </div>

          <div className="music-log-card">
            <span>Last Outbound Command</span>
            <strong>{musicState?.last_command ? compactDecoded(musicState.last_command, 160) : "No command sent yet."}</strong>
          </div>

          <div className="music-log-card">
            <span>Latest Inbound Status</span>
            <strong>{musicState?.latest_status ? compactDecoded(musicState.latest_status, 160) : "No MCU status yet."}</strong>
          </div>
        </aside>
      </div>
    </section>
  );
}
