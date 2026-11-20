# The release gate (release audit 2026-11-20): checks a packaged version before it is published, and, with -Published,
# what GitHub serves for it. Run after tools\package.ps1; exit 0 only when every required check passes.
#
#   tools\release\Test-Release.ps1 -Version 0.5.1                  local files in dist\ (signatures required)
#   tools\release\Test-Release.ps1 -Version 0.5.1 -AllowUnsigned    the same, unsigned files reported, not failed
#   tools\release\Test-Release.ps1 -Version 0.5.1 -Published        and every download link of its GitHub release
#   -Scan: Microsoft Defender scans a copy of the installer and the zip (in build\release-check, deleted afterwards)
#
# Checks: GameVersion, the executable's and the installer's version information; each .sha256 against its file; each
# release file under GitHub's 2 GiB limit; the executables shipped (only the game and Microsoft's runtime DLLs, no
# other program); Authenticode signatures with a timestamp on the game, the installer and its uninstaller;
# with -Published: each asset's link answers 200 and gives the bytes whose hash the release states.
param([Parameter(Mandatory)] [string]$Version, [switch]$AllowUnsigned, [switch]$Published, [switch]$Scan)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$dist = Join-Path $repo 'dist'
$bad = New-Object System.Collections.Generic.List[string]; $warn = New-Object System.Collections.Generic.List[string]
function Check([bool]$ok, [string]$what) { if ($ok) { Write-Host "ok    $what" } else { Write-Host "FAIL  $what"; $bad.Add($what) } }

$constants = Get-Content (Join-Path $repo 'game\src\GameConstants.h') -Raw
Check ($constants -match "GameVersion\s*=\s*`"$([regex]::Escape($Version))`"") "GameVersion is $Version"
$folder = Join-Path $dist "CaucasianEagle-$Version-win64"
$zip = "$folder.zip"; $setup = Join-Path $dist "CaucasianEagle-$Version-Setup.exe"
$exe = Join-Path $folder 'CaucasianEagle.exe'
Check (Test-Path $exe) "the package folder holds CaucasianEagle.exe"
if (Test-Path $exe) { Check ((Get-Item $exe).VersionInfo.ProductVersion.Trim([char[]]" `0") -eq $Version) "CaucasianEagle.exe says $Version" }
if (Test-Path $setup) {
    $vi = (Get-Item $setup).VersionInfo
    Check ($vi.ProductVersion.Trim([char[]]" `0") -eq $Version) "the installer says $Version"
    Check ([bool]$vi.LegalCopyright -and [bool]$vi.CompanyName) "the installer names its publisher and copyright"
}
foreach ($file in $zip, $setup) {
    if (-not (Test-Path $file)) { Check $false "$(Split-Path $file -Leaf) exists"; continue }
    $stated = ((Get-Content "$file.sha256" -ErrorAction SilentlyContinue) -split '\s+')[0]
    Check ($stated -eq (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()) "$(Split-Path $file -Leaf) matches its .sha256"
    Check ((Get-Item $file).Length -lt 2GB) ("$(Split-Path $file -Leaf) is under GitHub's 2 GiB limit ({0:N0} MiB)" -f ((Get-Item $file).Length / 1MB))
}
if (Test-Path $folder) {
    $programs = Get-ChildItem $folder -Recurse -File -Include *.exe, *.dll, *.sys, *.scr, *.com, *.msi, *.ps1, *.vbs, *.js, *.bat, *.cmd
    $expected = 'CaucasianEagle.exe', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'Play-TheRoad.cmd'
    $other = @($programs | Where-Object { $expected -notcontains $_.Name } | ForEach-Object { $_.FullName.Substring($folder.Length + 1) })
    Check ($other.Count -eq 0) "no other program ships ($($other -join ', '))"
    foreach ($dll in 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
        $s = Get-AuthenticodeSignature (Join-Path $folder $dll)
        Check ($s.Status -eq 'Valid' -and $s.SignerCertificate.Subject -match 'Microsoft') "$dll keeps Microsoft's signature"
    }
}
$ours = @($exe, $setup)   # (the uninstaller is inside the installer: checked below once installed)
foreach ($file in $ours) {
    if (-not (Test-Path $file)) { continue }
    $s = Get-AuthenticodeSignature $file
    $ok = $s.Status -eq 'Valid' -and $null -ne $s.TimeStamperCertificate
    $label = "$(Split-Path $file -Leaf) is signed and timestamped ($($s.Status))"
    if ($ok) { Write-Host "ok    $label" } elseif ($AllowUnsigned) { Write-Host "UNSIGNED  $label"; $warn.Add("UNSIGNED: $(Split-Path $file -Leaf)") } else { Check $false $label }
}
$installed = Join-Path $env:LOCALAPPDATA 'Programs\Caucasian Eagle\unins000.exe'
if (Test-Path $installed) {
    $s = Get-AuthenticodeSignature $installed
    $label = "the installed uninstaller is signed ($($s.Status))"
    if ($s.Status -eq 'Valid') { Write-Host "ok    $label" } elseif ($AllowUnsigned) { Write-Host "UNSIGNED  $label"; $warn.Add('UNSIGNED: unins000.exe') } else { Check $false $label }
}
if ($Scan) {
    $tmp = Join-Path $repo 'build\release-check'; New-Item -ItemType Directory -Force $tmp | Out-Null
    $mp = Join-Path $env:ProgramFiles 'Windows Defender\MpCmdRun.exe'
    foreach ($file in $setup, $zip) {
        if (-not (Test-Path $file)) { continue }
        $copy = Join-Path $tmp (Split-Path $file -Leaf); Copy-Item $file $copy -Force
        $out = & $mp -Scan -ScanType 3 -File $copy -DisableRemediation 2>&1
        Check ($LASTEXITCODE -eq 0 -and ($out -join ' ') -match 'found no threats') "Defender finds no threat in $(Split-Path $file -Leaf)"
        Remove-Item $copy -Force -ErrorAction SilentlyContinue
    }
}
if ($Published) {
    $release = gh release view "v$Version" --json assets | ConvertFrom-Json
    $sums = @{}
    foreach ($a in $release.assets) {
        $url = "https://github.com/kalk1t/Caucasian-Eagle/releases/download/v$Version/$($a.name)"
        $tmp = New-TemporaryFile
        $code = & curl.exe -sSL -o $tmp.FullName -w '%{http_code}' $url
        $size = (Get-Item $tmp.FullName).Length
        Check ($code -eq '200' -and $size -eq $a.size) "public link of $($a.name) answers $code with $size of $($a.size) bytes"
        if ($a.name -like '*.sha256' -and $code -eq '200') { $line = (Get-Content $tmp.FullName) -split '\s+'; $sums[$line[1]] = $line[0] }
        elseif ($code -eq '200') { $sums["have:$($a.name)"] = (Get-FileHash $tmp.FullName -Algorithm SHA256).Hash.ToLower() }
        Remove-Item $tmp.FullName -Force
    }
    foreach ($k in @($sums.Keys | Where-Object { $_ -notlike 'have:*' })) {
        Check ($sums["have:$k"] -eq $sums[$k]) "the downloaded $k matches the published checksum"
    }
}
Write-Host ''
if ($warn.Count) { Write-Host "Warnings: $($warn -join '; ')" }
if ($bad.Count) { Write-Host "RELEASE CHECK FAILED ($($bad.Count))"; exit 1 }
Write-Host $(if ($warn.Count) { 'RELEASE CHECK PASSED WITH UNSIGNED FILES (not for publication without the owner''s decision)' } else { 'RELEASE CHECK PASSED' })
