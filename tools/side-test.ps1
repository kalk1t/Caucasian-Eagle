# Off the way (CAUCASIAN_EAGLE_DEMO=14, 0.5.3; the owner: "i want whole mountain to be walkable beside the paths we
# have"): runs the staged game unattended. Every -Every m along the way she is stood on it and runs straight off it to
# its left, then to its right, until she is 30 m off, stopped, under the land, or 8 s pass. side.tsv in the out folder
# has each run (how far, how far she dropped, why she stopped) and the counts; a picture of each run gone wrong
# (BLOCKED: stopped where the ground ahead is walkable; FELL_THROUGH: under the land; timeout).
# -Film: every frame of each run in side-trace.tsv and a picture every 0.25 s (use with -From and -To);
#   -FilmEvery <s>: a picture so often. -Trace: side-trace.tsv without the pictures (any number of runs). The trace has
#   her place, the land and the walkable ground at it, and the camera's place and the land at it (the camera under the
#   land outside a cave: cam_y under cam_land where cam_cave is 0).
# -Dash: she slides (F) off the way and runs on from where the slide leaves her (19 m in a second: her slide against the
# mountain's walls). The same verdicts; compare its runs with a run without it, place by place.
# -Ahead: one run a place, up the way itself ("ahead") in place of the two off it: with -Dash and -Trace, her slide up
# the way's stairs and climbs as the player's camera follows it.
# -Jog: at her easy pace (W alone, 9.6 m/s; 3.2 before her shadow dash, 0.5.4) in place of her run (Shift, 16.2 m/s;
# 5.4); a run then has 14 s. At her pace as made her height followed the ground the closer the slower she went, and
# her easy pace climbed slopes her run stopped at; going in pieces now (NyxaraKit's FootStep) she runs up them too.
# -SlowMs <n>: every frame waits n thousandths of a second more, as on a slow computer
#   (16: about 31 frames a second, 33: about 21, 60: about 12, where the test runs at 200 and more; 100 and more: 10,
#   the engine's longest frame: slower than that the game plays slowly).
#   tools\side-test.ps1 [-Every 40] [-From 20] [-To 8000] [-Film] [-FilmEvery 0.25] [-Trace] [-Dash] [-Ahead] [-Jog] [-SlowMs 0] [-Out <folder>]
param([double]$Every = 40, [double]$From = 20, [double]$To = 1e9, [switch]$Film, [double]$FilmEvery = 0.25, [switch]$Trace, [switch]$Dash, [switch]$Ahead, [switch]$Jog, [int]$SlowMs = 0, [string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-side'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$vars = @{ CAUCASIAN_EAGLE_DEMO = '14'; CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv'); CAUCASIAN_EAGLE_TOUR_SHOTS = $Out;
           CAUCASIAN_EAGLE_SIDE_EVERY = "$Every"; CAUCASIAN_EAGLE_SIDE_FROM = "$From"; CAUCASIAN_EAGLE_SIDE_TO = "$To"; CAUCASIAN_EAGLE_SIDE_FILM = $(if ($Film) { '1' } else { '' }); CAUCASIAN_EAGLE_SIDE_TRACE = $(if ($Trace) { '1' } else { '' }); CAUCASIAN_EAGLE_SIDE_FILM_EVERY = "$FilmEvery"; CAUCASIAN_EAGLE_SIDE_DASH = $(if ($Dash) { '1' } else { '' }); CAUCASIAN_EAGLE_SIDE_AHEAD = $(if ($Ahead) { '1' } else { '' }); CAUCASIAN_EAGLE_SIDE_JOG = $(if ($Jog) { '1' } else { '' }); CAUCASIAN_EAGLE_SLOW_MS = "$SlowMs" }
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
