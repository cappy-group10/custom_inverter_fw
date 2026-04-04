#!/usr/bin/env python3
"""Xbox controller for custom inverter motor control."""

import argparse
import time
from controller import XboxController, ButtonEdge
from mapping import DriveMapping, MusicalMapping
from runtime import DriveRuntime, POLL_RATE_HZ
from uart import UARTLink


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
    parser.add_argument(
        "-j", "--joystick-index", type=int, default=0,
        help="joystick index for pygame (default: 0)",
    )
    return parser.parse_args()


def _run_drive_cli(args):
    runtime = DriveRuntime()
    try:
        runtime.open(port=args.port, baudrate=args.baud, joystick_index=args.joystick_index)

        print("Mapping: DriveMapping")
        print("Press Ctrl+C to exit.\n")

        last_event_at = 0.0
        last_status_at = None
        while True:
            runtime.step()
            snapshot = runtime.get_snapshot()
            if snapshot.session_state == "error":
                raise RuntimeError(snapshot.last_error or "Drive runtime failed")

            for event in snapshot.recent_events:
                if event.timestamp <= last_event_at:
                    continue
                last_event_at = max(last_event_at, event.timestamp)
                if event.kind == "button":
                    arrow = "v" if event.data.get("edge") == ButtonEdge.PRESSED.name else "^"
                    print(f"\r\033[K  {arrow} {event.data.get('button', '?')}")
                elif event.kind in {"fault", "error"}:
                    print(f"\r\033[K  {event.message}")

            if (
                snapshot.latest_mcu_status is not None
                and snapshot.health.get("last_status_at") != last_status_at
            ):
                last_status_at = snapshot.health.get("last_status_at")
                print(f"\r\033[K  {snapshot.latest_mcu_status}")

            if snapshot.controller_state is None:
                time.sleep(1.0 / POLL_RATE_HZ)
                continue

            s = snapshot.controller_state
            cmd = snapshot.last_host_command
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
    except Exception as exc:
        print(f"\nRuntime error: {exc}")
    finally:
        runtime.stop()


def _run_music_cli(args):
    mapping = MusicalMapping()
    ctrl = XboxController(joystick_index=args.joystick_index, deadzone=0.08)
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

            for ev in events:
                arrow = "v" if ev.edge == ButtonEdge.PRESSED else "^"
                print(f"\r\033[K  {arrow} {ev.button}")

            cmd = mapping.process(s, events)
            link.send(cmd)

            for status in link.pop_statuses():
                print(f"\r\033[K  {status}")
            for fault in link.pop_faults():
                print(f"\r\033[K  {fault}")

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


def main():
    args = _parse_args()

    if args.mode == "music":
        _run_music_cli(args)
    else:
        _run_drive_cli(args)


if __name__ == "__main__":
    main()
