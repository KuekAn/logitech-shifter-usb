# Watch the shifter through the Windows joystick API (winmm), the layer joy.cpl uses.
# Prints the pressed button (1-6, 7 = R) whenever it changes.
# Usage: joytest.ps1 [-Seconds 60]
param([int]$Seconds = 60)

Add-Type -Namespace W -Name Joy -MemberDefinition @'
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct JOYCAPSW {
  public ushort wMid, wPid;
  [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
  public uint wXmin, wXmax, wYmin, wYmax, wZmin, wZmax, wNumButtons, wPeriodMin, wPeriodMax,
              wRmin, wRmax, wUmin, wUmax, wVmin, wVmax, wCaps, wMaxAxes, wNumAxes, wMaxButtons;
  [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szRegKey;
  [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string szOEMVxD;
}
[StructLayout(LayoutKind.Sequential)]
public struct JOYINFOEX {
  public uint dwSize, dwFlags, dwXpos, dwYpos, dwZpos, dwRpos, dwUpos, dwVpos,
              dwButtons, dwButtonNumber, dwPOV, dwReserved1, dwReserved2;
}
[DllImport("winmm.dll")] public static extern uint joyGetNumDevs();
[DllImport("winmm.dll", CharSet = CharSet.Unicode)] public static extern uint joyGetDevCapsW(UIntPtr id, ref JOYCAPSW caps, uint size);
[DllImport("winmm.dll")] public static extern uint joyGetPosEx(uint id, ref JOYINFOEX info);
'@

$id = $null
for ($i = 0; $i -lt [W.Joy]::joyGetNumDevs(); $i++) {
  $caps = New-Object W.Joy+JOYCAPSW
  if ([W.Joy]::joyGetDevCapsW([UIntPtr]::new($i), [ref]$caps, [Runtime.InteropServices.Marshal]::SizeOf($caps)) -eq 0) {
    "joystick {0}: {1:X4}:{2:X4} '{3}' buttons={4} axes={5}" -f $i, $caps.wMid, $caps.wPid, $caps.szPname, $caps.wNumButtons, $caps.wNumAxes
    if ($caps.wMid -eq 0x1209 -and $caps.wPid -eq 0x0001) { $id = $i }
  }
}
if ($null -eq $id) { throw "shifter (1209:0001) not found by winmm" }

$info = New-Object W.Joy+JOYINFOEX
$info.dwSize = [Runtime.InteropServices.Marshal]::SizeOf($info)
$info.dwFlags = 0x80  # JOY_RETURNBUTTONS
$last = -1
$start = Get-Date
while (((Get-Date) - $start).TotalSeconds -lt $Seconds) {
  if ([W.Joy]::joyGetPosEx($id, [ref]$info) -eq 0 -and $info.dwButtons -ne $last) {
    $last = $info.dwButtons
    $name = switch ($last) { 0 {'N'} 1 {'1'} 2 {'2'} 4 {'3'} 8 {'4'} 16 {'5'} 32 {'6'} 64 {'R'} default {"multiple: $last"} }
    "{0,5:F1}s  buttons=0x{1:X2}  gear {2}" -f ((Get-Date) - $start).TotalSeconds, $last, $name
  }
  Start-Sleep -Milliseconds 10
}
