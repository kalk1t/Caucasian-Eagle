# The Ascent's nature test (grass, trees, her menu), unattended, on the staged game: pictures and a table.
#   tools\nature-test.ps1 [-Out build\nature\<name>] [-NoGameTrees] [-Timeout 420]
# Writes <Out>\nature.tsv (the test's rows, frame times and the trees drawn), <Out>\shots\*.bmp (the game's own frames)
# and <Out>\evidence.tsv. -NoGameTrees: CAUCASIAN_EAGLE_GAME_TREES=0 (before/after pictures). Exit 0 when NATURE_PASS.
param([string]$Out = '', [switch]$NoGameTrees, [int]$Timeout = 420)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $repo ("build\nature\" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$Out = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))
New-Item -ItemType Directory -Force $Out, (Join-Path $Out 'shots') | Out-Null
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
$env:CAUCASIAN_EAGLE_DEMO = '5'
$env:CAUCASIAN_EAGLE_MAP_LOG = (Join-Path $Out 'evidence.tsv')
$env:CAUCASIAN_EAGLE_TOUR_SHOTS = (Join-Path $Out 'shots')
$env:CAUCASIAN_EAGLE_GAME_TREES = $(if ($NoGameTrees) { '0' } else { '' })
try {
    $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
    if (-not $p.WaitForExit($Timeout * 1000)) { Stop-Process -Id $p.Id -Force; throw "The game did not finish within $Timeout s" }
} finally {
    foreach ($name in 'CAUCASIAN_EAGLE_DEMO', 'CAUCASIAN_EAGLE_MAP_LOG', 'CAUCASIAN_EAGLE_TOUR_SHOTS', 'CAUCASIAN_EAGLE_GAME_TREES') { Remove-Item "env:$name" -ErrorAction SilentlyContinue }
}
$table = Join-Path $Out 'nature.tsv'
if (-not (Test-Path $table)) { throw "No nature.tsv in $Out (exit code $($p.ExitCode))" }
Get-Content $table
if (Select-String -LiteralPath $table -Pattern 'NATURE_PASS' -Quiet) { exit 0 } else { exit 1 }

