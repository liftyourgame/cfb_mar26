# Captive Portal — PLAN

## Overview
ESP32-C3 acts as a WiFi access point with a captive DNS portal. When users connect and open a browser, they're automatically redirected to a welcome page served by the ESP32.

## WiFi AP
- **SSID:** `humn.au`
- **Password:** `turner73`
- Standalone network (no internet bridging)

## Captive Portal Mechanism
1. **DNS Server** — responds to ALL DNS queries with the ESP32's AP IP (`192.168.4.1`), so any domain name resolves to the ESP32
2. **Web Server** — serves the welcome page on any request
3. **Captive Portal Detection** — handle OS-specific captive portal probes (Android `/generate_204`, Apple `/hotspot-detect.html`, Windows `/connecttest.txt`) to trigger the automatic "sign in to network" popup

## Welcome Page
- Clean, modern HTML page
- "Welcome to Humn" heading
- Minimal, dark-themed design
- Self-contained (inline CSS, no external resources)

## OLED Display (72x40)
- Line 1: "humn.au" (SSID)
- Line 2: Number of connected clients (e.g. "Clients: 3")
- Updates every 2 seconds

## LED
- Blue LED blinks while AP is starting
- Solid on once AP is running

## Libraries
- `WiFi.h` — built-in, AP mode
- `DNSServer.h` — built-in, intercept all DNS
- `WebServer.h` — built-in, serve portal page
- `U8g2lib.h` — already installed, OLED display
- `Wire.h` — built-in, I2C for OLED
