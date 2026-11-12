# Packages Caucasian Eagle for distribution.
#
#   dist\CaucasianEagle-<version>-win64\        the game, assembled fresh from the two repositories
#   dist\CaucasianEagle-<version>-win64.zip     portable package
#   dist\CaucasianEagle-<version>-win64.zip.sha256
#   dist\CaucasianEagle-<version>-Setup.exe     installer, only when Inno Setup 6 is installed
#   dist\CaucasianEagle-<version>-Setup.exe.sha256
#
# <version> comes from GameVersion in game\src\GameConstants.h.
#
# The package folder is not a copy of the stage folder: the stage keeps files of older builds
# (robocopy never deletes there). It is assembled from engine\assets and game\assets the way
# tools\stage.ps1 does it, plus the executable and the Visual C++ runtime DLLs beside it, so the
# game starts on a PC without Visual Studio.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-SkipBuild] [-Exe <path>] [-NoZip]
#   -SkipBuild   use the executable already in build\stage\CaucasianEagle
#   -Exe <path>  use this executable instead (it is packed as CaucasianEagle.exe); implies -SkipBuild
#   -NoZip       only the folder and the installer
param([switch]$SkipBuild, [string]$Exe, [switch]$NoZip)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$constants = Get-Content 'game\src\GameConstants.h' -Raw
if ($constants -notmatch 'GameVersion\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)"') { throw 'GameVersion not found in game\src\GameConstants.h' }
$version = $Matches[1]

if (-not $Exe) {
    if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release }
    $Exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
}
if (-not (Test-Path $Exe)) { throw "The executable is not built: $Exe" }

$dist = Join-Path $repo 'dist'
$name = "CaucasianEagle-$version-win64"
$out = Join-Path $dist $name
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

function Sync([string]$from, [string]$to) {
    & robocopy $from $to /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 /XF *.pdb *.ilk | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $from -> $to" }
}

# The same content as tools\stage.ps1: engine runtime assets, then the game's on top.
foreach ($folder in 'shaders', 'ui', 'textures', 'models', 'Compiled') {
    Sync (Join-Path $repo "engine\assets\$folder") (Join-Path $out "assets\$folder")
}
Sync (Join-Path $repo 'game\assets') (Join-Path $out 'assets')
Copy-Item $Exe (Join-Path $out 'CaucasianEagle.exe')

# Visual C++ runtime, beside the executable (the build links it dynamically).
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -prerelease -property installationPath | Select-Object -First 1
$crt = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Directory | Where-Object { $_.Name -match '^\d' } |
    Sort-Object Name -Descending | ForEach-Object { Get-ChildItem (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' } |
    Select-Object -First 1
if (-not $crt) { throw 'Visual C++ redistributable folder not found in the Visual Studio installation.' }
foreach ($dll in 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
    Copy-Item (Join-Path $crt.FullName $dll) $out
}

$size = (Get-ChildItem $out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("Folder: {0} ({1:N0} MB)" -f $out, $size)

function Hash([string]$file) {
    $hash = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
    "$hash  $(Split-Path -Leaf $file)" | Set-Content -Encoding ascii "$file.sha256"
}

if (-not $NoZip) {
    $zip = Join-Path $dist "$name.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path $out -DestinationPath $zip -CompressionLevel Optimal
    Hash $zip
    Write-Host ("Package: {0} ({1:N0} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
}

$iscc = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($iscc) {
    & $iscc /Qp "/DAppVersion=$version" "/DSourceDir=$out" "/DOutputDir=$dist" (Join-Path $PSScriptRoot 'installer\CaucasianEagle.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compile failed.' }
    $setup = Join-Path $dist "CaucasianEagle-$version-Setup.exe"
    Hash $setup
    Write-Host ("Installer: {0} ({1:N0} MB)" -f $setup, ((Get-Item $setup).Length / 1MB))
} else {
    Write-Warning 'Inno Setup 6 is not installed, so no Setup.exe was built. Install it (winget install JRSoftware.InnoSetup) and run this script again.'
}
