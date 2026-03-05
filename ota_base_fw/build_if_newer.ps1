# PowerShell script to rebuild firmware if source files are newer
# Usage: .\build_if_newer.ps1

$ErrorActionPreference = "Stop"

$FIRMWARE_BIN = "build\workshop_firmware.bin"
$SOURCE_FILES = @(
  "main\main.c",
  "components\u8g2\u8g2_esp32_hal.c",
  "components\u8g2\u8g2_esp32_hal.h",
  "main\CMakeLists.txt",
  "components\u8g2\CMakeLists.txt",
  "CMakeLists.txt",
  "partitions.csv",
  "sdkconfig.defaults"
)

Write-Host "========================================"
Write-Host "ESP32-C3 Workshop Firmware Build Check"
Write-Host "========================================"
Write-Host ""

if (-not (Test-Path $FIRMWARE_BIN)) {
  Write-Host "Firmware binary not found. Building..."
  Write-Host ""
  & C:\Espressif\frameworks\esp-idf-v5.5.3\export.bat
  & C:\Espressif\frameworks\esp-idf-v5.5.3\tools\idf.py build
  exit $LASTEXITCODE
}

$firmwareTime = (Get-Item $FIRMWARE_BIN).LastWriteTime
Write-Host "Firmware binary: $FIRMWARE_BIN"
Write-Host "Last built: $firmwareTime"
Write-Host ""

$needsRebuild = $false
foreach ($sourceFile in $SOURCE_FILES) {
  if (Test-Path $sourceFile) {
    $sourceTime = (Get-Item $sourceFile).LastWriteTime
    if ($sourceTime -gt $firmwareTime) {
      Write-Host "Source file is newer: $sourceFile ($sourceTime)"
      $needsRebuild = $true
    }
  }
}

if ($needsRebuild) {
  Write-Host ""
  Write-Host "Source files are newer than firmware. Rebuilding..."
  Write-Host "========================================"
  Write-Host ""
  
  $currentDir = Get-Location
  & cmd.exe /c "call C:\Espressif\idf_cmd_init.bat esp-idf-d8385ac05a174f441556f52f49cc7a3f && cd /d $currentDir && idf.py build"
  $buildResult = $LASTEXITCODE
  
  if ($buildResult -eq 0) {
    Write-Host ""
    Write-Host "========================================"
    Write-Host "Build complete!"
    Write-Host "Firmware: $FIRMWARE_BIN"
    $newSize = (Get-Item $FIRMWARE_BIN).Length
    Write-Host "Size: $([math]::Round($newSize/1KB, 2)) KB"
    Write-Host "========================================"
  } else {
    Write-Host ""
    Write-Host "========================================"
    Write-Host "Build FAILED!"
    Write-Host "========================================"
  }
  
  exit $buildResult
} else {
  Write-Host ""
  Write-Host "Firmware is up to date. No rebuild needed."
  Write-Host "========================================"
  exit 0
}
