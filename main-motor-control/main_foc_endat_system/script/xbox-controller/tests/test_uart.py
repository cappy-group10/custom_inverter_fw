import struct

import pytest

from xbox_controller.commands import CtrlState, MotorCommand
from xbox_controller.uart import FRAME_STATUS, RX_SYNC, TX_MOTOR_LEN, MCUStatus, UARTLink


def build_status_frame(link: UARTLink, status: MCUStatus) -> bytes:
    payload = struct.pack(
        ">BBBBHfffffffffI",
        RX_SYNC,
        FRAME_STATUS,
        status.run_motor,
        status.ctrl_state,
        status.trip_flag,
        status.speed_ref,
        status.speed_fbk,
        status.pos_mech_theta,
        status.vdc_bus,
        status.id_fbk,
        status.iq_fbk,
        status.current_as,
        status.current_bs,
        status.current_cs,
        status.isr_ticker,
    )
    return payload + bytes([link._checksum(payload)])


def build_legacy_status_frame(link: UARTLink, status: MCUStatus) -> bytes:
    payload = struct.pack(
        ">BBBBHffffffffI",
        RX_SYNC,
        FRAME_STATUS,
        status.run_motor,
        status.ctrl_state,
        status.trip_flag,
        status.speed_ref,
        status.pos_mech_theta,
        status.vdc_bus,
        status.id_fbk,
        status.iq_fbk,
        status.current_as,
        status.current_bs,
        status.current_cs,
        status.isr_ticker,
    )
    return payload + bytes([link._checksum(payload)])


def test_pack_motor_frame_and_log_tx_record():
    link = UARTLink(port=None)
    cmd = MotorCommand(ctrl_state=CtrlState.RUN, speed_ref=0.12, id_ref=-0.04, iq_ref=0.08)

    frame = link.pack_motor(cmd)
    assert len(frame) == TX_MOTOR_LEN
    assert frame[-1] == link._checksum(frame[:-1])

    link.send(cmd)

    frame_records = link.pop_frame_records()
    counters = link.get_counters()
    assert len(frame_records) == 1
    assert frame_records[0].direction == "tx"
    assert frame_records[0].decoded["ctrl_state"] == "RUN"
    assert counters.tx_frames == 1


def test_parse_status_frame_and_record_rx_state():
    link = UARTLink(port=None)
    expected = MCUStatus(
        run_motor=1,
        ctrl_state=CtrlState.RUN,
        trip_flag=0x0002,
        speed_ref=0.15,
        speed_fbk=0.12,
        pos_mech_theta=0.33,
        vdc_bus=34.5,
        id_fbk=-0.02,
        iq_fbk=0.07,
        current_as=1.0,
        current_bs=-0.4,
        current_cs=-0.6,
        isr_ticker=42,
    )

    link._rx_buf.extend(build_status_frame(link, expected))
    link._parse_rx_buf()

    statuses = link.pop_statuses()
    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(statuses) == 1
    assert statuses[0].speed_ref == pytest.approx(expected.speed_ref)
    assert statuses[0].speed_fbk == pytest.approx(expected.speed_fbk)
    assert len(frames) == 1
    assert frames[0].direction == "rx"
    assert frames[0].frame_name == "status"
    assert counters.rx_frames == 1
    assert counters.status_frames == 1


def test_checksum_mismatch_is_counted_without_status_update():
    link = UARTLink(port=None)
    status = MCUStatus(ctrl_state=CtrlState.RUN, speed_ref=0.2, speed_fbk=0.18, vdc_bus=36.0, isr_ticker=7)
    frame = bytearray(build_status_frame(link, status))
    frame[-1] ^= 0xFF

    link._rx_buf.extend(frame)
    link._parse_rx_buf()

    assert link.pop_statuses() == []
    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(frames) == 1
    assert frames[0].checksum_ok is False
    assert counters.checksum_errors == 1


def test_parse_legacy_status_frame_without_checksum_mismatch():
    link = UARTLink(port=None)
    expected = MCUStatus(
        run_motor=1,
        ctrl_state=CtrlState.RUN,
        trip_flag=0x0003,
        speed_ref=0.12,
        pos_mech_theta=0.22,
        vdc_bus=48.0,
        id_fbk=0.01,
        iq_fbk=0.05,
        current_as=0.8,
        current_bs=-0.4,
        current_cs=-0.4,
        isr_ticker=99,
    )

    link._rx_buf.extend(build_legacy_status_frame(link, expected))
    link._parse_rx_buf()

    statuses = link.pop_statuses()
    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(statuses) == 1
    assert statuses[0].speed_ref == pytest.approx(expected.speed_ref)
    assert statuses[0].speed_fbk == pytest.approx(0.0)
    assert len(frames) == 1
    assert frames[0].checksum_ok is True
    assert frames[0].decoded["legacy_status_frame"] is True
    assert counters.status_frames == 1
    assert counters.checksum_errors == 0
