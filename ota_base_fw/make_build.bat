@echo off
setlocal

rem Activate ESP-IDF environment
call C:\Espressif\frameworks\esp-idf-v5.5.3\export.bat

rem Build the project
cd /d %~dp0
idf.py build

endlocal
