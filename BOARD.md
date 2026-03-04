## BOARD DETAILS

**Board:** AI-Thinker ESP32-C3 0.42" OLED Dev Board (ESP32-C3 Mini OLED)

![Board pinout](esp32-C3.jpeg)

### Chip
| Item | Detail |
|------|--------|
| Chip | ESP32-C3 (QFN32), revision v0.4 |
| Architecture | 32-bit RISC-V, single core, 160 MHz |
| Flash | 4 MB embedded (XMC, Manufacturer: 0x46, Device: 0x4016) |
| Crystal | 40 MHz |
| Wi-Fi | 2.4 GHz 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| USB | Native USB-Serial/JTAG via USB-C (no CH340 chip) |

### Onboard OLED
| Item | Detail |
|------|--------|
| Size | 0.42 inch |
| Resolution | 72 × 40 px |
| Protocol | I²C (internal wiring) |
| SDA | GPIO5 |
| SCL | GPIO6 |

> Note: header labels on the board show GPIO5/6 as MISO/MOSI (SPI labels) but the OLED uses them as I²C internally. Treat GPIO5/6 as reserved for the display.

### Pin Map
| Pin | Left header | | Right header | Pin |
|-----|-------------|---|--------------|-----|
| — | 5V | | GPIO10 | 10 |
| — | GND | | GPIO9 (SCL/IIC) | 9 |
| — | 3V3 | | GPIO8 (SDA/IIC) | 8 |
| 20 | GPIO20 (RX/UART) | | GPIO7 (SS/SPI) | 7 |
| 21 | GPIO21 (TX/UART) | | GPIO6 (MOSI/SPI) ⚠ OLED SCL | 6 |
| 2 | GPIO2 (A2/ADC) | | GPIO5 (MISO/SPI) ⚠ OLED SDA | 5 |
| 1 | GPIO1 (A1/ADC) | | GPIO4 (A4/ADC) | 4 |
| 0 | GPIO0 (A0/ADC) | | GPIO3 (A3/ADC) | 3 |
| — | BOOT button | | RST button | — |

### Key notes for programming
- **Flash mode:** QIO, 80 MHz; 4 MB flash
- **Flashing:** hold BOOT, press RST, release RST, then release BOOT (if auto-download fails)
- **OLED:** SSD1306 driver, I²C address `0x3C`, 72×40 px, SDA=GPIO5, SCL=GPIO6
- **ADC:** GPIO0–GPIO4 are ADC1 channels; avoid GPIO5/6 for ADC (reserved for OLED)

