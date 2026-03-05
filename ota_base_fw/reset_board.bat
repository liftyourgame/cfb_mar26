@echo off
echo Resetting ESP32-C3 via OpenOCD JTAG...

set OPENOCD=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\openocd-esp32\bin\openocd.exe
set SCRIPTS=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\openocd-esp32\share\openocd\scripts

%OPENOCD% -s %SCRIPTS% -f board/esp32c3-builtin.cfg -c "init; reset; resume; shutdown"

echo Board reset complete!
