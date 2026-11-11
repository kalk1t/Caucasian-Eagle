# Builds Caucasian Eagle: engine library + game executable, then stages the runtime folder.
#
#   1. initialises the engine submodule if it is missing
#   2. restores the engine's NuGet packages into engine\packages (DirectX Tool Kit)
#   3. builds CaucasianEagle.slnx (DoorwaysEngine.lib, then CaucasianEagle.exe)
#   4. runs tools\stage.ps1
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Configuration Release|Debug] [-Run]
param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path $vswhere) {
    $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio with the Desktop development with C++ workload.' }

if (-not (Test-Path 'engine\src\DOORWAYS\DoorwaysEngine.vcxproj')) {
    git submodule update --init
    if ($LASTEXITCODE -ne 0) { throw 'git submodule update failed.' }
}

if (-not (Test-Path 'engine\packages\directxtk_desktop_win10.2026.5.8.1')) {
    & $msbuild 'engine\src\DOORWAYS\DoorwaysEngine.vcxproj' -t:Restore -p:RestorePackagesConfig=true "-p:SolutionDir=$repo\engine\" -v:minimal -nologo
    if ($LASTEXITCODE -ne 0) { throw 'NuGet restore failed.' }
}

& $msbuild 'CaucasianEagle.slnx' -t:CaucasianEagle "-p:Configuration=$Configuration" -p:Platform=x64 -m -v:minimal -nologo
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

& (Join-Path $PSScriptRoot 'stage.ps1') -Configuration $Configuration

if ($Run) {
    $stageName = if ($Configuration -eq 'Debug') { 'stage-debug' } else { 'stage' }
    Start-Process (Join-Path $repo "build\$stageName\CaucasianEagle\CaucasianEagle.exe")
}
