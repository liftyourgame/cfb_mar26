#!/bin/bash
# Compiles and flashes a sketch to the ESP32-C3 over USB.
# Usage: ./deploy.sh [sketch_name]
# Default sketch: captive_portal

set -e

ARDUINO_CLI="/opt/homebrew/bin/arduino-cli"
FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app"
SKETCHES_DIR="$(dirname "$0")/sketches"
SKETCH="${1:-captive_portal}"
SKETCH_PATH="$SKETCHES_DIR/$SKETCH/$SKETCH.ino"

# Validate sketch exists
if [ ! -f "$SKETCH_PATH" ]; then
  echo "Error: sketch not found at $SKETCH_PATH"
  echo "Available sketches:"
  ls "$SKETCHES_DIR"
  exit 1
fi

# Auto-detect USB port
PORT=$("$ARDUINO_CLI" board list 2>/dev/null | awk '/USB.*ESP32/ {print $1}')
if [ -z "$PORT" ]; then
  echo "Error: no ESP32 found over USB. Is the board plugged in?"
  exit 1
fi

echo "Sketch : $SKETCH"
echo "Port   : $PORT"
echo "FQBN   : $FQBN"
echo ""

echo "Compiling..."
"$ARDUINO_CLI" compile --fqbn "$FQBN" "$SKETCH_PATH"

echo ""
echo "Flashing..."
"$ARDUINO_CLI" upload --fqbn "$FQBN" --port "$PORT" "$SKETCH_PATH"

echo ""
echo "Done."
