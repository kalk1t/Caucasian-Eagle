# Signs Caucasian Eagle's own files with Authenticode and a trusted timestamp, then verifies each (release audit
# 2026-11-20: v0.5 shipped with nothing signed, so Windows names no publisher and SmartScreen / Smart App Control have
# no publisher reputation to go on).
#
#   tools\release\Sign-Files.ps1 -Files dist\...\CaucasianEagle.exe [more files]
#
# How to sign is read from tools\release\signing.local.json (git-ignored; copy signing.template.json) or from the
# environment, never from the command line, and nothing secret is printed:
#   Mode "certificate": a code-signing certificate in the Windows certificate store (CurrentUser\My or LocalMachine\My)
#                       named by Thumbprint (a hardware token's certificate shows there too); TimestampUrl from its CA.
#   Mode "artifact":    Microsoft Artifact Signing: Dlib (Azure.CodeSigning.Dlib.dll, x64, from the NuGet package
#                       Microsoft.ArtifactSigning.Client) and Metadata (a json with Endpoint, CodeSigningAccountName,
#                       CertificateProfileName); signing in through Azure (az login or the usual Azure environment
#                       credentials). TimestampUrl http://timestamp.acs.microsoft.com.
# Environment instead of the file: CE_SIGN_MODE, CE_SIGN_THUMBPRINT, CE_SIGN_DLIB, CE_SIGN_METADATA, CE_SIGN_TIMESTAMP.
#
# A file that already carries a valid signature of another publisher (the Visual C++ runtime DLLs: Microsoft) is left
# as it is. Exit code 0: every file signed and verified; 2: not configured; 1: a signature failed.
param([Parameter(Mandatory)] [string[]]$Files)
$ErrorActionPreference = 'Stop'

$config = @{}
$local = Join-Path $PSScriptRoot 'signing.local.json'
if (Test-Path $local) { (Get-Content $local -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $config[$_.Name] = [string]$_.Value } }
foreach ($pair in @{ Mode = 'CE_SIGN_MODE'; Thumbprint = 'CE_SIGN_THUMBPRINT'; Dlib = 'CE_SIGN_DLIB'; Metadata = 'CE_SIGN_METADATA'; TimestampUrl = 'CE_SIGN_TIMESTAMP' }.GetEnumerator()) {
    $v = [Environment]::GetEnvironmentVariable($pair.Value); if ($v) { $config[$pair.Key] = $v }
}
if (-not $config.Mode) { Write-Host 'SIGNING NOT CONFIGURED: no tools\release\signing.local.json and no CE_SIGN_MODE (see the head of this script).'; exit 2 }

$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object FullName -match '\\x64\\' | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $signtool) { throw 'signtool.exe (Windows SDK) not found.' }

$common = @('sign', '/fd', 'SHA256', '/td', 'SHA256')
switch ($config.Mode) {
    'certificate' {
        if (-not $config.Thumbprint) { throw 'Mode certificate needs Thumbprint.' }
        $ts = if ($config.TimestampUrl) { $config.TimestampUrl } else { throw 'Mode certificate needs the TimestampUrl of the certificate''s CA.' }
        $how = @('/sha1', $config.Thumbprint, '/tr', $ts)
        if (Get-ChildItem Cert:\LocalMachine\My -ErrorAction SilentlyContinue | Where-Object Thumbprint -eq $config.Thumbprint) { $how += '/sm' }
    }
    'artifact' {
        foreach ($k in 'Dlib', 'Metadata') { if (-not $config[$k] -or -not (Test-Path $config[$k])) { throw "Mode artifact needs $k (a file that exists)." } }
        $ts = if ($config.TimestampUrl) { $config.TimestampUrl } else { 'http://timestamp.acs.microsoft.com' }
        $how = @('/tr', $ts, '/dlib', $config.Dlib, '/dmdf', $config.Metadata)
    }
    default { throw "Unknown signing mode '$($config.Mode)' (certificate or artifact)." }
}

$failed = 0
foreach ($file in $Files) {
    $path = (Resolve-Path $file).Path
    $before = Get-AuthenticodeSignature $path
    if ($before.Status -eq 'Valid' -and $before.SignerCertificate.Subject -notmatch 'Samkharadze|Caucasian Eagle') {
        Write-Host "kept (signed by $($before.SignerCertificate.Subject.Split(',')[0])): $(Split-Path $path -Leaf)"; continue
    }
    & $signtool @common @how $path | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "SIGN FAILED: $path"; $failed++; continue }
    & $signtool verify /pa /tw $path | Out-Null
    $after = Get-AuthenticodeSignature $path
    if ($LASTEXITCODE -ne 0 -or $after.Status -ne 'Valid' -or -not $after.TimeStamperCertificate) { Write-Host "SIGNATURE INVALID: $path ($($after.Status))"; $failed++; continue }
    Write-Host "signed and verified: $(Split-Path $path -Leaf) by $($after.SignerCertificate.Subject.Split(',')[0]), timestamped by $($after.TimeStamperCertificate.Subject.Split(',')[0])"
}
exit $(if ($failed) { 1 } else { 0 })
