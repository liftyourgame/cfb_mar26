# PowerShell script to build firmware with proper ESP-IDF environment
# This properly sets up the environment in PowerShell

$ErrorActionPreference = "Stop"

Write-Host "========================================"
Write-Host "Setting up ESP-IDF environment..."
Write-Host "========================================"

# Get environment variables from ESP-IDF
$envOutput = & C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.5.3\tools\idf_tools.py export --format key-value 2>&1

# Parse and set environment variables
foreach ($line in $envOutput) {
  if ($line -match '^([^=]+)=(.*)$') {
    $varName = $matches[1]
    $varValue = $matches[2]
    
    if ($varName -eq "PATH") {
      $expandedPath = $varValue -replace '%PATH%', $env:PATH
      $env:PATH = $expandedPath
    } else {
      Set-Item -Path "env:$varName" -Value $varValue
    }
  }
}

# Set IDF_PATH
$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.3"

Write-Host "Environment configured successfully!"
Write-Host ""
Write-Host "========================================"
Write-Host "Building firmware..."
Write-Host "========================================"
Write-Host ""

# Change to script directory
Set-Location $PSScriptRoot

# Run idf.py build
& C:\Espressif\frameworks\esp-idf-v5.5.3\tools\idf.py build

if ($LASTEXITCODE -eq 0) {
  Write-Host ""
  Write-Host "========================================"
  Write-Host "Build complete!"
  Write-Host "Firmware: build\workshop_firmware.bin"
  if (Test-Path "build\workshop_firmware.bin") {
    $size = (Get-Item "build\workshop_firmware.bin").Length
    Write-Host "Size: $([math]::Round($size/1KB, 2)) KB"
  }
  Write-Host "========================================"
}

exit $LASTEXITCODE
