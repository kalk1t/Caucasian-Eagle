# Starts the staged game with Nyxara in one of her gears, to try it before her quests give it (0.5.3):
#   0 her own, 1 THE HIGHLANDS, 2 AMIRANI'S FIRE, 3 MOONLIT.
#   tools\play-gear.ps1 -Gear 2
# The gear is set over her save (CAUCASIAN_EAGLE_GEAR); saving at a nest (H) writes it into the save.
param([ValidateRange(0, 3)][int]$Gear = 1)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
$env:CAUCASIAN_EAGLE_GEAR = "$Gear"
try { Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) | Out-Null }
finally { Remove-Item env:CAUCASIAN_EAGLE_GEAR -ErrorAction SilentlyContinue }
