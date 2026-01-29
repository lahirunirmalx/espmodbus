#!/bin/bash
# Manual filesystem upload script for ESP32

FS_IMAGE=".pio/build/esp32dev/littlefs.bin"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
PORT="/dev/ttyUSB0"

echo "Building filesystem image..."
pio run --target buildfs

if [ ! -f "$FS_IMAGE" ]; then
    echo "Error: Filesystem image not found at $FS_IMAGE"
    exit 1
fi

echo "=========================================="
echo "ESP32 Filesystem Upload"
echo "=========================================="
echo "Filesystem image size: $(ls -lh "$FS_IMAGE" | awk '{print $5}')"
echo ""

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo "Error: Port $PORT not found!"
    exit 1
fi

echo "Method 1: Automatic reset (recommended)"
echo "Press ENTER to continue, or Ctrl+C to cancel..."
read

echo "Attempting connection with automatic reset..."
$ESPTOOL \
  --chip esp32 \
  --port $PORT \
  --baud 115200 \
  --connect-attempts 5 \
  --before default_reset \
  --after hard_reset \
  write_flash 0x290000 "$FS_IMAGE"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Upload successful!"
    exit 0
fi

echo ""
echo "Automatic reset failed. Trying manual bootloader mode..."
echo ""
echo "MANUAL BOOTLOADER MODE:"
echo "1. Hold the BOOT button (GPIO0) on ESP32"
echo "2. Press and release RESET button (keep holding BOOT)"
echo "3. Release BOOT button after 1-2 seconds"
echo ""
echo "Press ENTER when ESP32 is in bootloader mode..."
read

echo "Attempting upload with manual bootloader mode..."
$ESPTOOL \
  --chip esp32 \
  --port $PORT \
  --baud 115200 \
  --connect-attempts 5 \
  --before no_reset \
  --after hard_reset \
  write_flash 0x290000 "$FS_IMAGE"

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Upload successful!"
else
    echo ""
    echo "✗ Upload failed. Troubleshooting:"
    echo "  - Try a different USB cable (data-capable, shorter is better)"
    echo "  - Try a different USB port (USB 2.0 often works better)"
    echo "  - Check permissions: sudo chmod 666 $PORT"
    echo "  - Make sure no other program is using the port"
    echo "  - Try: sudo usermod -a -G dialout $USER (then logout/login)"
    exit 1
fi
