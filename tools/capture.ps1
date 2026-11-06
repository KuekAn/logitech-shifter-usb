# Log the shifter's serial output for a number of seconds, prefixed with the elapsed time.
# Usage: capture.ps1 [-Seconds 5] [-OutFile log.csv] [-Port COM10]
# Without -Port it uses the serial port of the nice!nano running one of these sketches (USB 1209:0001).
param(
  [string]$Port,
  [double]$Seconds = 5,
  [string]$OutFile
)

$ErrorActionPreference = 'Stop'
if (-not $Port) {
  $d = Get-PnpDevice -PresentOnly -Class Ports -ErrorAction SilentlyContinue |
       Where-Object { $_.InstanceId -match 'VID_1209&PID_0001' } | Select-Object -First 1
  if ($d -and $d.FriendlyName -match '\((COM\d+)\)') { $Port = $Matches[1] }
  else { throw "shifter serial port (USB 1209:0001) not found; pass -Port" }
}

$sp = New-Object System.IO.Ports.SerialPort $Port, 115200
$sp.DtrEnable = $true   # TinyUSB CDC only sends once DTR is set
$sp.ReadTimeout = 500
$sp.Open()
$lines = New-Object System.Collections.Generic.List[string]
$start = Get-Date
$end = $start.AddSeconds($Seconds)
try {
  while ((Get-Date) -lt $end) {
    try {
      $l = $sp.ReadLine().Trim()
      $t = ((Get-Date) - $start).TotalSeconds
      if ($l) { $lines.Add(('{0:F1},{1}' -f $t, $l)) }
    } catch [System.TimeoutException] { }
  }
} finally { $sp.Close() }

if ($OutFile) { $lines | Set-Content -Encoding utf8 $OutFile }
$lines
