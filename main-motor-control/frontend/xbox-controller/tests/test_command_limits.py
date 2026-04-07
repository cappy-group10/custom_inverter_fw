from xbox_controller.commands import CommandLimits
from xbox_controller.controller import ButtonEdge, ButtonEvent, ControllerState
from xbox_controller.mapping import DriveMapping


def _empty_state() -> ControllerState:
    return ControllerState(buttons={name: False for name in (
        "a", "b", "x", "y", "lb", "rb", "back", "start",
        "guide", "left_stick", "right_stick", "dpad_up",
        "dpad_down", "dpad_left", "dpad_right",
    )})


def test_default_drive_limits_match_updated_id_iq_ranges():
    limits = CommandLimits()

    assert limits.speed_min == -0.65
    assert limits.speed_max == 0.65
    assert limits.id_min == -0.5
    assert limits.id_max == 0.5
    assert limits.iq_min == -0.8
    assert limits.iq_max == 0.8


def test_drive_mapping_saturates_speed_to_new_forward_limit():
    mapping = DriveMapping()
    state = _empty_state()
    state.left_y = -1.0

    command = mapping.process(state, [])

    assert command.speed_ref == 0.65


def test_drive_mapping_saturates_speed_to_new_reverse_limit():
    mapping = DriveMapping()
    state = _empty_state()
    state.left_y = 1.0

    command = mapping.process(state, [])

    assert command.speed_ref == -0.65


def test_drive_mapping_saturates_to_new_id_iq_limits():
    mapping = DriveMapping()
    state = _empty_state()

    command = None
    for _ in range(100):
        command = mapping.process(state, [ButtonEvent("dpad_up", ButtonEdge.PRESSED)])
    assert command is not None
    assert command.iq_ref == 0.8

    for _ in range(100):
        command = mapping.process(state, [ButtonEvent("dpad_down", ButtonEdge.PRESSED)])
    assert command is not None
    assert command.iq_ref == -0.8

    for _ in range(100):
        command = mapping.process(state, [ButtonEvent("dpad_right", ButtonEdge.PRESSED)])
    assert command is not None
    assert command.id_ref == 0.5

    for _ in range(100):
        command = mapping.process(state, [ButtonEvent("dpad_left", ButtonEdge.PRESSED)])
    assert command is not None
    assert command.id_ref == -0.5
