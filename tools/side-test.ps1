# Off the way (CAUCASIAN_EAGLE_DEMO=14, 0.5.3; the owner: "i want whole mountain to be walkable beside the paths we
# have"): runs the staged game unattended. Every -Every m along the way she is stood on it and runs straight off it to
# its left, then to its right, until she is 30 m off, stopped, under the land, or 8 s pass. side.tsv in the out folder
# has each run (how far, how far she dropped, why she stopped) and the counts; a picture of each run gone wrong
# (BLOCKED: stopped where the ground ahead is walkable; FELL_THROUGH: under the land; timeout).
# -Film: every frame of each run in side-trace.tsv and a picture every 0.25 s (use with -From and -To).
#   tools\side-test.ps1 [-Every 40] [-From 20] [-To 8000] [-Film] [-Out <folder>]
param([double]$Every = 40, [double]$From = 20, [double]$To = 1e9, [switch]$Film, [string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-side'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$vars = @{ CAUCASIAN_EAGLE_DEMO = '14'; CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv'); CAUCASIAN_EAGLE_TOUR_SHOTS = $Out;
           CAUCASIAN_EAGLE_SIDE_EVERY = "$Every"; CAUCASIAN_EAGLE_SIDE_FROM = "$From"; CAUCASIAN_EAGLE_SIDE_TO = "$To"; CAUCASIAN_EAGLE_SIDE_FILM = $(if ($Film) { '1' } else { '' }) }
foreach ($k in $vars.Keys) { Set-Item "env:$k" $vars[$k] }
try { $game = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
finally { foreach ($k in $vars.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue } }
if (-not $game.WaitForExit(5400000)) { "The test ran too long; the game is left running (pid $($game.Id))."; exit 3 }
Add-Type -AssemblyName System.Drawing
Get-ChildItem $Out -Filter *.bmp | ForEach-Object {
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    $img.Save([IO.Path]::ChangeExtension($_.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
    Remove-Item $_.FullName
}
$table = Join-Path $Out 'side.tsv'
if (Test-Path $table) { Get-Content $table | Where-Object { $_ -match '^(count|SIDE_)' } }
"runs and pictures in $Out"
exit $game.ExitCode
