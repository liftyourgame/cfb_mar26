#!/bin/bash
# Builds a LittleFS image from sketches/captive_portal/data/ and uploads
# it to the SPIFFS partition on the ESP32.
# Run this whenever you change files in the data/ folder.
# Usage: ./upload_data.sh

set -e

MKLITTLEFS=~/Library/Arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs
ESPTOOL=~/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool
DATA_DIR="$(dirname "$0")/sketches/captive_portal/data"
IMAGE="/tmp/spiffs.bin"

# From huge_app.csv
SPIFFS_OFFSET=0x310000
SPIFFS_SIZE=0xE0000

# Auto-detect USB port
PORT=$(/opt/homebrew/bin/arduino-cli board list 2>/dev/null | awk '/USB.*ESP32/ {print $1}')
if [ -z "$PORT" ]; then
  echo "Error: no ESP32 found over USB. Is the board plugged in?"
  exit 1
fi

echo "Data dir : $DATA_DIR"
echo "Port     : $PORT"
echo "Offset   : $SPIFFS_OFFSET  Size: $SPIFFS_SIZE"
echo ""

echo "Building LittleFS image..."
"$MKLITTLEFS" -c "$DATA_DIR" -s $SPIFFS_SIZE "$IMAGE"

echo ""
echo "Uploading to $PORT..."
"$ESPTOOL" --port "$PORT" --baud 921600 write_flash $SPIFFS_OFFSET "$IMAGE"

echo ""
echo "Done. Reboot the board to pick up new files."
