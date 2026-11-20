# Reproduces what Windows shows a player who downloaded a release file (techplan/09, release audit 2026-11-20).
#
# A browser marks a downloaded file with a Zone.Identifier stream (ZoneId=3, the internet, and where it came from):
# Windows' SmartScreen checks such a file when it is opened through the shell. This script copies the file into a
# scratch folder, writes the same mark a browser writes, opens the copy through the shell (as a double-click does),
# waits, and saves a picture of the screen. It never removes a mark, never turns any protection off and never
# answers the warning: the copy's window (or the warning) is closed afterwards.
#
#   tools\release\Test-DownloadWarning.ps1 -File dist\CaucasianEagle-0.5.1-Setup.exe -Url <its download URL> -Out <folder>
#
# It is not a browser download: the browser's own checks (Edge or Chrome warnings while downloading) are not seen.
param(
    [Parameter(Mandatory)] [string]$File,
    [Parameter(Mandatory)] [string]$Url,
    [Parameter(Mandatory)] [string]$Out,
    [int]$Wait = 6,
    # A picture of the whole screen shows everything else open on it (personal windows): only on a test machine
    [switch]$Picture
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
New-Item -ItemType Directory -Force $Out | Out-Null
$copyDir = Join-Path $Out 'copy'; New-Item -ItemType Directory -Force $copyDir | Out-Null
$copy = Join-Path $copyDir (Split-Path $File -Leaf)
Copy-Item -LiteralPath $File -Destination $copy -Force
$referrer = ($Url -replace '/download/.*$', '')
Set-Content -LiteralPath $copy -Stream Zone.Identifier -Value @('[ZoneTransfer]', 'ZoneId=3', "ReferrerUrl=$referrer", "HostUrl=$Url")
$before = @(Get-Process | Select-Object -ExpandProperty Id)
$started = Get-Date
Start-Process -FilePath $copy | Out-Null     # ShellExecute: the path a double-click takes
Start-Sleep -Seconds $Wait
$shot = ''
if ($Picture) {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $shot = Join-Path $Out ("screen-" + [IO.Path]::GetFileNameWithoutExtension($File) + ".png"); $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
}
# what opened: the installer's own window (no warning stood before it) or only SmartScreen's (it holds the file back)
$new = Get-Process | Where-Object { $before -notcontains $_.Id -and $_.StartTime -ge $started.AddSeconds(-1) } | Select-Object Id, ProcessName, MainWindowTitle
$report = [ordered]@{
    File = (Split-Path $File -Leaf); Sha256 = (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash.ToLower()
    Mark = (Get-Content -LiteralPath $copy -Stream Zone.Identifier) -join ' | '
    Started = $started.ToString('o'); Picture = $shot
    NewProcesses = @($new | ForEach-Object { "$($_.ProcessName) [$($_.MainWindowTitle)]" })
    Verdict = $(if (@($new | Where-Object { $_.ProcessName -notin 'smartscreen', 'dllhost', 'conhost' }).Count -gt 0) { 'OPENED: the file ran with no warning before it' } elseif (@($new | Where-Object ProcessName -eq 'smartscreen').Count) { 'HELD: SmartScreen stood before the file (a warning)' } else { 'UNKNOWN' })
}
$report | ConvertTo-Json | Set-Content (Join-Path $Out ("result-" + [IO.Path]::GetFileNameWithoutExtension($File) + ".json"))
$report | Format-List | Out-String
# close what was opened (the copy's own process; a SmartScreen window goes with the shell's check when its owner closes)
foreach ($p in $new) {
    $proc = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
    if (-not $proc) { continue }
    if ($proc.ProcessName -eq 'smartscreen') { [void]$proc.CloseMainWindow() }    # the warning: closed, never answered
    else { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }          # the opened copy (an installer's wizard)
}
