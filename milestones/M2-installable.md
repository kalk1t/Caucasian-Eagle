# M2 · Installable build (v0.2)

**Who:** Claude + you · **Needs:** Inno Setup 6 installed; a second PC or a fresh Windows user account for the test · **Next:** [M3](M3-story-spine.md)

## Goal
Someone who has never seen the project can download one Setup file, install Caucasian Eagle, play it and uninstall it.

## Steps

### 1. Play check after the split (You, 10 min)
- [ ] Run `tools\build.ps1 -Run` and go through the manual checklist in [techplan/08-testing.md](../techplan/08-testing.md). Tell Claude anything that changed from before the split.

### 2. Install Inno Setup (You, 5 min)
- [ ] Download Inno Setup 6 from https://jrsoftware.org/isinfo.php and install with the defaults. Tell Claude "inno done".

### 3. Build the installer (Claude)
- [ ] Run `tools\package.ps1`; fix whatever the first compile of `tools/installer/CaucasianEagle.iss` reports.
- [ ] Install, launch from the Start menu, uninstall; confirm settings in `%LOCALAPPDATA%\CaucasianEagle` survive.

### 4. Make it look like the game (Claude)
- [ ] Game icon and version information in `CaucasianEagle.exe` (you supply or approve the icon).
- [ ] Main menu shows "CAUCASIAN EAGLE" instead of "DOORWAYS" **(engine)**: a menu-title option in `DoorwaysGame.h`.
- [ ] Hide or repoint the menu's Website and Forum entries **(engine)**.

### 5. Shrink the package (Claude)
- [ ] Map which engine assets are needed at start-up (question Q1) and stage only those.
- [ ] Target: package under 300 MB.

### 6. Clean-PC test (You + Claude)
- [ ] Install on a PC or Windows account without Visual Studio. If it fails to start, bundle the Visual C++ runtime or link it statically.
- [ ] Add a smoke test script: start with the showcase variable, check the start-up log line, exit.

### 7. Release v0.2
- [ ] Follow the release checklist in [techplan/09-build-package-release.md](../techplan/09-build-package-release.md); attach the Setup file to a GitHub release.

## ✅ Done when
- [ ] `CaucasianEagle-0.2.0-Setup.exe` installs and runs on a PC that never had the project.
- [ ] The window, the menu and the Start-menu entry all say Caucasian Eagle.
- [ ] Uninstall removes the program and keeps the player's settings.
