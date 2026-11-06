# Build ShifterHID twice and package the files attached to a GitHub release.
# Two builds, because nice!nano clones differ in how P0.13 switches the VCC pad (see VccProbe):
#   vcc-high  EXT_VCC_ON=1  VCC on when P0.13 is HIGH (the clone this mod was built on)
#   vcc-low   EXT_VCC_ON=0  VCC on when P0.13 is LOW  (genuine nice!nano v2)
# For each: ShifterHID-<version>-<vcc>.uf2      drag-and-drop onto the NICENANO drive
#           ShifterHID-<version>-<vcc>-dfu.zip  for adafruit-nrfutil (serial DFU)
#           ShifterHID-<version>-<vcc>.hex      application image (0x26000+, needs SoftDevice S140 6.1.1)
# Usage: make-release.ps1 -Version v1.0.0      (output in build\release)
# The .uf2 is converted here from the .hex (same layout as Adafruit's uf2conv.py -f 0xADA52840),
# so no Python is needed.
param([Parameter(Mandatory)][string]$Version)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ada  = "$env:LOCALAPPDATA\Arduino15\packages\adafruit\hardware\nrf52\1.7.0"  # installed by setup.ps1
if (-not (Test-Path $ada)) { throw "Adafruit nRF52 core 1.7.0 not found; run tools\setup.ps1 first" }
$dist  = "$root\build\release"
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
$env:ARDUINO_DIRECTORIES_USER = $root

function Build-Variant([string]$name, [int]$vccOn) {
  $out = "$root\build\release-$name"
  $flag = "-DEXT_VCC_ON=$vccOn"
  # Separate, clean build path so the core (which includes variant.cpp) is recompiled with the flag.
  # Kept in %TEMP%: object paths under a deep repo folder can exceed Windows' 260-char limit.
  arduino-cli compile -b nicenano:nrf52:nicenano --libraries "$ada\libraries" --clean `
    --build-path "$env:TEMP\shifterhid-$name" --output-dir $out `
    --build-property "runtime.platform.path=$ada" `
    --build-property "compiler.c.extra_flags=$flag" --build-property "compiler.cpp.extra_flags=$flag" `
    "$root\ShifterHID" | Select-Object -Last 1
  if ($LASTEXITCODE) { throw "compile failed ($name)" }
  $out
}

function U32([string]$hex) { [Convert]::ToUInt32($hex, 16) }  # PS 5.1 reads 0x8xxxxxxx+ literals as negative

function ConvertTo-Uf2([string]$hexPath, [string]$uf2Path, [uint32]$family = (U32 'ADA52840')) {
  # Parse Intel HEX into 256-byte pages (unwritten bytes stay 0x00, like uf2conv.py).
  $pages = @{}
  $base = 0
  foreach ($line in [IO.File]::ReadAllLines($hexPath)) {
    if (-not $line.StartsWith(':')) { continue }
    # int[], not byte[]: -shl on a byte would truncate to 8 bits
    $rec  = [int[]]::new(($line.Length - 1) / 2)
    for ($i = 0; $i -lt $rec.Length; $i++) { $rec[$i] = [Convert]::ToInt32($line.Substring(1 + 2 * $i, 2), 16) }
    $len = $rec[0]; $off = ($rec[1] -shl 8) -bor $rec[2]; $type = $rec[3]
    switch ($type) {
      0 {
        for ($i = 0; $i -lt $len; $i++) {
          $addr = [uint32]($base + $off + $i)
          $page = [uint32]($addr - ($addr % 256))
          if (-not $pages.ContainsKey($page)) { $pages[$page] = [byte[]]::new(256) }
          $pages[$page][$addr % 256] = [byte]$rec[4 + $i]
        }
      }
      2 { $base = ((($rec[4] -shl 8) -bor $rec[5]) -shl 4) }
      4 { $base = ((($rec[4] -shl 8) -bor $rec[5]) -shl 16) }
    }
  }
  $addrs = $pages.Keys | Sort-Object
  $n = $addrs.Count
  $out = New-Object IO.MemoryStream
  $w = New-Object IO.BinaryWriter $out
  $blk = 0
  foreach ($a in $addrs) {
    $w.Write((U32 '0A324655')); $w.Write((U32 '9E5D5157'))
    $w.Write([uint32]0x00002000)        # flags: familyID present
    $w.Write([uint32]$a); $w.Write([uint32]256)
    $w.Write([uint32]$blk); $w.Write([uint32]$n); $w.Write([uint32]$family)
    $w.Write($pages[$a]); $w.Write([byte[]]::new(476 - 256))
    $w.Write((U32 '0AB16F30'))
    $blk++
  }
  $w.Flush()
  [IO.File]::WriteAllBytes($uf2Path, $out.ToArray())
  "{0}: {1} blocks, 0x{2:X} - 0x{3:X}" -f (Split-Path $uf2Path -Leaf), $n, $addrs[0], ($addrs[-1] + 255)
}

New-Item -ItemType Directory -Force $dist | Out-Null
Get-ChildItem $dist -Filter 'ShifterHID-*' | Remove-Item
foreach ($v in @(@{ Name = 'vcc-high'; On = 1 }, @{ Name = 'vcc-low'; On = 0 })) {
  $build = (Build-Variant $v.Name $v.On)[-1]
  $base = "$dist\ShifterHID-$Version-$($v.Name)"
  ConvertTo-Uf2 "$build\ShifterHID.ino.hex" "$base.uf2"
  Copy-Item "$build\ShifterHID.ino.zip" "$base-dfu.zip"
  Copy-Item "$build\ShifterHID.ino.hex" "$base.hex"
}
Get-ChildItem $dist | ForEach-Object {
  '{0}  {1,7} bytes  sha256 {2}' -f $_.Name, $_.Length, (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
}
