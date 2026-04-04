import serial
import struct
import time
import sys
from collections import deque

SERIAL_PORT = '/dev/tty.usbserial-TI9PSUYY1'
BAUD_RATE = 115200
FRAME_SIZE = 43
HEADER = bytes([0x55, 0x10])
MAX_HISTORY = 5  # number of frames to keep in display

def print_status_lines(frames):
    """Print stacked lines, keeping the display updated."""
    # Move cursor up to overwrite previous lines
    sys.stdout.write("\033[F" * len(frames))
    sys.stdout.flush()
    for f in frames:
        line = (f"ticker={f['isrTicker']}, speedRef={f['speedRef']:.2f}, "
                f"Vdc={f['Vdcbus']:.2f}, Iq={f['IqFbk']:.2f}")
        sys.stdout.write("\033[K" + line + "\n")  # clear line then print
    sys.stdout.flush()

def print_log_line(text):
    sys.stdout.write(text + "\n")
    sys.stdout.flush()

def calc_checksum(frame):
    """Checksum = sum(all preceding bytes) & 0xFF"""
    return sum(frame[:-1]) & 0xFF

def parse_frame(frame):
    """Parse the 43-byte frame into meaningful fields."""
    if len(frame) != FRAME_SIZE:
        return None

    runMotor = frame[2]
    ctrlState = frame[3]
    tripFlag = struct.unpack(">H", frame[4:6])[0]
    speedRef = struct.unpack(">f", frame[6:10])[0]
    posMechTheta = struct.unpack(">f", frame[10:14])[0]
    Vdcbus = struct.unpack(">f", frame[14:18])[0]
    IdFbk = struct.unpack(">f", frame[18:22])[0]
    IqFbk = struct.unpack(">f", frame[22:26])[0]
    currentAs = struct.unpack(">f", frame[26:30])[0]
    currentBs = struct.unpack(">f", frame[30:34])[0]
    currentCs = struct.unpack(">f", frame[34:38])[0]
    isrTicker = struct.unpack(">I", frame[38:42])[0]
    checksum = frame[42]

    if checksum != calc_checksum(frame):
        return None

    return {
        "runMotor": runMotor,
        "ctrlState": ctrlState,
        "tripFlag": tripFlag,
        "speedRef": speedRef,
        "posMechTheta": posMechTheta,
        "Vdcbus": Vdcbus,
        "IdFbk": IdFbk,
        "IqFbk": IqFbk,
        "currentAs": currentAs,
        "currentBs": currentBs,
        "currentCs": currentCs,
        "isrTicker": isrTicker,
        "checksum": checksum
    }

def main():
    s = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    buffer = bytearray()
    history = deque(maxlen=MAX_HISTORY)

    print_log_line("Listening for Phase 2 status frames... Press Ctrl+C to exit.")
    # Print empty lines for initial history display
    for _ in range(MAX_HISTORY):
        print()

    try:
        while True:
            data = s.read(256)
            if not data:
                continue
            buffer.extend(data)

            while len(buffer) >= FRAME_SIZE:
                if buffer[0:2] == HEADER:
                    frame = buffer[:FRAME_SIZE]
                    parsed = parse_frame(frame)
                    if parsed:
                        history.append(parsed)
                        print_status_lines(list(history))
                        buffer = buffer[FRAME_SIZE:]
                    else:
                        # Bad checksum
                        buffer = buffer[1:]
                else:
                    # Header mismatch
                    buffer = buffer[1:]

    except KeyboardInterrupt:
        print_log_line("\nExiting.")

    finally:
        s.close()

if __name__ == "__main__":
    main()