import serial
import sys
import time

def print_status_line(text):
    sys.stdout.write("\r\033[K" + text)
    sys.stdout.flush()

def print_log_line(text):
    sys.stdout.write("\r\033[K" + text + "\n")
    sys.stdout.flush()

SERIAL_PORT = '/dev/tty.usbserial-TI9PSUYY1'
BAUD_RATE = 115200

print_log_line("Opening serial port...")

try:
    s = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
except serial.SerialException as e:
    print_log_line(f"Failed to open serial port:\n{e}")
    sys.exit(1)

# Initialize byte values
val1, val2, val3, val4 = 0x01, 0x02, 0x03, 0x04

try:
    print_log_line("Testing serial port...")

    while True:
        try:
            # Construct bytes to send
            tx = bytes([val1, val2, val3, val4])
            s.write(tx)

            # Read back 4 bytes
            rx = s.read(4)

            # Print as hex
            tx_hex = ' '.join(f'{b:02X}' for b in tx)
            rx_hex = ' '.join(f'{b:02X}' for b in rx)
            print_status_line(f"TX: {tx_hex}   RX: {rx_hex}")

            # Increment bytes (wrap around)
            val1 = (val1 + 1) & 0xFF
            val2 = (val2 + 1) & 0xFF
            val3 = (val3 + 1) & 0xFF
            val4 = (val4 + 1) & 0xFF

            time.sleep(0.1)

        except serial.SerialException as e:
            print_log_line(f"\nSerial error: {e}. Attempting to reconnect...")
            s.close()
            time.sleep(1)
            try:
                s = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                print_log_line("Reconnected to serial port.")
            except serial.SerialException as e2:
                print_log_line(f"Reconnect failed: {e2}. Retrying...")
                time.sleep(2)

except KeyboardInterrupt:
    print_log_line("\nClosing serial port...")

finally:
    if s.is_open:
        s.close()
        print_log_line("Serial port closed.")
