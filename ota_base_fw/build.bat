@echo off
echo ========================================
echo Building ESP32-C3 Workshop Firmware
echo ========================================
call C:\Espressif\idf_cmd_init.bat esp-idf-d8385ac05a174f441556f52f49cc7a3f
cd /d %~dp0
if "%~1"=="" (
  idf.py build
) else (
  idf.py %*
)
