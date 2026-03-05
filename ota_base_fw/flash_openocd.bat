@echo off
echo ========================================
echo Flashing ESP32-C3 via OpenOCD JTAG
echo ========================================

set OPENOCD=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\openocd-esp32\bin\openocd.exe
set SCRIPTS=C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251215\openocd-esp32\share\openocd\scripts

if not exist "build\workshop_firmware.bin" (
  echo ERROR: Firmware not found! Run build.bat first.
  exit /b 1
)

if not exist "build\bootloader\bootloader.bin" (
  echo ERROR: Bootloader not found! Run build.bat first.
  exit /b 1
)

if not exist "build\partition_table\partition-table.bin" (
  echo ERROR: Partition table not found! Run build.bat first.
  exit /b 1
)

echo Flashing bootloader, partition table, and firmware via JTAG...
echo This will flash:
echo   - Bootloader at 0x0
echo   - Partition table at 0x8000
echo   - Main firmware at 0x20000 (factory partition)
echo.
%OPENOCD% -s %SCRIPTS% -f board/esp32c3-builtin.cfg -c "init; reset halt; adapter speed 1000; flash probe 0; program_esp build/bootloader/bootloader.bin 0x0 verify; program_esp build/partition_table/partition-table.bin 0x8000 verify; program_esp build/workshop_firmware.bin 0x20000 verify; reset run; shutdown"

echo.
echo ========================================
echo Flash complete! Device should be running.
echo ========================================
