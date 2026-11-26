# The jars' test (CAUCASIAN_EAGLE_DEMO=4): runs the staged game unattended. The clay jars are laid and counted; she cuts
# at ten groups (eight of the way from the shore to the top, two in caves); she saves at the first nest, breaks more,
# falls back to the save, and the save is read back. ways.tsv in the out folder has each row and WAYS_PASS or WAYS_FAIL,
# with a picture of each group whole and broken.
# -Dash: in place of her three cuts she stands on the way beside each group, facing it, and slides (F) through it once
# (after 0.5.3 her slide is a dash and breaks the jars beside her way); slid at a cave's wall she must be in the cave still.
# -SlowMs <n>: every frame waits n thousandths of a second more, as on a slow computer
#   (16: about 31 frames a second, 33: about 21, 60: about 12, where the test runs at 200 and more; 100 and more: 10,
#   the engine's longest frame: slower than that the game plays slowly).
#   tools\ways-test.ps1 [-Dash] [-SlowMs 0] [-Out <folder>]
param([switch]$Dash, [int]$SlowMs = 0, [string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-ways'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$vars = @{ CAUCASIAN_EAGLE_DEMO = '4'; CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv'); CAUCASIAN_EAGLE_TOUR_SHOTS = $Out;
           CAUCASIAN_EAGLE_WAYS_DASH = $(if ($Dash) { '1' } else { '' }); CAUCASIAN_EAGLE_SLOW_MS = "$SlowMs" }
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
$table = Join-Path $Out 'ways.tsv'
if (Test-Path $table) { Get-Content $table }
"rows and pictures in $Out"
exit $game.ExitCode
