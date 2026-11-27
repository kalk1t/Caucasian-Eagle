# Her soul's test (0.5.4, CAUCASIAN_EAGLE_DEMO=15): runs the staged game unattended. At places along the way she leaves
# her soul (Q), runs on and is taken back to it (Q); at the first place also Q in flight, her window opened and closed
# with her soul away, and a nest's put letting it go; at the second, Q again from the air (EaglesAscentGame.h:
# SoulTest). Prints soul.tsv, which ends with SOUL_PASS or SOUL_FAIL. Its settings are in a folder of its own
# (%LOCALAPPDATA%\CaucasianEagle-test, removed after), its save beside its evidence; -Shots: pictures of the first three
# places (and of any that fail), and of the HUD and her window at the first. Every frame of her ways back and of the
# check after each: soul-trace.tsv beside it. CAUCASIAN_EAGLE_SOUL_PUT=0: the map's rules on her way back too (a
# baseline; so 7 places of 17 failed at three times her pace, 2026-11-27).
#   tools\soul-test.ps1 [-Out <folder>] [-Every <m>] [-From <m>] [-Shots]
param([string]$Out = (Join-Path $env:TEMP 'caucasian-eagle-soul'), [double]$Every = 400, [double]$From = 60, [switch]$Shots)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repo 'build\stage\CaucasianEagle\CaucasianEagle.exe'
if (-not (Test-Path $exe)) { throw "Not staged: $exe (tools\build.ps1)" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File -ErrorAction SilentlyContinue | Remove-Item -Force
$table = Join-Path $Out 'soul.tsv'
$inv = [Globalization.CultureInfo]::InvariantCulture
$names = 'CAUCASIAN_EAGLE_DEMO', 'CAUCASIAN_EAGLE_MAP_LOG', 'CAUCASIAN_EAGLE_TOUR_SHOTS', 'CAUCASIAN_EAGLE_SOUL_EVERY', 'CAUCASIAN_EAGLE_SOUL_FROM', 'CAUCASIAN_EAGLE_USER_FOLDER'
$env:CAUCASIAN_EAGLE_DEMO = '15'
$env:CAUCASIAN_EAGLE_MAP_LOG = Join-Path $Out 'evidence.tsv'
$env:CAUCASIAN_EAGLE_SOUL_EVERY = $Every.ToString($inv)
$env:CAUCASIAN_EAGLE_SOUL_FROM = $From.ToString($inv)
$env:CAUCASIAN_EAGLE_USER_FOLDER = 'CaucasianEagle-test'
if ($Shots) { $env:CAUCASIAN_EAGLE_TOUR_SHOTS = $Out }
try { $game = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru }
finally { $names | ForEach-Object { Remove-Item "env:$_" -ErrorAction SilentlyContinue } }
$taken = @{}
$deadline = (Get-Date).AddMinutes(20)
while (-not $game.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    if (-not $Shots -or -not (Test-Path $table)) { continue }
    foreach ($line in @(Get-Content $table -ErrorAction SilentlyContinue)) {
        if ($line -notmatch '^picture\t(.+)$') { continue }
        $name = $Matches[1]
        if ($taken.ContainsKey($name)) { continue }
        $taken[$name] = $true
        Start-Sleep -Milliseconds 400   # (the test holds 2 s and more after asking)
        try { & (Join-Path $PSScriptRoot 'release\Save-GameWindow.ps1') -ProcessId $game.Id -Out (Join-Path $Out "$name.png") | Out-Null }
        catch { "picture $name not saved: $_" }
    }
}
if (-not $game.HasExited) { "The test ran past 20 minutes; the game is left running (pid $($game.Id))."; exit 3 }
$own = Join-Path $env:LOCALAPPDATA 'CaucasianEagle-test'
if (Test-Path $own) { Remove-Item -Recurse -Force $own }
if (Test-Path $table) { Get-Content $table } else { 'No soul.tsv: the test did not start.' }
"evidence in $Out"
exit $game.ExitCode
