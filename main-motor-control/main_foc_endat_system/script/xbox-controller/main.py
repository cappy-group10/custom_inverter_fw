#!/usr/bin/env python3
"""Xbox controller for custom inverter motor control."""

import argparse
import time
from controller import XboxController, ButtonEdge
from mapping import DriveMapping, MusicalMapping
from uart import UARTLink


POLL_RATE_HZ = 60


def _parse_args():
    parser = argparse.ArgumentParser(
        description="Xbox controller interface for custom inverter"
    )
    parser.add_argument(
        "mode", nargs="?", default="drive", choices=["drive", "music"],
        help="control mode (default: drive)",
    )
    parser.add_argument(
        "-p", "--port",
        help="serial port (e.g. /dev/tty.usbserial-1234). "
             "omit for terminal-only mode",
    )
    parser.add_argument(
        "-b", "--baud", type=int, default=115200,
        help="baud rate (default: 115200)",
    )
    return parser.parse_args()


def main():
    args = _parse_args()

    if args.mode == "music":
        mapping = MusicalMapping()
    else:
        mapping = DriveMapping()

    ctrl = XboxController(deadzone=0.08)
    ctrl.connect()

    link = UARTLink(port=args.port, baudrate=args.baud)
    link.open()

    print(f"Mapping: {type(mapping).__name__}")
    print("Press Ctrl+C to exit.\n")

    try:
        while True:
            ctrl.poll()
            s = ctrl.state
            events = ctrl.drain_events()

            # Log button edges
            for ev in events:
                arrow = "v" if ev.edge == ButtonEdge.PRESSED else "^"
                print(f"\r\033[K  {arrow} {ev.button}")

            # Compute and send command
            cmd = mapping.process(s, events)
            link.send(cmd)

            # Print any MCU responses
            for status in link.drain_status():
                print(f"\r\033[K  {status}")
            for fault in link.drain_faults():
                print(f"\r\033[K  {fault}")

            # Status line
            pressed = [name for name, on in s.buttons.items() if on]
            dpad_str = f"({s.dpad[0]:+d},{s.dpad[1]:+d})" if s.dpad != (0, 0) else ""

            print(
                f"\r\033[K"
                f"LX:{s.left_x:+.2f} LY:{s.left_y:+.2f} "
                f"RX:{s.right_x:+.2f} RY:{s.right_y:+.2f} "
                f"BTN:[{','.join(pressed)}] {dpad_str} "
                f"| {cmd}",
                end="", flush=True,
            )

            time.sleep(1.0 / POLL_RATE_HZ)
    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        link.close()
        ctrl.disconnect()


if __name__ == "__main__":
    main()
