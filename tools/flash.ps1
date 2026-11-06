# Build a sketch for the nice!nano and flash it over the UF2 bootloader's serial DFU.
# Usage: flash.ps1 -Sketch ShifterTest   (or VccProbe, ShifterHID)
# If the board runs a sketch, it is reset into the bootloader with a 1200 baud touch;
# if it is already in the bootloader (NICENANO drive visible), it is flashed directly.
param([Parameter(Mandatory)][string]$Sketch)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ada  = "$env:LOCALAPPDATA\Arduino15\packages\adafruit\hardware\nrf52\1.7.0"  # installed by setup.ps1
if (-not (Test-Path $ada)) { throw "Adafruit nRF52 core 1.7.0 not found; run tools\setup.ps1 first" }
$out  = "$root\build\$Sketch"
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
$env:ARDUINO_DIRECTORIES_USER = $root

function Get-Port([string]$vidPid) {
  $d = Get-PnpDevice -PresentOnly -Class Ports -ErrorAction SilentlyContinue |
       Where-Object { $_.InstanceId -match $vidPid } | Select-Object -First 1
  if ($d -and $d.FriendlyName -match '\((COM\d+)\)') { $Matches[1] }
}

arduino-cli compile -b nicenano:nrf52:nicenano --libraries "$ada\libraries" `
  --build-property "runtime.platform.path=$ada" --output-dir $out "$root\$Sketch"
if ($LASTEXITCODE) { throw "compile failed" }

$boot = 'VID_239A&PID_00B3'
$port = Get-Port $boot
if (-not $port) {
  $app = Get-Port 'VID_1209&PID_0001'
  if (-not $app) { throw "no nice!nano found (neither bootloader nor shifter sketch)" }
  Write-Host "Resetting $app into the bootloader"
  $sp = New-Object System.IO.Ports.SerialPort $app, 1200
  # The board may reboot while the port is still opening, so errors here are expected.
  try { $sp.Open(); $sp.DtrEnable = $true; Start-Sleep -Milliseconds 100; $sp.DtrEnable = $false; $sp.Close() } catch { }
  $deadline = (Get-Date).AddSeconds(10)
  while (-not ($port = Get-Port $boot) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
  if (-not $port) { throw "bootloader did not appear; double-tap reset and retry" }
  Start-Sleep -Milliseconds 500
}

Write-Host "Flashing via bootloader on $port"
& "$ada\tools\adafruit-nrfutil\win32\adafruit-nrfutil.exe" dfu serial `
  -pkg "$out\$Sketch.ino.zip" -p $port -b 115200 --singlebank
if ($LASTEXITCODE) { throw "flash failed" }
