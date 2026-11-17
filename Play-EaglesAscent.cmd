@echo off
rem The Eagle's Ascent: the game on Map A on its own (Nyxara climbs from the sea to Amiran's court).
rem Plays the staged game (tools\build.ps1 or tools\stage.ps1 make it); the road is CaucasianEagle.exe without the switch.
set "GAME=%~dp0build\stage\CaucasianEagle\CaucasianEagle.exe"
if not exist "%GAME%" (
    echo The game is not built yet: run tools\build.ps1 first.
    pause
    exit /b 1
)
start "" /D "%~dp0build\stage\CaucasianEagle" "%GAME%" --eagles-ascent
