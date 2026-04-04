import argparse
import struct
import sys
import time
from collections import deque

SERIAL_PORT = "/dev/tty.usbserial-TI9PSUYY1"
BAUD_RATE = 115200

TX_SYNC = 0xAA
TX_FRAME_ID = 0x01
TX_FRAME_SIZE = 16

RX_SYNC = 0x55
RX_FRAME_ID = 0x10
RX_FRAME_SIZE = 43

MAX_HISTORY = 5

CTRL_STATE_NAMES = {
    0: "STOP",
    1: "RUN",
    2: "BRAKE",
    3: "RESET",
    4: "FAULT",
}


def print_log_line(text):
    sys.stdout.write(text + "\n")
    sys.stdout.flush()


def calc_checksum(data):
    """Checksum = sum(all preceding bytes) & 0xFF."""
    return sum(data) & 0xFF


def ctrl_state_name(ctrl_state):
    return CTRL_STATE_NAMES.get(ctrl_state, f"UNKNOWN({ctrl_state})")


def parse_ctrl_state(value):
    text = value.strip().upper()

    if text.isdigit():
        ctrl_state = int(text)
    else:
        if text.startswith("CTRL_"):
            text = text[5:]

        for enum_value, enum_name in CTRL_STATE_NAMES.items():
            if text == enum_name:
                return enum_value

        raise argparse.ArgumentTypeError(
            "ctrlState must be 0-4 or one of STOP, RUN, BRAKE, RESET, FAULT"
        )

    if ctrl_state not in CTRL_STATE_NAMES:
        raise argparse.ArgumentTypeError(
            "ctrlState must be 0-4 or one of STOP, RUN, BRAKE, RESET, FAULT"
        )

    return ctrl_state


def frame_to_hex(frame):
    return " ".join(f"{byte:02X}" for byte in frame)


def build_motor_cmd_frame(ctrl_state, speed_ref, id_ref, iq_ref):
    """Build [0xAA][0x01][ctrlState][speedRef][idRef][iqRef][chk]."""
    payload = struct.pack(
        ">BBBfff",
        TX_SYNC,
        TX_FRAME_ID,
        ctrl_state,
        speed_ref,
        id_ref,
        iq_ref,
    )
    frame = payload + bytes([calc_checksum(payload)])

    if len(frame) != TX_FRAME_SIZE:
        raise ValueError(f"Expected {TX_FRAME_SIZE} bytes, built {len(frame)} bytes")

    return frame


def parse_status_frame(frame):
    """Parse a 43-byte Phase 2 status frame."""
    if len(frame) != RX_FRAME_SIZE:
        return None

    if frame[0] != RX_SYNC or frame[1] != RX_FRAME_ID:
        return None

    checksum = frame[-1]
    if checksum != calc_checksum(frame[:-1]):
        return None

    return {
        "runMotor": frame[2],
        "ctrlState": frame[3],
        "tripFlag": struct.unpack(">H", frame[4:6])[0],
        "speedRef": struct.unpack(">f", frame[6:10])[0],
        "posMechTheta": struct.unpack(">f", frame[10:14])[0],
        "Vdcbus": struct.unpack(">f", frame[14:18])[0],
        "IdFbk": struct.unpack(">f", frame[18:22])[0],
        "IqFbk": struct.unpack(">f", frame[22:26])[0],
        "currentAs": struct.unpack(">f", frame[26:30])[0],
        "currentBs": struct.unpack(">f", frame[30:34])[0],
        "currentCs": struct.unpack(">f", frame[34:38])[0],
        "isrTicker": struct.unpack(">I", frame[38:42])[0],
        "checksum": checksum,
    }


def collect_status_frames(serial_port, timeout_s, max_history):
    """Collect valid status frames for a short window after TX."""
    deadline = time.monotonic() + timeout_s
    buffer = bytearray()
    history = deque(maxlen=max_history)

    while time.monotonic() < deadline:
        chunk = serial_port.read(256)
        if not chunk:
            continue

        buffer.extend(chunk)

        while len(buffer) >= RX_FRAME_SIZE:
            if buffer[0:2] == bytes([RX_SYNC, RX_FRAME_ID]):
                frame = bytes(buffer[:RX_FRAME_SIZE])
                parsed = parse_status_frame(frame)
                if parsed:
                    history.append(parsed)
                    buffer = buffer[RX_FRAME_SIZE:]
                else:
                    buffer = buffer[1:]
            else:
                buffer = buffer[1:]

    return list(history)


def print_status_frames(frames):
    if not frames:
        print_log_line("No valid status frame received during the readback window.")
        return

    print_log_line(f"Received {len(frames)} valid status frame(s):")

    for frame in frames:
        print_log_line(
            "  "
            f"ticker={frame['isrTicker']}, "
            f"runMotor={frame['runMotor']}, "
            f"ctrlState={ctrl_state_name(frame['ctrlState'])}({frame['ctrlState']}), "
            f"tripFlag=0x{frame['tripFlag']:04X}, "
            f"speedRef={frame['speedRef']:.3f}, "
            f"IdFbk={frame['IdFbk']:.3f}, "
            f"IqFbk={frame['IqFbk']:.3f}, "
            f"Vdcbus={frame['Vdcbus']:.3f}, "
            f"posMechTheta={frame['posMechTheta']:.3f}"
        )


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Send a Phase 3 UART motor-command frame to the MCU and optionally "
            "listen for Phase 2 status telemetry."
        )
    )
    parser.add_argument("--port", default=SERIAL_PORT, help=f"serial port (default: {SERIAL_PORT})")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help=f"baud rate (default: {BAUD_RATE})")
    parser.add_argument(
        "--ctrl-state",
        type=parse_ctrl_state,
        default=1,
        help="ctrlState as 0-4 or STOP/RUN/BRAKE/RESET/FAULT (default: RUN)",
    )
    parser.add_argument("--speed-ref", type=float, default=0.05, help="speedRef float32 (default: 0.05)")
    parser.add_argument("--id-ref", type=float, default=0.0, help="IdRef float32 (default: 0.0)")
    parser.add_argument("--iq-ref", type=float, default=0.10, help="IqRef float32 (default: 0.10)")
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="number of identical command frames to send (default: 1)",
    )
    parser.add_argument(
        "--period",
        type=float,
        default=0.10,
        help="seconds between repeated frames when --count > 1 (default: 0.10)",
    )
    parser.add_argument(
        "--status-timeout",
        type=float,
        default=2.0,
        help="seconds to listen for status frames after TX; set to 0 to disable (default: 2.0)",
    )
    parser.add_argument(
        "--history",
        type=int,
        default=MAX_HISTORY,
        help=f"number of received status frames to keep for display (default: {MAX_HISTORY})",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    try:
        import serial
    except ModuleNotFoundError:
        print_log_line("pyserial is not installed. Install it with: python3 -m pip install pyserial")
        sys.exit(1)

    if args.count < 1:
        print_log_line("--count must be >= 1")
        sys.exit(1)

    if args.period < 0.0:
        print_log_line("--period must be >= 0")
        sys.exit(1)

    if args.status_timeout < 0.0:
        print_log_line("--status-timeout must be >= 0")
        sys.exit(1)

    if args.history < 1:
        print_log_line("--history must be >= 1")
        sys.exit(1)

    frame = build_motor_cmd_frame(
        args.ctrl_state,
        args.speed_ref,
        args.id_ref,
        args.iq_ref,
    )

    print_log_line("Opening serial port...")

    try:
        serial_port = serial.Serial(args.port, args.baud, timeout=0.05)
    except serial.SerialException as exc:
        print_log_line(f"Failed to open serial port:\n{exc}")
        sys.exit(1)

    try:
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()

        print_log_line("Sending Phase 3 motor command frame(s):")
        print_log_line(
            f"  ctrlState={ctrl_state_name(args.ctrl_state)}({args.ctrl_state}), "
            f"speedRef={args.speed_ref:.3f}, "
            f"IdRef={args.id_ref:.3f}, "
            f"IqRef={args.iq_ref:.3f}"
        )
        print_log_line(f"  raw={frame_to_hex(frame)}")

        for tx_idx in range(args.count):
            serial_port.write(frame)
            serial_port.flush()
            print_log_line(f"  TX[{tx_idx + 1}/{args.count}] checksum=0x{frame[-1]:02X}")

            if tx_idx + 1 < args.count:
                time.sleep(args.period)

        if args.status_timeout > 0.0:
            print_log_line("")
            print_log_line(
                "Listening for Phase 2 status frames so you can confirm that the "
                "MCU applied the command..."
            )
            print_log_line(
                "Note: runSyncControl() always mirrors ctrlState/speedRef, while "
                "IdRef/IqRef are only copied when BUILDLEVEL == FCL_LEVEL3."
            )
            status_frames = collect_status_frames(serial_port, args.status_timeout, args.history)
            print_status_frames(status_frames)

    except KeyboardInterrupt:
        print_log_line("\nExiting.")
    finally:
        serial_port.close()
        print_log_line("Serial port closed.")


if __name__ == "__main__":
    main()
