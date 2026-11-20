@echo off
rem The road to Amirani (the game of 0.4). Since 0.5 CaucasianEagle.exe alone starts the Eagle's Ascent (Play-EaglesAscent.cmd).
set "GAME=%~dp0build\stage\CaucasianEagle\CaucasianEagle.exe"
if not exist "%GAME%" (
    echo The game is not built yet: run tools\build.ps1 first.
    pause
    exit /b 1
)
start "" /D "%~dp0build\stage\CaucasianEagle" "%GAME%" --road
