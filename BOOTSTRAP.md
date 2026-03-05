# Bootstrap Checklist

This file is for the agent. Work through each step silently. Keep the user informed with brief, friendly status updates but do not ask them to run commands or look at this file. Do not install unnecessary things like emulators etc unless explicitly asked to.

---

## Step 1: Arduino CLI

1. Locate `arduino-cli` using: `which arduino-cli || find /usr /opt /home/$USER ~/bin -name arduino-cli 2>/dev/null | head -1`
2. If found, record the full path. If `arduino-cli version` works without a full path, use `arduino-cli`. Otherwise use the full path for all subsequent commands.
3. If not found, detect OS and install:
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

4. Confirm it works, then record the resolved path in CLAUDE.md under the `## TOOLS` section, replacing the `<!-- BOOTSTRAP WILL FILL THIS IN -->` comment with:
   ```
   - `arduino-cli` path: <full path> — always use this full path, do not assume it is on PATH.
   - ESP32 core status: <installed/not installed>
   ```
5. Install ESP32 core if not already present:
   ```
   arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core install esp32:esp32 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

[] Arduino CLI installed
[] Updated CLI path in CLAUDE.md and AGENTS.md

---

## Step 2: Connect to the board

The default assumption is that the board is connected via USB-C data cable.

**Path A — USB cable (default):**
- Run `arduino-cli board list` and confirm a port appears
- Store the port for use in subsequent flashing

**Path B — WiFi (no cable):**
- If no USB port is detected, ask the user to power the board on and read the IP from the OLED
- Ping the IP to confirm it's reachable
- Store the IP for use in subsequent flashing

[] Board is reachable

---

## Step 3: Test flash

- Compile and flash `sketches/hello_oled/hello_oled.ino`
- Use USB port if detected, otherwise WiFi OTA
- Confirm the OLED updates with the hello_oled output

[] Test flash succeeded

---

## Done

Once all steps are checked, update CLAUDE.md:
- Change `[] Bootstrap complete` to `[x] Bootstrap complete`
- Ensure the `## TOOLS` section has been filled in with the detected arduino-cli path and ESP32 core status
- Do not bother the user with details — just let them know they're ready to go
