import time
from copy import deepcopy

from xbox_controller.music_runtime import MusicRuntime
from xbox_controller.music_uart import CTRL_ACTION_PAUSE, CTRL_ACTION_RESUME, CTRL_ACTION_STOP, MusicUARTCounters, MusicUARTHealth
from xbox_controller.runtime_models import FrameRecord, MusicStatusRecord, to_payload


class FakeMusicLink:
    def __init__(self, port=None, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self._frames = []
        self._statuses = []
        self._counters = MusicUARTCounters()
        self._health = MusicUARTHealth(terminal_only=port is None)
        self._current_status = MusicStatusRecord()

    def open(self):
        self._health.port_open = self.port is not None

    def close(self):
        self._health.port_open = False

    def _record_tx(self, frame_name: str, frame_id: int, decoded: dict):
        now = time.time()
        self._counters.tx_frames += 1
        self._health.last_frame_at = now
        self._frames.append(FrameRecord("tx", frame_id, frame_name, "aa", decoded, True, now))
        return now

    def _record_status(self, status: MusicStatusRecord, now: float):
        self._current_status = status
        self._statuses.append(status)
        self._frames.append(FrameRecord("rx", 0x30, "status", "55 30", to_payload(status), True, now))
        self._counters.rx_frames += 1
        self._counters.status_frames += 1
        self._health.last_status_at = now
        self._health.last_frame_at = now

    def send_song(self, song_id: int, amplitude: float):
        now = self._record_tx("song_cmd", 0x20, {"song_id": song_id, "amplitude": amplitude})
        self._record_status(
            MusicStatusRecord(
                play_state="PLAYING",
                play_mode="SONG",
                song_id=song_id,
                note_index=1,
                note_total=16,
                current_freq_hz=440.0 + song_id,
                amplitude=amplitude,
                isr_ticker=11,
            ),
            now,
        )

    def send_control(self, action: int):
        now = self._record_tx("ctrl_cmd", 0x22, {"action": action})
        next_state = self._current_status
        if action == CTRL_ACTION_PAUSE:
            next_state = MusicStatusRecord(**{**to_payload(self._current_status), "play_state": "PAUSED"})
        elif action == CTRL_ACTION_RESUME:
            next_state = MusicStatusRecord(**{**to_payload(self._current_status), "play_state": "PLAYING"})
        elif action == CTRL_ACTION_STOP:
            next_state = MusicStatusRecord(**{**to_payload(self._current_status), "play_state": "IDLE", "note_index": 0, "note_total": 0, "current_freq_hz": 0.0})
        self._record_status(next_state, now)

    def send_volume(self, volume: float):
        now = self._record_tx("vol_cmd", 0x23, {"volume": volume})
        self._record_status(
            MusicStatusRecord(**{**to_payload(self._current_status), "amplitude": volume}),
            now,
        )

    def pop_frame_records(self):
        frames = list(self._frames)
        self._frames.clear()
        return frames

    def pop_statuses(self):
        statuses = list(self._statuses)
        self._statuses.clear()
        return statuses

    def get_counters(self):
        return deepcopy(self._counters)

    def get_health(self):
        return deepcopy(self._health)


def test_music_runtime_lifecycle_and_control_commands():
    runtime = MusicRuntime(link_factory=FakeMusicLink)

    runtime.open(port="demo", baudrate=115200)
    opened = runtime.get_snapshot()

    assert opened.mode == "music"
    assert opened.controller_connected is False
    assert opened.music_state is not None
    assert opened.music_state.selected_song_id == 0

    played = runtime.play_song(1, amplitude=0.3)
    assert played.music_state is not None
    assert played.music_state.last_started_song_id == 1
    assert played.music_state.play_state == "PLAYING"
    assert played.music_state.latest_status is not None
    assert played.music_state.latest_status.song_id == 1

    paused = runtime.pause()
    assert paused.music_state is not None
    assert paused.music_state.play_state == "PAUSED"

    resumed = runtime.resume()
    assert resumed.music_state is not None
    assert resumed.music_state.play_state == "PLAYING"

    volume_updated = runtime.set_volume(0.45)
    assert volume_updated.music_state is not None
    assert volume_updated.music_state.volume == 0.45
    assert volume_updated.music_state.amplitude == 0.45

    stopped = runtime.stop_playback()
    assert stopped.music_state is not None
    assert stopped.music_state.play_state == "IDLE"
    assert stopped.counters["tx_frames"] >= 4

    final = runtime.stop()
    assert final.session_state == "stopped"
