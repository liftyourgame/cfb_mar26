#!/bin/bash
# Streams serial log output from the ESP32-C3 over USB.
# Usage: ./logs.sh
# Press Ctrl+C to exit.

ARDUINO_CLI="/opt/homebrew/bin/arduino-cli"

# Auto-detect USB port
PORT=$("$ARDUINO_CLI" board list 2>/dev/null | awk '/USB.*ESP32/ {print $1}')
if [ -z "$PORT" ]; then
  echo "Error: no ESP32 found over USB. Is the board plugged in?"
  exit 1
fi

echo "Listening on $PORT at 115200 baud — Ctrl+C to stop"
echo "----------------------------------------"

"$ARDUINO_CLI" monitor --port "$PORT" --config baudrate=115200
