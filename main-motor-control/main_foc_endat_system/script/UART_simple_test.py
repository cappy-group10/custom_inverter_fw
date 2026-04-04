import serial
import sys
import time


def print_status_line(text):
    sys.stdout.write("\r\033[K" + text)
    sys.stdout.flush()

def print_log_line(text):
    sys.stdout.write("\r\033[K" + text + "\n")
    sys.stdout.flush()

print_log_line("Opening serial port...")
s = serial.Serial('/dev/tty.usbserial-TI9PSUYY1', 115200, timeout=1)

# Initialize byte values
val1, val2, val3, val4 = 0x01, 0x02, 0x03, 0x04

try:
    print_log_line("Testing serial port...")

    while True:
        # Construct bytes to send
        tx = bytes([val1, val2, val3, val4])
        s.write(tx)

        # Read back 4 bytes (assumes MCU echoes them)
        rx = s.read(4)

        # Print in hex for binary-safe view
        tx_hex = ' '.join(f'{b:02X}' for b in tx)
        rx_hex = ' '.join(f'{b:02X}' for b in rx)

        print_status_line(f"TX: {tx_hex}   RX: {rx_hex}")

        # Increment each byte (wrap around at 0xFF)
        val1 = (val1 + 1) & 0xFF
        val2 = (val2 + 1) & 0xFF
        val3 = (val3 + 1) & 0xFF
        val4 = (val4 + 1) & 0xFF

        time.sleep(0.1)  # 100 ms delay

except KeyboardInterrupt:
    print_log_line("Closing serial port...")

finally:
    s.close()