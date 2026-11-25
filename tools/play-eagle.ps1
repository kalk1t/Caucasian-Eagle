# Starts the staged game at the last Eagle Nest (the snow-line gate), a short climb below the Eagle's court, to fight the
# Eagle without climbing the whole mountain (0.5.3). Its own save in Temp: the player's save is never read or touched.
#   tools\play-eagle.ps1 [-Gear 0..3] [-Nest 1..9]
#   Gear: 0 her own, 1 THE HIGHLANDS, 2 AMIRANI'S FIRE, 3 MOONLIT (the default). She has five potions (Z).
# Every run starts fresh at that nest; H at a nest saves into the Temp save only.
param([ValidateRange(0, 3)][int]$Gear = 3, [ValidateRange(1, 9)][int]$Nest = 9)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
$dir = Join-Path $env:TEMP 'caucasian-eagle-play'
New-Item -ItemType Directory -Force $dir | Out-Null
$save = Join-Path $dir 'eagle-try.sav'
@("# tools\play-eagle.ps1: a try at the Eagle", "rules 3", "nest $Nest", "crystals 0 0 0", "potions 5", "gear $Gear") |
    Set-Content -LiteralPath $save -Encoding ascii
$env:CAUCASIAN_EAGLE_ASCENT_SAVE = $save
try { Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) | Out-Null }
finally { Remove-Item env:CAUCASIAN_EAGLE_ASCENT_SAVE -ErrorAction SilentlyContinue }
"Started at Eagle Nest $Nest in gear $Gear (save: $save). Choose Climb the Eagle's Ascent: it goes on from that nest."
