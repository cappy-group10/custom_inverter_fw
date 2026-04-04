import time
from copy import deepcopy

import pygame

from commands import CtrlState
from controller import ButtonEdge, ButtonEvent, ControllerState, XboxController
from runtime import DriveRuntime
from runtime_models import FrameRecord, to_payload
from uart import FRAME_FAULT, FRAME_STATUS, MCUFault, MCUStatus, UARTCounters, UARTHealth


class FakeJoystick:
    def __init__(self):
        self.hat = (0, 0)
        self.axes = []
        self.buttons = [False] * 15

    def get_numaxes(self):
        return len(self.axes)

    def get_axis(self, index):
        return self.axes[index]

    def get_numbuttons(self):
        return len(self.buttons)

    def get_button(self, index):
        return self.buttons[index]

    def get_numhats(self):
        return 1

    def get_hat(self, _index):
        return self.hat


class FakeController:
    def __init__(self, joystick_index=0, deadzone=0.08):
        self.name = f"Fake Pad {joystick_index}"
        self.connected = False
        self.state = ControllerState(buttons={name: False for name in (
            "a", "b", "x", "y", "lb", "rb", "back", "start",
            "guide", "left_stick", "right_stick", "dpad_up",
            "dpad_down", "dpad_left", "dpad_right",
        )})
        self._poll_count = 0
        self._events = []

    def connect(self):
        self.connected = True

    def disconnect(self):
        self.connected = False

    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 1:
            self.state.left_y = -0.50
            self.state.buttons["a"] = True
            self._events.append(ButtonEvent("a", ButtonEdge.PRESSED))
        elif self._poll_count == 2:
            self.state.left_y = -0.25
            self.state.buttons["a"] = False
            self.state.buttons["dpad_up"] = True
            self._events.append(ButtonEvent("a", ButtonEdge.RELEASED))
            self._events.append(ButtonEvent("dpad_up", ButtonEdge.PRESSED))
        else:
            self.state.left_y = 0.0
            self.state.buttons["dpad_up"] = False
            self._events.append(ButtonEvent("dpad_up", ButtonEdge.RELEASED))

    def drain_events(self):
        events = list(self._events)
        self._events.clear()
        return events


class BrokenController(FakeController):
    def poll(self):
        raise RuntimeError("controller poll failed")


class FakeLink:
    def __init__(self, port=None, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self._frames = []
        self._statuses = []
        self._faults = []
        self._sent = []
        self._counters = UARTCounters()
        self._health = UARTHealth(terminal_only=port is None)

    def open(self):
        self._health.port_open = self.port is not None

    def close(self):
        self._health.port_open = False

    def send(self, cmd):
        now = time.time()
        self._sent.append(deepcopy(cmd))
        self._counters.tx_frames += 1
        self._health.last_frame_at = now
        self._frames.append(
            FrameRecord("tx", 0x01, "motor_cmd", "aa 01", to_payload(cmd), True, now)
        )

        if len(self._sent) == 1:
            status = MCUStatus(
                run_motor=1,
                ctrl_state=CtrlState.RUN,
                speed_ref=cmd.speed_ref,
                pos_mech_theta=0.25,
                vdc_bus=36.2,
                id_fbk=0.01,
                iq_fbk=0.04,
                current_as=0.7,
                current_bs=-0.2,
                current_cs=-0.5,
                isr_ticker=11,
            )
            self._statuses.append(status)
            self._frames.append(
                FrameRecord("rx", FRAME_STATUS, "status", "55 10", to_payload(status), True, now)
            )
            self._counters.rx_frames += 1
            self._counters.status_frames += 1
            self._health.last_status_at = now
        elif len(self._sent) == 2:
            fault = MCUFault(trip_flag=0x0002, trip_count=1)
            self._faults.append(fault)
            self._frames.append(
                FrameRecord("rx", FRAME_FAULT, "fault", "55 11", to_payload(fault), True, now)
            )
            self._counters.rx_frames += 1
            self._counters.fault_frames += 1

    def pop_frame_records(self):
        frames = list(self._frames)
        self._frames.clear()
        return frames

    def pop_statuses(self):
        statuses = list(self._statuses)
        self._statuses.clear()
        return statuses

    def pop_faults(self):
        faults = list(self._faults)
        self._faults.clear()
        return faults

    def get_counters(self):
        return deepcopy(self._counters)

    def get_health(self):
        return deepcopy(self._health)


def wait_for(predicate, timeout=1.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(0.01)
    return False


def test_dpad_hat_generates_normalized_button_edges(monkeypatch):
    joystick = FakeJoystick()
    controller = XboxController()
    controller.connected = True
    controller._joystick = joystick
    for name in ("dpad_up", "dpad_down", "dpad_left", "dpad_right"):
        controller.state.buttons[name] = False

    monkeypatch.setattr(pygame.event, "pump", lambda: None)

    joystick.hat = (0, 1)
    controller.poll()
    first_events = controller.drain_events()
    assert any(event.button == "dpad_up" and event.edge == ButtonEdge.PRESSED for event in first_events)
    assert controller.state.buttons["dpad_up"] is True

    joystick.hat = (0, 0)
    controller.poll()
    second_events = controller.drain_events()
    assert any(event.button == "dpad_up" and event.edge == ButtonEdge.RELEASED for event in second_events)
    assert controller.state.buttons["dpad_up"] is False


def test_drive_runtime_runs_and_bounds_history():
    runtime = DriveRuntime(
        controller_factory=FakeController,
        link_factory=FakeLink,
        frame_history=3,
        event_history=3,
        telemetry_history=2,
    )

    runtime.start(port="demo", baudrate=115200, joystick_index=0)
    assert wait_for(lambda: runtime.get_snapshot().counters["tx_frames"] >= 2)

    snapshot = runtime.get_snapshot()
    assert snapshot.session_state == "running"
    assert snapshot.last_host_command.ctrl_state == CtrlState.RUN
    assert snapshot.last_host_command.iq_ref > 0
    assert any(item.control_id == "left_stick" and item.mapping_target == "speed_ref" for item in snapshot.controller_layout)
    assert any(item.control_id == "a" and item.mapping_text == "ctrl_state = RUN" for item in snapshot.controller_layout)
    assert len(snapshot.recent_frames) <= 3
    assert len(snapshot.recent_events) <= 3
    assert len(snapshot.telemetry_samples) <= 2

    stopped = runtime.stop()
    assert stopped.session_state == "stopped"
    assert stopped.controller_connected is False


def test_drive_runtime_surfaces_background_errors():
    runtime = DriveRuntime(controller_factory=BrokenController, link_factory=FakeLink)
    runtime.start(port="demo", baudrate=115200, joystick_index=0)

    assert wait_for(lambda: runtime.get_snapshot().session_state == "error")
    snapshot = runtime.stop()

    assert snapshot.session_state == "error"
    assert "controller poll failed" in snapshot.last_error
