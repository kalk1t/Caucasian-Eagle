# What an install could change on Windows, as plain lines to compare before and after (release audit 2026-11-20):
# startup entries, scheduled tasks, services, firewall rules, the game's uninstall entry, Start menu / desktop
# shortcuts, its install folder, and the hashes of the player's saves and settings (%LOCALAPPDATA%\CaucasianEagle).
#   tools\release\Get-SystemSnapshot.ps1 -Out before.txt      ...install...      -Out after.txt; then compare the two
param([Parameter(Mandatory)] [string]$Out)
$lines = New-Object System.Collections.Generic.List[string]
foreach ($key in 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run', 'HKCU:\Software\Microsoft\Windows\CurrentVersion\RunOnce',
                 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Run', 'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Run') {
    $p = Get-ItemProperty $key -ErrorAction SilentlyContinue
    if ($p) { $p.PSObject.Properties | Where-Object Name -notlike 'PS*' | ForEach-Object { $lines.Add("run`t$key`t$($_.Name)") } }
}
Get-ScheduledTask -ErrorAction SilentlyContinue | ForEach-Object { $lines.Add("task`t$($_.TaskPath)$($_.TaskName)") }
Get-Service -ErrorAction SilentlyContinue | ForEach-Object { $lines.Add("service`t$($_.Name)") }
Get-NetFirewallRule -ErrorAction SilentlyContinue | ForEach-Object { $lines.Add("firewall`t$($_.Name)") }
$u = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{7E3C1B52-9A4D-4F6E-8B21-CA0CA51A0E61}_is1'
$e = Get-ItemProperty $u -ErrorAction SilentlyContinue
if ($e) { $lines.Add("uninstall`t$($e.DisplayName)`t$($e.DisplayVersion)`t$($e.Publisher)`t$($e.InstallLocation)") }
foreach ($dir in "$env:APPDATA\Microsoft\Windows\Start Menu\Programs", [Environment]::GetFolderPath('Desktop')) {
    Get-ChildItem $dir -Recurse -Filter *.lnk -ErrorAction SilentlyContinue | Where-Object Name -like 'Caucasian Eagle*' | ForEach-Object {
        $s = (New-Object -ComObject WScript.Shell).CreateShortcut($_.FullName)
        $lines.Add("shortcut`t$($_.FullName)`t$($s.TargetPath) $($s.Arguments)")
    }
}
$app = "$env:LOCALAPPDATA\Programs\Caucasian Eagle"
if (Test-Path $app) { $f = Get-ChildItem $app -Recurse -File; $lines.Add("installed`t$app`t$($f.Count) files`t$([math]::Round(($f | Measure-Object Length -Sum).Sum / 1MB)) MB") }
$data = "$env:LOCALAPPDATA\CaucasianEagle"
if (Test-Path $data) { Get-ChildItem $data -File | ForEach-Object { $lines.Add("playerdata`t$($_.Name)`t$((Get-FileHash $_.FullName).Hash.Substring(0, 16))") } }
$lines | Sort-Object | Set-Content -Encoding UTF8 $Out
"$($lines.Count) lines -> $Out"
