from types import SimpleNamespace

from fastapi.testclient import TestClient

import dashboard
from dashboard import create_app
from runtime_models import ControllerLayoutDescriptor, SessionSnapshot


class StubRuntime:
    def __init__(self):
        self.callback = None
        self.started = []
        self.stop_calls = 0
        self.snapshot = SessionSnapshot(
            session_state="idle",
            mode="drive",
            controller_layout=[
                ControllerLayoutDescriptor(
                    control_id="a",
                    label="A",
                    group="face_buttons",
                    mapped=True,
                    mapping_target="ctrl_state",
                    mapping_text="ctrl_state = RUN",
                )
            ],
            health={"terminal_only": True, "has_mcu_telemetry": False, "telemetry_stale": False},
        )

    def set_event_callback(self, callback):
        self.callback = callback

    async def start_async(self, port=None, baudrate=115200, joystick_index=0):
        self.started.append((port, baudrate, joystick_index))
        self.snapshot.session_state = "running"
        self.snapshot.port = None if port in (None, "", "demo") else port
        self.snapshot.baudrate = baudrate
        self.snapshot.joystick_index = joystick_index
        self.snapshot.health = {
            "terminal_only": self.snapshot.port is None,
            "has_mcu_telemetry": False,
            "telemetry_stale": False,
            "last_error": None,
        }

    async def stop_async(self):
        self.stop_calls += 1
        self.snapshot.session_state = "stopped"
        return self.snapshot

    def get_snapshot(self):
        return self.snapshot

    def emit(self, event):
        if self.callback is not None:
            self.callback(event)


def test_dashboard_api_start_stop_and_ports():
    runtime = StubRuntime()
    with TestClient(create_app(runtime=runtime)) as client:
        ports = client.get("/api/ports")
        assert ports.status_code == 200
        assert ports.json()["ports"][0]["port"] == "demo"
        assert ports.json()["ports"][0]["label"] == "Demo Mode (No MCU Serial)"
        state = client.get("/api/state")
        assert state.status_code == 200
        assert state.json()["controller_layout"][0]["mapping_target"] == "ctrl_state"

        started = client.post(
            "/api/session/start",
            json={"port": "demo", "baudrate": 57600, "joystick_index": 2},
        )
        assert started.status_code == 200
        payload = started.json()
        assert payload["session_state"] == "running"
        assert runtime.started[-1] == ("demo", 57600, 2)

        stopped = client.post("/api/session/stop")
        assert stopped.status_code == 200
        assert stopped.json()["session_state"] == "stopped"


def test_dashboard_websocket_streams_runtime_events():
    runtime = StubRuntime()
    with TestClient(create_app(runtime=runtime)) as client:
        with client.websocket_connect("/api/stream") as websocket:
            initial = websocket.receive_json()
            assert initial["type"] == "snapshot"
            assert initial["payload"]["session_state"] == "idle"

            runtime.emit(
                {
                    "type": "event",
                    "payload": {
                        "kind": "session",
                        "title": "Stub Event",
                        "message": "runtime emitted an event",
                        "timestamp": 123.0,
                        "data": {"ok": True},
                    },
                }
            )

            streamed = websocket.receive_json()
            assert streamed["type"] == "event"
            assert streamed["payload"]["title"] == "Stub Event"


def test_dashboard_shutdown_stops_active_runtime():
    runtime = StubRuntime()
    with TestClient(create_app(runtime=runtime)) as client:
        started = client.post(
            "/api/session/start",
            json={"port": "demo", "baudrate": 115200, "joystick_index": 0},
        )
        assert started.status_code == 200

    assert runtime.stop_calls == 1


def test_dashboard_port_labels_are_operator_friendly(monkeypatch):
    fake_ports = [
        SimpleNamespace(
            device="/dev/cu.wlan-debug",
            description="n/a",
            manufacturer="",
            hwid="",
        ),
        SimpleNamespace(
            device="/dev/cu.debug-console",
            description="n/a",
            manufacturer="",
            hwid="",
        ),
        SimpleNamespace(
            device="/dev/cu.Bluetooth-Incoming-Port",
            description="n/a",
            manufacturer="",
            hwid="",
        ),
    ]
    monkeypatch.setattr(dashboard.list_ports, "comports", lambda: fake_ports)

    payload = dashboard._list_ports_payload()
    labels = [item["label"] for item in payload["ports"]]
    descriptions = [item["description"] for item in payload["ports"]]

    assert labels == [
        "Demo Mode (No MCU Serial)",
        "macOS WLAN Debug Port (Not MCU UART)",
        "macOS Debug Console (Not MCU UART)",
        "macOS Bluetooth Incoming Port (Not MCU UART)",
    ]
    assert descriptions[1] == "Path: /dev/cu.wlan-debug"
    assert descriptions[2] == "Path: /dev/cu.debug-console"


def test_dashboard_port_labels_mark_launchxl_app_uart(monkeypatch):
    fake_ports = [
        SimpleNamespace(
            device="/dev/cu.usbmodem123401",
            description="XDS110 Class Application/User UART",
            manufacturer="Texas Instruments",
            hwid="USB VID:PID=0451:BEEF",
        ),
        SimpleNamespace(
            device="/dev/cu.usbmodem123400",
            description="XDS110 Class Auxiliary Data Port",
            manufacturer="Texas Instruments",
            hwid="USB VID:PID=0451:BEEF",
        ),
    ]
    monkeypatch.setattr(dashboard.list_ports, "comports", lambda: fake_ports)

    payload = dashboard._list_ports_payload()
    app_port = payload["ports"][1]
    debug_port = payload["ports"][2]

    assert app_port["label"] == "LaunchXL App UART (Recommended) (cu.usbmodem123401)"
    assert app_port["recommended"] is True
    assert app_port["possible_mcu_uart"] is True
    assert debug_port["label"] == "LaunchXL Debug UART (Usually Console Only) (cu.usbmodem123400)"
    assert debug_port["recommended"] is False
