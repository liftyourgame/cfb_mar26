$ErrorActionPreference = "Stop"

$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.3"
$env:PATH = "C:\Espressif\tools\riscv32-esp-elf\esp-13.2.0_20240530\riscv32-esp-elf\bin;" + $env:PATH
$env:PATH = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" + $env:PATH
$env:PATH = "C:\Espressif\tools\cmake\3.24.0\bin;" + $env:PATH
$env:PATH = "C:\Espressif\tools\ninja\1.11.1;" + $env:PATH

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Setting ESP32-C3 Target" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Set-Location "C:\Users\cnd\Downloads\cursor\esp32c3lcd\workshop_firmware"

& python.exe "$env:IDF_PATH\tools\idf.py" set-target esp32c3

if ($LASTEXITCODE -eq 0) {
  Write-Host "========================================" -ForegroundColor Green
  Write-Host "Building Firmware" -ForegroundColor Green
  Write-Host "========================================" -ForegroundColor Green
  
  & python.exe "$env:IDF_PATH\tools\idf.py" build
}
