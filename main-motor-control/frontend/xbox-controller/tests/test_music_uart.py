import struct

import pytest

from xbox_controller.music_uart import (
    FRAME_CTRL_CMD,
    FRAME_SONG_CMD,
    FRAME_STATUS,
    FRAME_VOL_CMD,
    RX_SYNC,
    CTRL_ACTION_PAUSE,
    MusicUARTLink,
)


def build_status_frame(link: MusicUARTLink) -> bytes:
    payload = struct.pack(
        ">BBBBBHHffI",
        RX_SYNC,
        FRAME_STATUS,
        1,
        0,
        2,
        4,
        32,
        523.25,
        0.2,
        88,
    )
    return payload + bytes([link._checksum(payload)])


def test_pack_song_control_and_volume_frames():
    link = MusicUARTLink(port=None)

    song_frame = link.pack_song(1, 0.25)
    ctrl_frame = link.pack_control(CTRL_ACTION_PAUSE)
    vol_frame = link.pack_volume(0.4)

    assert song_frame[1] == FRAME_SONG_CMD
    assert ctrl_frame[1] == FRAME_CTRL_CMD
    assert vol_frame[1] == FRAME_VOL_CMD
    assert song_frame[-1] == link._checksum(song_frame[:-1])
    assert ctrl_frame[-1] == link._checksum(ctrl_frame[:-1])
    assert vol_frame[-1] == link._checksum(vol_frame[:-1])


def test_send_song_logs_tx_frame():
    link = MusicUARTLink(port=None)

    link.send_song(2, 0.3)

    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(frames) == 1
    assert frames[0].direction == "tx"
    assert frames[0].frame_name == "song_cmd"
    assert frames[0].decoded["song_id"] == 2
    assert counters.tx_frames == 1


def test_parse_status_frame_and_record_rx_state():
    link = MusicUARTLink(port=None)

    link._rx_buf.extend(build_status_frame(link))
    link._parse_rx_buf()

    statuses = link.pop_statuses()
    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(statuses) == 1
    assert statuses[0].play_state == "PLAYING"
    assert statuses[0].play_mode == "SONG"
    assert statuses[0].song_id == 2
    assert statuses[0].current_freq_hz == pytest.approx(523.25)
    assert len(frames) == 1
    assert frames[0].direction == "rx"
    assert frames[0].frame_name == "status"
    assert counters.rx_frames == 1
    assert counters.status_frames == 1


def test_checksum_mismatch_is_counted_for_music_status():
    link = MusicUARTLink(port=None)
    frame = bytearray(build_status_frame(link))
    frame[-1] ^= 0xFF

    link._rx_buf.extend(frame)
    link._parse_rx_buf()

    assert link.pop_statuses() == []
    frames = link.pop_frame_records()
    counters = link.get_counters()

    assert len(frames) == 1
    assert frames[0].checksum_ok is False
    assert counters.checksum_errors == 1
