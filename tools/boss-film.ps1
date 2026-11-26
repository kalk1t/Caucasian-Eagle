# A boss filmed as it fights her (CAUCASIAN_EAGLE_DEMO=12): runs the staged game unattended, puts her before one boss and
# keeps her whole; boss.tsv (its place, the ground it stands on, the highest walkable floor under it, hers) and its frames
# land in the out folder. Never touches a player's save.
# -Keys "<seconds>:<key>,...": her keys by the film's clock, each down a tenth of a second (L, R the mouse buttons, F, V,
# C, E, J for Space): "4:F" is her slide four seconds in, toward the creature she was stood facing, 20 m off.
# -Walk (a creature of the way, "-Boss way-0200"): she walks at it up the way, as a player goes, and cuts within reach,
# her left string and her right in turn: her gear's flair and a finishing kill in the game itself.
# -SlowMs <n>: every frame waits n thousandths of a second more, as on a slow computer
#   (16: about 31 frames a second, 33: about 21, 60: about 12, where the film runs at 200 and more; 100 and more: 10,
#   the engine's longest frame: slower than that the game plays slowly).
#   tools\boss-film.ps1 [-Boss boss-eagle] [-Seconds 40] [-Every 0.5] [-Fight | -Walk] [-Gear 0] [-Keys "4:F"] [-SlowMs 0] [-Out <folder>]
param([string]$Boss = 'boss-eagle', [double]$Seconds = 40, [double]$Every = 0.5, [switch]$Fight, [switch]$Walk, [int]$Gear = 0, [string]$Keys = '', [int]$SlowMs = 0,
      [string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-boss'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$vars = @{ CAUCASIAN_EAGLE_DEMO = '12'; CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv'); CAUCASIAN_EAGLE_TOUR_SHOTS = $Out;
           CAUCASIAN_EAGLE_BOSS = $Boss; CAUCASIAN_EAGLE_BOSS_SECONDS = "$Seconds"; CAUCASIAN_EAGLE_BOSS_EVERY = "$Every";
           CAUCASIAN_EAGLE_BOSS_FIGHT = $(if ($Walk) { '2' } elseif ($Fight) { '1' } else { '0' }); CAUCASIAN_EAGLE_GEAR = "$Gear"; CAUCASIAN_EAGLE_BOSS_KEYS = $Keys; CAUCASIAN_EAGLE_SLOW_MS = "$SlowMs" }
foreach ($k in $vars.Keys) { Set-Item "env:$k" $vars[$k] }
try { $game = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
finally { foreach ($k in $vars.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue } }
if (-not $game.WaitForExit([int](($Seconds + 120) * 1000))) { "The film ran too long; the game is left running (pid $($game.Id))."; exit 3 }
Get-ChildItem $Out -Filter *.bmp | ForEach-Object {
    Add-Type -AssemblyName System.Drawing
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    $img.Save([IO.Path]::ChangeExtension($_.FullName, '.png'), [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
    Remove-Item $_.FullName
}
"frames and boss.tsv in $Out"
exit $game.ExitCode
