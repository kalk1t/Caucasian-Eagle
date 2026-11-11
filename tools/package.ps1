# Packages Caucasian Eagle for distribution.
#
#   dist\CaucasianEagle-<version>-win64\        clean copy of the stage folder (no .pdb / .ilk)
#   dist\CaucasianEagle-<version>-win64.zip     portable package
#   dist\CaucasianEagle-<version>-win64.zip.sha256
#   dist\CaucasianEagle-<version>-Setup.exe     installer, only when Inno Setup 6 is installed
#
# <version> comes from GameVersion in game\src\GameConstants.h.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-SkipBuild]
param([switch]$SkipBuild)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$constants = Get-Content 'game\src\GameConstants.h' -Raw
if ($constants -notmatch 'GameVersion\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)"') { throw 'GameVersion not found in game\src\GameConstants.h' }
$version = $Matches[1]

if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release }

$stage = Join-Path $repo 'build\stage\CaucasianEagle'
if (-not (Test-Path (Join-Path $stage 'CaucasianEagle.exe'))) { throw "Stage is not built: $stage" }

$dist = Join-Path $repo 'dist'
$name = "CaucasianEagle-$version-win64"
$out = Join-Path $dist $name
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

& robocopy $stage $out /E /NFL /NDL /NJH /NJS /NP /XF *.pdb *.ilk | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $out -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $name.zip" | Set-Content -Encoding ascii "$zip.sha256"
Write-Host ("Package: {0} ({1:N0} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))

$iscc = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($iscc) {
    & $iscc "/DAppVersion=$version" "/DSourceDir=$out" "/DOutputDir=$dist" (Join-Path $PSScriptRoot 'installer\CaucasianEagle.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compile failed.' }
    Write-Host "Installer: $dist\CaucasianEagle-$version-Setup.exe"
} else {
    Write-Warning 'Inno Setup 6 is not installed, so no Setup.exe was built. Install it from https://jrsoftware.org/isinfo.php and run this script again.'
}
