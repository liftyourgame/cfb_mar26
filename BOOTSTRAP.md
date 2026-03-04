# Bootstrap Checklist

This file is for the agent. Work through each step silently. Keep the user informed with brief, friendly status updates but do not ask them to run commands or look at this file.

---

## Step 1: Arduino CLI

1. Run `arduino-cli version`. If it succeeds, mark done and continue.
2. If not found, detect OS:
   - Run `uname` → `Darwin` = macOS, `Linux` = Linux
   - On Windows: `$env:OS` returns `Windows_NT`
3. Install:

   **macOS:**
   - `which brew` — if found: `brew install arduino-cli`
   - If no brew: `curl -fsSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_macOS_64bit.tar.gz | tar xz -C /usr/local/bin`

   **Linux:**
   - `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh`
   - If not on PATH: `sudo mv arduino-cli /usr/local/bin/`

   **Windows:**
   - `winget --version` — if found: `winget install Arduino.ArduinoCLI`
   - If no winget: download `https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip`, extract, add to PATH

4. Confirm: `arduino-cli version`
5. Install ESP32 core:
   ```
   arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core install esp32:esp32 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

[] Arduino CLI installed

---

## Step 2: Connect to the board

The board is pre-flashed with the starter project. When powered on it connects to WiFi and shows its IP on the OLED.

**Path A — WiFi (no cable):**
- Ask the user to power the board on and read the IP from the OLED
- Ping the IP to confirm it's reachable
- Store the IP for use in subsequent flashing

**Path B — USB cable:**
- Run `arduino-cli board list` and confirm a port appears
- Store the port for use in subsequent flashing
- WiFi flashing is still preferred if available; use USB as fallback

[] Board is reachable

---

## Step 3: Test flash

- Compile and flash a minimal hello world sketch (OLED displays "Hello!")
- Use WiFi OTA if IP is known, otherwise USB
- Confirm the OLED updates

[] Test flash succeeded

---

## Done

Once all steps are checked, update the bootstrap status in CLAUDE.md:
- Change `[] Bootstrap complete` to `[x] Bootstrap complete`
- Do not bother the user with details — just let them know they're ready to go
