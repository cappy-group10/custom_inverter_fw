from fastapi.testclient import TestClient

from xbox_controller.dashboard import create_app
from xbox_controller.runtime_models import MusicCommandRecord, MusicSongOption, MusicState, MusicStatusRecord, SessionSnapshot, to_payload


class MusicManagerStub:
    def __init__(self):
        self.callback = None
        self.snapshot = SessionSnapshot(
            session_state="idle",
            mode="drive",
            health={"terminal_only": True, "has_mcu_telemetry": False, "telemetry_stale": False},
        )

    def set_event_callback(self, callback):
        self.callback = callback

    def set_logger(self, _logger):
        return None

    async def start_async(self, port=None, baudrate=115200, joystick_index=0, mode="drive"):
        del joystick_index
        self.snapshot.session_state = "running"
        self.snapshot.mode = mode
        self.snapshot.port = None if port in (None, "", "demo") else port
        self.snapshot.baudrate = baudrate
        self.snapshot.started_at = 123.0
        self.snapshot.health = {
            "terminal_only": self.snapshot.port is None,
            "has_mcu_telemetry": mode == "music",
            "telemetry_stale": False,
            "last_frame_at": 123.0,
            "last_status_at": 123.0,
            "last_error": None,
        }
        self.snapshot.counters = {"tx_frames": 1, "rx_frames": 1, "checksum_errors": 0, "serial_errors": 0}
        if mode == "music":
            self.snapshot.music_state = MusicState(
                songs=[
                    MusicSongOption(song_id=0, label="Mario"),
                    MusicSongOption(song_id=1, label="Megalovania"),
                    MusicSongOption(song_id=2, label="Jingle Bells"),
                ],
                selected_song_id=0,
                volume=0.2,
                play_state="IDLE",
                play_mode="SONG",
            )
        else:
            self.snapshot.music_state = None

    async def stop_async(self):
        self.snapshot.session_state = "stopped"
        return self.snapshot

    def request_shutdown(self):
        return None

    def get_snapshot(self):
        return self.snapshot

    def play_music(self, song_id: int, amplitude: float | None = None):
        self._require_music()
        volume = 0.2 if amplitude is None else amplitude
        self.snapshot.music_state = MusicState(
            **{
                **to_payload(self.snapshot.music_state),
                "selected_song_id": song_id,
                "last_started_song_id": song_id,
                "volume": volume,
                "last_command": MusicCommandRecord(command_type="song", song_id=song_id, song_label="Megalovania", amplitude=volume, timestamp=124.0),
                "latest_status": MusicStatusRecord(
                    play_state="PLAYING",
                    play_mode="SONG",
                    song_id=song_id,
                    note_index=1,
                    note_total=16,
                    current_freq_hz=440.0,
                    amplitude=volume,
                    isr_ticker=17,
                ),
                "play_state": "PLAYING",
                "play_mode": "SONG",
                "note_index": 1,
                "note_total": 16,
                "current_freq_hz": 440.0,
                "amplitude": volume,
                "isr_ticker": 17,
            }
        )
        return self.snapshot

    def pause_music(self):
        self._require_music()
        self.snapshot.music_state = MusicState(**{**to_payload(self.snapshot.music_state), "play_state": "PAUSED"})
        return self.snapshot

    def resume_music(self):
        self._require_music()
        self.snapshot.music_state = MusicState(**{**to_payload(self.snapshot.music_state), "play_state": "PLAYING"})
        return self.snapshot

    def stop_music(self):
        self._require_music()
        self.snapshot.music_state = MusicState(**{**to_payload(self.snapshot.music_state), "play_state": "IDLE", "note_index": 0, "note_total": 0, "current_freq_hz": 0.0})
        return self.snapshot

    def set_music_volume(self, volume: float):
        self._require_music()
        self.snapshot.music_state = MusicState(**{**to_payload(self.snapshot.music_state), "volume": volume, "amplitude": volume})
        return self.snapshot

    def engage_brake_override(self):
        raise RuntimeError("Brake override is only available in drive mode")

    def release_brake_override(self):
        raise RuntimeError("Brake override is only available in drive mode")

    def _require_music(self):
        if self.snapshot.mode != "music":
            raise RuntimeError("Music playback requires an active music session")


def test_dashboard_music_session_and_endpoints():
    runtime = MusicManagerStub()
    with TestClient(create_app(runtime=runtime)) as client:
        started = client.post(
            "/api/session/start",
            json={"port": "demo", "baudrate": 115200, "joystick_index": 0, "mode": "music"},
        )
        assert started.status_code == 200
        payload = started.json()
        assert payload["mode"] == "music"
        assert payload["music_state"]["songs"][0]["label"] == "Mario"

        detail = client.get("/api/mcus/primary/music")
        assert detail.status_code == 200
        assert detail.json()["music_state"]["play_state"] == "IDLE"

        played = client.post("/api/mcus/primary/music/play", json={"song_id": 1, "amplitude": 0.3})
        assert played.status_code == 200
        assert played.json()["music_state"]["last_started_song_id"] == 1
        assert played.json()["music_state"]["play_state"] == "PLAYING"

        paused = client.post("/api/mcus/primary/music/pause")
        assert paused.status_code == 200
        assert paused.json()["music_state"]["play_state"] == "PAUSED"

        volume = client.post("/api/mcus/primary/music/volume", json={"volume": 0.45})
        assert volume.status_code == 200
        assert volume.json()["music_state"]["volume"] == 0.45

        brake = client.post("/api/mcus/primary/brake")
        assert brake.status_code == 409
        assert "drive mode" in brake.json()["detail"].lower()


def test_dashboard_music_websocket_snapshot_includes_music_state():
    runtime = MusicManagerStub()
    with TestClient(create_app(runtime=runtime)) as client:
        client.post(
            "/api/session/start",
            json={"port": "demo", "baudrate": 115200, "joystick_index": 0, "mode": "music"},
        )

        with client.websocket_connect("/api/stream") as websocket:
            initial = websocket.receive_json()
            assert initial["type"] == "snapshot"
            assert initial["payload"]["mode"] == "music"
            assert initial["payload"]["music_state"]["songs"][1]["label"] == "Megalovania"
