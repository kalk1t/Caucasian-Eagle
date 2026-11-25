# The camera swept (CAUCASIAN_EAGLE_DEMO=13): runs the staged game unattended; every -Every m along the way she stands
# facing up it and the camera's pitch goes from its lowest to its highest. camera.tsv (the pitch, the camera's distance
# and height from her, where she is in its view: over 1 = out of it) and a picture of each land in the out folder.
# -Walk 'a:b,c:d' instead: she walks up the way from a to b m and back down (stairs), the camera following her as a
# player has it; camera-walk.tsv has every frame (her height, the camera's, its distance, where she is in the view).
# -Old: the camera as it was before 2026-11-22 (to compare).
#   tools\camera-test.ps1 [-Every 600] [-Walk '60:180,920:1040'] [-Old] [-Out <folder>]
param([double]$Every = 600, [string]$Walk = '', [switch]$Old, [string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-camera'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$vars = @{ CAUCASIAN_EAGLE_DEMO = '13'; CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv'); CAUCASIAN_EAGLE_TOUR_SHOTS = $Out;
           CAUCASIAN_EAGLE_CAMERA_EVERY = "$Every"; CAUCASIAN_EAGLE_CAMERA_WALK = $Walk; CAUCASIAN_EAGLE_CAMERA_OLD = $(if ($Old) { '1' } else { '' }) }
foreach ($k in $vars.Keys) { Set-Item "env:$k" $vars[$k] }
try { $game = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
finally { foreach ($k in $vars.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue } }
if (-not $game.WaitForExit(900000)) { "The test ran too long; the game is left running (pid $($game.Id))."; exit 3 }
Add-Type -AssemblyName System.Drawing
Get-ChildItem $Out -Filter *.bmp | ForEach-Object {
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    $img.Save([IO.Path]::ChangeExtension($_.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
    Remove-Item $_.FullName
}
"pictures and camera tables in $Out"
exit $game.ExitCode
