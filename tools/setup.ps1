# One-time toolchain setup on Windows: arduino-cli + the Adafruit nRF52 core (the versions this mod was tested with).
# Usage: tools\setup.ps1
# Leaves any existing Arduino IDE / arduino-cli configuration untouched.

$ErrorActionPreference = 'Stop'
$coreVersion = '1.7.0'
$boardIndex  = 'https://adafruit.github.io/arduino-board-index/package_adafruit_index.json'

$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
  Write-Host "Installing arduino-cli with winget"
  winget install --id ArduinoSA.CLI -e --accept-source-agreements --accept-package-agreements
  $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
}
arduino-cli version

arduino-cli core update-index --additional-urls $boardIndex
if ($LASTEXITCODE) { throw "core update-index failed" }
arduino-cli core install "adafruit:nrf52@$coreVersion" --additional-urls $boardIndex
if ($LASTEXITCODE) { throw "core install failed" }

$root = Split-Path $PSScriptRoot -Parent
$env:ARDUINO_DIRECTORIES_USER = $root
arduino-cli board listall nicenano
Write-Host "`nDone. Next: tools\flash.ps1 -Sketch VccProbe (see README)"
