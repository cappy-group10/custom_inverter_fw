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

try:
    print_log_line("Testing serial port...")

    while True:
        tx = b'\xAA\x01\x02\x03'
        s.write(tx)
        rx = s.read(4)

        print_status_line(f"TX: {tx}   RX: {rx}")

        time.sleep(0.1)  # 100 ms delay

except KeyboardInterrupt:
    print_log_line("Closing serial port...")

finally:
    s.close()