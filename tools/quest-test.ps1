# Her quests' test (0.5.3, CAUCASIAN_EAGLE_DEMO=11): runs the staged game unattended, and saves a picture of
# its window with the HUD wherever the test holds for one (a "picture" row in quests.tsv). Prints quests.tsv and ends
# with QUESTS_PASS or QUESTS_FAIL.
#   tools\quest-test.ps1 [-Out <folder>]
param([string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-quests'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$table = Join-Path $Out 'quests.tsv'
$env:CAUCASIAN_EAGLE_DEMO = '11'
$env:CAUCASIAN_EAGLE_MAP_LOG = Join-Path $Out 'evidence.tsv'
$env:CAUCASIAN_EAGLE_TOUR_SHOTS = $Out
try { $game = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
finally { 'CAUCASIAN_EAGLE_DEMO', 'CAUCASIAN_EAGLE_MAP_LOG', 'CAUCASIAN_EAGLE_TOUR_SHOTS' | ForEach-Object { Remove-Item "env:$_" -ErrorAction SilentlyContinue } }
$taken = @{}
$deadline = (Get-Date).AddMinutes(8)
while (-not $game.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    if (-not (Test-Path $table)) { continue }
    foreach ($line in @(Get-Content $table -ErrorAction SilentlyContinue)) {
        if ($line -notmatch '^picture\t(.+)$') { continue }
        $name = $Matches[1]
        if ($taken.ContainsKey($name)) { continue }
        $taken[$name] = $true
        Start-Sleep -Milliseconds 700   # (the hold is 2.5 s: the panel faded in)
        try { & (Join-Path $PSScriptRoot 'release\Save-GameWindow.ps1') -ProcessId $game.Id -Out (Join-Path $Out "$name.png") | Out-Null }
        catch { "picture $name not saved: $_" }
    }
}
if (-not $game.HasExited) { "The test ran past 8 minutes; the game is left running (pid $($game.Id))."; exit 3 }
if (Test-Path $table) { Get-Content $table } else { 'No quests.tsv: the test did not start.' }
"pictures in $Out"
exit $game.ExitCode
