# Map Lab -> the game: copies a map's last Map Lab build into game\assets, so the game can play it
# (Map A, the Eagle's Ascent: CaucasianEagle.exe --eagles-ascent) and tools\stage.ps1 ships it.
#
#   tools\map-to-game.ps1                    Map A: scene MapA_EaglesAscent, data file EaglesAscent.txt
#   tools\map-to-game.ps1 -Map MapA -Scene MapA_EaglesAscent -As EaglesAscent
#
# From build\map-lab\content\assets (tools\map-lab.ps1 -Map MapA -PrepareAssets -StageOnly makes it) to game\assets:
#   Compiled\Scenes\<Scene>.dscene and Compiled\{StaticMeshes,Materials,Collision,Textures}\<Scene>\ (mirrored: pictures
#   an older build named differently are removed), Lab\Maps\<Map>.txt and .heights -> Maps\<As>.txt and .heights.
# Run the lab's tests on that build first (-Smoke, -Cross, -Walk, -Miss). Nothing is committed.
param(
    [string]$Map = 'MapA',
    [string]$Scene = 'MapA_EaglesAscent',
    [string]$As = 'EaglesAscent'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$from = Join-Path $repo 'build\map-lab\content\assets'
$to = Join-Path $repo 'game\assets'
$dscene = Join-Path $from "Compiled\Scenes\$Scene.dscene"
$data = Join-Path $from "Lab\Maps\$Map.txt"
foreach ($need in $dscene, $data, [IO.Path]::ChangeExtension($data, '.heights')) {
    if (-not (Test-Path -LiteralPath $need)) { throw "Not built yet: $need (tools\map-lab.ps1 -Map $Map -PrepareAssets -StageOnly)" }
}
foreach ($kind in 'StaticMeshes', 'Materials', 'Collision', 'Textures') {
    $source = Join-Path $from "Compiled\$kind\$Scene"
    if (-not (Test-Path -LiteralPath $source)) { continue }
    & robocopy $source (Join-Path $to "Compiled\$kind\$Scene") /MIR /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $source" }
}
New-Item -ItemType Directory -Force (Join-Path $to 'Compiled\Scenes'), (Join-Path $to 'Maps') | Out-Null
Copy-Item -LiteralPath $dscene -Destination (Join-Path $to "Compiled\Scenes\$Scene.dscene") -Force
Copy-Item -LiteralPath $data -Destination (Join-Path $to "Maps\$As.txt") -Force
Copy-Item -LiteralPath ([IO.Path]::ChangeExtension($data, '.heights')) -Destination (Join-Path $to "Maps\$As.heights") -Force
foreach ($splat in 'splat_a', 'splat_b') {   # the ground's splat for the terrain shader (testing/MapLab/terrain_splat.py)
    $file = [IO.Path]::ChangeExtension($data, ".$splat.dds")
    if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination (Join-Path $to "Maps\$As.$splat.dds") -Force }
}
$size = (Get-ChildItem -LiteralPath (Join-Path $to 'Compiled') -Recurse -File | Where-Object { $_.FullName -match [regex]::Escape($Scene) } | Measure-Object Length -Sum).Sum
Write-Host ("Copied $Map ($Scene) into game\assets: {0:N0} MB compiled, data file Maps\$As.txt" -f ($size / 1MB))

exit 0
