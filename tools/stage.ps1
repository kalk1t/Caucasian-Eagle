# Builds the runtime stage folder: build\stage\CaucasianEagle
#
#   CaucasianEagle.exe            (written there by the game project)
#   assets\shaders, ui, textures  engine runtime assets (from the engine submodule)
#   assets\Compiled               engine compiled assets, then the game's compiled assets on top
#
# The engine finds its content root by walking up from the executable until one folder holds
# both assets\shaders and assets\Compiled\Scenes, so the stage folder is self-contained:
# it runs in place and is exactly what the installer packs.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\stage.ps1 [-Configuration Release|Debug]
param([ValidateSet('Release', 'Debug')][string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$stageName = if ($Configuration -eq 'Debug') { 'stage-debug' } else { 'stage' }
$stage = Join-Path $repo "build\$stageName\CaucasianEagle"
$engineAssets = Join-Path $repo 'engine\assets'
$gameAssets = Join-Path $repo 'game\assets'

if (-not (Test-Path (Join-Path $engineAssets 'shaders'))) {
    throw "Engine submodule is missing. Run: git submodule update --init"
}
New-Item -ItemType Directory -Force $stage | Out-Null

function Sync([string]$from, [string]$to, [string[]]$extra = @()) {
    # robocopy exit codes 0-7 are success (files copied / already in step).
    & robocopy $from $to /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 @extra | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $from -> $to" }
}

# Engine runtime assets. Source art, references and model exports are not runtime content.
foreach ($folder in 'shaders', 'ui', 'textures', 'models', 'Compiled') {
    Sync (Join-Path $engineAssets $folder) (Join-Path $stage "assets\$folder")
}
# Game assets go on top (same folder layout as the engine's assets\Compiled).
Sync $gameAssets (Join-Path $stage 'assets')

$exe = Join-Path $stage 'CaucasianEagle.exe'
$size = '{0:N0} MB' -f ((Get-ChildItem $stage -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
Write-Host "Stage: $stage ($size)"
if (-not (Test-Path $exe)) { Write-Warning "CaucasianEagle.exe is not built yet. Run tools\build.ps1." }
