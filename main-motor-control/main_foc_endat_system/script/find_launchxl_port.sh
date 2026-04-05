#!/bin/bash
# find_launchxl.sh - Finds TI F28379D LaunchXL connected ports

echo "=== Scanning for TI LaunchXL (F28379D) ==="

# Known TI XDS110 USB vendor ID
TI_VENDOR="0451"

echo ""
echo "--- All tty devices ---"
ls /dev/tty.usbmodem* /dev/tty.usbserial* /dev/tty.SLAB* 2>/dev/null || echo "No USB serial devices found"

echo ""
echo "--- Checking system_profiler for TI USB devices ---"
system_profiler SPUSBDataType 2>/dev/null | grep -A 10 -i "texas\|XDS\|F2837\|0451\|LaunchXL" | grep -v "^$"

echo ""
echo "--- Matching ports to TI Vendor ID ($TI_VENDOR) ---"
# Cross-reference ioreg for vendor/product IDs
PORTS=$(ls /dev/tty.usbmodem* /dev/tty.usbserial* 2>/dev/null)
if [ -z "$PORTS" ]; then
    echo "No usbmodem ports found"
else
    for port in $PORTS; do
        # Extract the device suffix to match against ioreg
        echo "Found: $port"
    done
fi

echo ""
echo "--- ioreg TI device details ---"
ioreg -p IOUSB -l 2>/dev/null | grep -A 20 -i "XDS\|Texas Instruments\|0x0451" | \
    grep -E "idVendor|idProduct|USB Product Name|IODialinDevice|IOCalloutDevice|tty" | \
    sed 's/^[ \t]*//'

echo ""
echo "--- Recommended ports (most likely Application UART) ---"
# XDS110 always enumerates two ports; the higher-numbered one is App UART
MODEM_PORTS=($(ls /dev/tty.usbmodem* 2>/dev/null | sort))
COUNT=${#MODEM_PORTS[@]}

if [ $COUNT -eq 0 ]; then
    echo "No LaunchXL detected. Check USB cable and power."
elif [ $COUNT -eq 1 ]; then
    echo "Only one port found (may need CCS/UniFlash to enable App UART):"
    echo "  Debug UART : ${MODEM_PORTS[0]}"
else
    echo "  Debug UART : ${MODEM_PORTS[0]}"
    echo "  App UART   : ${MODEM_PORTS[1]}  <-- use this in pyserial"
    echo ""
    echo "  ser = serial.Serial('${MODEM_PORTS[1]}', baudrate=115200, timeout=0.01)"
fi

echo ""
echo "=== Done ==="