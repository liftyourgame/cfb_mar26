# Makefile for ESP32-C3 Workshop Firmware
# 
# IMPORTANT: Must be run from CMD.EXE (not PowerShell) after activating ESP-IDF!
# 
# Usage:
#   1. Open Command Prompt (cmd.exe)
#   2. call C:\Espressif\idf_cmd_init.bat esp-idf-d8385ac05a174f441556f52f49cc7a3f
#   3. cd C:\Users\cnd\Downloads\cursor\esp32c3lcd\workshop_firmware
#   4. make build
#
# For PowerShell users, use: build_if_newer.ps1 (smart rebuild script)

.PHONY: all build flash reset clean help check

all: help

help:
	@echo ========================================
	@echo ESP32-C3 Workshop Firmware Makefile
	@echo ========================================
	@echo.
	@echo IMPORTANT: Run from CMD.EXE after: call C:\Espressif\...\export.bat
	@echo.
	@echo Available targets:
	@echo   make build    - Build firmware
	@echo   make flash    - Flash via JTAG
	@echo   make reset    - Reset board via JTAG
	@echo   make clean    - Clean build directory
	@echo   make help     - Show this help
	@echo.
	@echo If using PowerShell, use this instead:
	@echo   powershell -ExecutionPolicy Bypass -File build_if_newer.ps1
	@echo.
	@echo Or use batch files directly:
	@echo   flash_openocd.bat  - Flash firmware
	@echo   reset_board.bat    - Reset board
	@echo.
	@echo Firmware location: build\workshop_firmware.bin
	@echo.
	@echo ========================================


build:
	@echo ========================================
	@echo Building firmware...
	@echo ========================================
	@call build.bat
	@echo.
	@echo ========================================
	@echo Build complete!
	@echo ========================================

flash:
	@echo ========================================
	@echo Flashing ESP32-C3 via OpenOCD JTAG
	@echo ========================================
	@call flash_openocd.bat

reset:
	@echo ========================================
	@echo Resetting ESP32-C3 via OpenOCD JTAG
	@echo ========================================
	@call reset_board.bat

clean:
	@echo ========================================
	@echo Cleaning build directory...
	@echo ========================================
	@idf.py fullclean
	@echo.
	@echo ========================================
	@echo Clean complete!
	@echo ========================================
