import time
from copy import deepcopy
from types import SimpleNamespace

import pygame
import pytest

from xbox_controller.commands import CtrlState
from xbox_controller.controller import ButtonEdge, ButtonEvent, ControllerState, XboxController
from xbox_controller.runtime import DriveRuntime
from xbox_controller.runtime_models import FrameRecord, to_payload
from xbox_controller.uart import FRAME_FAULT, FRAME_STATUS, MCUFault, MCUStatus, UARTCounters, UARTHealth


class FakeJoystick:
    def __init__(self):
        self.hat = (0, 0)
        self.axes = []
        self.buttons = [False] * 15
        self.instance_id = 77
        self.initialized = True

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

    def get_instance_id(self):
        return self.instance_id

    def get_init(self):
        return self.initialized

    def quit(self):
        self.initialized = False


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


class DisconnectingController(FakeController):
    def poll(self):
        raise RuntimeError("Controller disconnected during runtime")


class IdleController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        self.state.left_y = 0.0
        self.state.right_x = 0.0


class ButtonEdgeController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 2:
            self.state.buttons["a"] = True
            self._events.append(ButtonEvent("a", ButtonEdge.PRESSED))
        elif self._poll_count == 3:
            self.state.buttons["a"] = False
            self._events.append(ButtonEvent("a", ButtonEdge.RELEASED))


class LeftStickController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 2:
            self.state.left_y = -0.20
        elif self._poll_count == 3:
            self.state.left_y = -0.20
        elif self._poll_count >= 4:
            self.state.left_y = 0.0


class RightStickOnlyController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 2:
            self.state.right_x = 0.35
        elif self._poll_count == 3:
            self.state.right_x = -0.35
        else:
            self.state.right_x = 0.0


class DpadIqTrimController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 2:
            self.state.buttons["dpad_up"] = True
            self._events.append(ButtonEvent("dpad_up", ButtonEdge.PRESSED))


class RunThenIqTrimController(FakeController):
    def poll(self):
        self._poll_count += 1
        self._events = []
        if self._poll_count == 1:
            self.state.buttons["a"] = True
            self._events.append(ButtonEvent("a", ButtonEdge.PRESSED))
        elif self._poll_count == 2:
            self.state.buttons["dpad_up"] = True
            self._events.append(ButtonEvent("dpad_up", ButtonEdge.PRESSED))


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
                speed_fbk=max(cmd.speed_ref - 0.02, -1.0),
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


class ReleaseBrakeLink(FakeLink):
    def send(self, cmd):
        now = time.time()
        self._sent.append(deepcopy(cmd))
        self._counters.tx_frames += 1
        self._health.last_frame_at = now
        self._frames.append(FrameRecord("tx", 0x01, "motor_cmd", "aa 01", to_payload(cmd), True, now))

        status = MCUStatus(
            run_motor=1 if cmd.ctrl_state == CtrlState.RUN else 0,
            ctrl_state=cmd.ctrl_state,
            speed_ref=cmd.speed_ref,
            speed_fbk=cmd.speed_ref,
            pos_mech_theta=0.10,
            vdc_bus=36.0,
            id_fbk=cmd.id_ref,
            iq_fbk=cmd.iq_ref,
            current_as=0.1,
            current_bs=-0.05,
            current_cs=-0.05,
            isr_ticker=len(self._sent),
        )
        self._statuses.append(status)
        self._frames.append(FrameRecord("rx", FRAME_STATUS, "status", "55 10", to_payload(status), True, now))
        self._counters.rx_frames += 1
        self._counters.status_frames += 1
        self._health.last_status_at = now

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
    monkeypatch.setattr(pygame.event, "get", lambda *_args, **_kwargs: [])
    monkeypatch.setattr(pygame.joystick, "get_count", lambda: 1)

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


def test_controller_poll_detects_disconnected_controller(monkeypatch):
    joystick = FakeJoystick()
    controller = XboxController()
    controller.connected = True
    controller._joystick = joystick
    controller._instance_id = joystick.instance_id
    controller.name = "Fake Pad"

    monkeypatch.setattr(pygame.event, "pump", lambda: None)
    monkeypatch.setattr(
        pygame.event,
        "get",
        lambda *_args, **_kwargs: [SimpleNamespace(type=pygame.JOYDEVICEREMOVED, instance_id=joystick.instance_id)],
    )

    with pytest.raises(RuntimeError, match="Controller disconnected during runtime"):
        controller.poll()

    assert controller.connected is False
    assert controller.name == ""


def test_controller_trigger_rest_state_defaults_to_negative_one():
    state = ControllerState()

    assert state.left_trigger == -1.0
    assert state.right_trigger == -1.0


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


def test_drive_runtime_defaults_to_deeper_uart_history():
    runtime = DriveRuntime(controller_factory=FakeController, link_factory=FakeLink)
    assert runtime._frame_history.maxlen == 1000


def test_drive_runtime_latches_ui_brake_override_until_stop():
    runtime = DriveRuntime(controller_factory=FakeController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    override_snapshot = runtime.engage_brake_override()
    assert override_snapshot.active_override == "BRAKE"

    runtime.step()
    stepped = runtime.get_snapshot()

    assert stepped.last_host_command.ctrl_state == CtrlState.BRAKE
    assert stepped.last_host_command.speed_ref == 0.0
    assert stepped.last_host_command.id_ref == 0.0
    assert stepped.last_host_command.iq_ref == 0.0
    assert stepped.active_override == "BRAKE"

    stopped = runtime.stop()
    assert stopped.active_override is None


def test_drive_runtime_release_brake_override_forces_stop_and_resets_mapping():
    runtime = DriveRuntime(controller_factory=FakeController, link_factory=ReleaseBrakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    running = runtime.get_snapshot()
    assert running.last_host_command.ctrl_state == CtrlState.RUN
    assert running.last_host_command.speed_ref > 0.0

    runtime.engage_brake_override()
    runtime.step()
    braked = runtime.get_snapshot()
    assert braked.last_host_command.ctrl_state == CtrlState.BRAKE
    assert braked.active_override == "BRAKE"

    released = runtime.release_brake_override()
    assert released.active_override is None
    assert released.last_host_command.ctrl_state == CtrlState.STOP
    assert released.last_host_command.speed_ref == 0.0
    assert released.last_host_command.id_ref == 0.0
    assert released.last_host_command.iq_ref == 0.0

    runtime.step()
    after_step = runtime.get_snapshot()
    assert after_step.active_override is None
    assert after_step.last_host_command.ctrl_state == CtrlState.STOP
    assert after_step.last_host_command.speed_ref == 0.0
    assert after_step.counters["tx_frames"] >= 3

    runtime.stop()


def test_drive_runtime_surfaces_background_errors():
    runtime = DriveRuntime(controller_factory=BrokenController, link_factory=FakeLink)
    runtime.start(port="demo", baudrate=115200, joystick_index=0)

    assert wait_for(lambda: runtime.get_snapshot().session_state == "error")
    snapshot = runtime.stop()

    assert snapshot.session_state == "error"
    assert "controller poll failed" in snapshot.last_error


def test_drive_runtime_marks_controller_disconnected_when_unplugged():
    runtime = DriveRuntime(controller_factory=DisconnectingController, link_factory=FakeLink)
    runtime.start(port="demo", baudrate=115200, joystick_index=0)

    assert wait_for(lambda: runtime.get_snapshot().session_state == "error")
    snapshot = runtime.stop()

    assert snapshot.session_state == "error"
    assert snapshot.controller_connected is False
    assert "Controller disconnected during runtime" in snapshot.last_error


def test_drive_runtime_does_not_keepalive_when_command_is_idle(monkeypatch):
    fake_now = {"value": 100.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=IdleController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.10
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.16
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1


def test_drive_runtime_transmits_on_button_press_and_release(monkeypatch):
    fake_now = {"value": 200.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=ButtonEdgeController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 3

    fake_now["value"] += 0.30
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 4


def test_drive_runtime_transmits_when_left_stick_command_changes_and_returns_to_neutral(monkeypatch):
    fake_now = {"value": 300.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=LeftStickController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2
    assert runtime.get_snapshot().last_host_command.speed_ref > 0.01

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 3
    assert runtime.get_snapshot().last_host_command.speed_ref == 0.0


def test_drive_runtime_does_not_transmit_for_unmapped_right_stick_motion(monkeypatch):
    fake_now = {"value": 400.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=RightStickOnlyController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1


def test_drive_runtime_does_not_keepalive_for_nonzero_iq_outside_run(monkeypatch):
    fake_now = {"value": 500.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=DpadIqTrimController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1
    assert runtime.get_snapshot().last_host_command.iq_ref == 0.0
    assert runtime.get_snapshot().last_host_command.ctrl_state == CtrlState.STOP

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2
    assert runtime.get_snapshot().last_host_command.iq_ref > 0.0
    assert runtime.get_snapshot().last_host_command.ctrl_state == CtrlState.STOP

    fake_now["value"] += 0.30
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2


def test_drive_runtime_keeps_alive_nonzero_iq_when_running(monkeypatch):
    fake_now = {"value": 600.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    runtime = DriveRuntime(controller_factory=RunThenIqTrimController, link_factory=FakeLink)
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 1
    assert runtime.get_snapshot().last_host_command.ctrl_state == CtrlState.RUN

    fake_now["value"] += 0.05
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 2
    assert runtime.get_snapshot().last_host_command.ctrl_state == CtrlState.RUN
    assert runtime.get_snapshot().last_host_command.iq_ref > 0.0

    fake_now["value"] += 0.30
    runtime.step()
    assert runtime.get_snapshot().counters["tx_frames"] == 3


def test_drive_runtime_emits_coalesced_ui_tick(monkeypatch):
    fake_now = {"value": 700.0}

    monkeypatch.setattr(time, "time", lambda: fake_now["value"])

    streamed = []
    runtime = DriveRuntime(
        controller_factory=FakeController,
        link_factory=FakeLink,
        event_callback=streamed.append,
    )
    runtime.open(port="demo", baudrate=115200, joystick_index=0)

    runtime.step()
    assert streamed == []

    fake_now["value"] += runtime._ui_sample_interval
    runtime.step()

    ui_ticks = [event for event in streamed if event["type"] == "ui_tick"]
    assert len(ui_ticks) == 1
    payload = ui_ticks[0]["payload"]

    assert payload["controller_state"]["buttons"]["dpad_up"] is True
    assert payload["motor_config"]["base_speed_rpm"] > 0
    assert payload["last_host_command"]["ctrl_state"] == CtrlState.RUN.name
    assert payload["latest_mcu_status"]["ctrl_state"] == CtrlState.RUN.name
    assert payload["latest_mcu_status"]["speed_fbk"] == pytest.approx(payload["latest_mcu_status"]["speed_ref"] - 0.02)
    assert payload["counters"]["tx_frames"] == 2
    assert len(payload["new_frames"]) == 4
    assert len(payload["new_faults"]) == 1
    assert any(event["kind"] == "fault" for event in payload["new_events"])
    assert all(event["type"] == "ui_tick" for event in ui_ticks)
    snapshot = runtime.get_snapshot()
    assert snapshot.telemetry_samples[-1].speed_fbk == pytest.approx(snapshot.telemetry_samples[-1].speed_ref - 0.02)
    assert snapshot.telemetry_samples[-1].id_ref == pytest.approx(0.0)
    assert snapshot.telemetry_samples[-1].iq_ref == pytest.approx(0.0)
