#!/usr/bin/env python3
"""Xbox controller demo — prints live input to the terminal."""

import time
from controller import XboxController


POLL_RATE_HZ = 60


def main():
    ctrl = XboxController(deadzone=0.08)
    ctrl.connect()

    try:
        while True:
            ctrl.poll()
            s = ctrl.state

            # Build a compact status line
            pressed = [name for name, on in s.buttons.items() if on]
            dpad_str = f"({s.dpad[0]:+d},{s.dpad[1]:+d})" if s.dpad != (0, 0) else ""

            print(
                f"\r\033[KLX:{s.left_x:+.2f} LY:{s.left_y:+.2f} "
                f"RX:{s.right_x:+.2f} RY:{s.right_y:+.2f} "
                f"LT:{s.left_trigger:+.2f} RT:{s.right_trigger:+.2f} "
                f"BTN:[{','.join(pressed)}] {dpad_str}",
                end="", flush=True,
            )

            time.sleep(1.0 / POLL_RATE_HZ)
    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        ctrl.disconnect()


if __name__ == "__main__":
    main()
