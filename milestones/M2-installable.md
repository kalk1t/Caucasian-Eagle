# M2 · Installable build (v0.3.0)

**Who:** Claude + you · **Needs:** a second PC or a fresh Windows user account for the test · **Next:** [M3](M3-story-spine.md)

**Status, 2026-11-11.** v0.3.0 is released with `CaucasianEagle-0.3.0-Setup.exe`. Done: steps 2, 3, the icon and version information of step 4 (the title and the menus were already the game's own), the runtime of step 6, step 7. Open: step 1 by hand, step 5 (the package still carries all engine assets), the test on a PC that never had the project, the smoke test script.

## Goal
Someone who has never seen the project can download one Setup file, install Caucasian Eagle, play it and uninstall it.

## Steps

### 1. Play check after the split (You, 10 min)
- [ ] Run `tools\build.ps1 -Run` and go through the manual checklist in [techplan/08-testing.md](../techplan/08-testing.md). Tell Claude anything that changed from before the split.

### 2. Install Inno Setup (You, 5 min)
- [x] Inno Setup 6.7.3 installed for the user with `winget install JRSoftware.InnoSetup` (2026-11-11).

### 3. Build the installer (Claude)
- [x] Run `tools\package.ps1`; fix whatever the first compile of `tools/installer/CaucasianEagle.iss` reports.
- [x] Install (silently, into a test folder), start the installed game, uninstall; settings in `%LOCALAPPDATA%\CaucasianEagle` survive. Results in [techplan/08-testing.md](../techplan/08-testing.md). Not done: the wizard's pages and the Start-menu entry by hand.

### 4. Make it look like the game (Claude)
- [x] Game icon and version information in `CaucasianEagle.exe` (`game/CaucasianEagle.rc`, `art/ui/build_icon.py`: the Eagle before the moon). The author has not yet approved the icon.
- [x] The start screen is the game's own (R2), so the engine's "DOORWAYS" menu and its Website and Forum entries are not shown.

### 5. Shrink the package (Claude)
- [ ] Map which engine assets are needed at start-up (question Q1) and stage only those.
- [ ] Target: package under 300 MB.

### 6. Clean-PC test (You + Claude)
- [x] The Visual C++ runtime DLLs are packed beside the executable (`tools/package.ps1`).
- [ ] Install on a PC or Windows account without Visual Studio.
- [ ] Add a smoke test script: start with the showcase variable, check the start-up log line, exit.

### 7. Release v0.3.0
- [x] Follow the release checklist in [techplan/09-build-package-release.md](../techplan/09-build-package-release.md); attach the Setup file to a GitHub release.

## ✅ Done when
- [ ] `CaucasianEagle-0.3.0-Setup.exe` installs and runs on a PC that never had the project (so far: on the development PC only).
- [ ] The window, the menu and the Start-menu entry all say Caucasian Eagle.
- [ ] Uninstall removes the program and keeps the player's settings.
