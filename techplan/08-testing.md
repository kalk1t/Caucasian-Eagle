# 11. Testing

[← Index](TECHNICAL_DOCUMENTATION.md)

## 11.1 What exists in v0.1

| Check | How | Automated | Covers |
|---|---|---|---|
| Build | `tools\build.ps1` | yes (one command) | Engine library and game compile and link with warnings-as-errors for the listed warning numbers |
| Character pipeline gate | `DoorwaysAssetBuilder character-pipeline ...` prints `[PASS]` and the rig signature | yes | Mesh, skeleton and clips are consistent; altered-bind and reordered-joint rejection |
| Start-up | Run the staged exe; debug output contains `[PASS] Caucasian Eagle: Nyxara (EagleV1) is the player and her combat kit started.` | manual | Assets found, hero loaded, kit started |
| Scripted showcase | `CAUCASIAN_EAGLE_DEMO=1`, watch or capture frames | manual review | Skills, combos, barrier, swap, travel, HUD on screen |
| Engine sandbox | Build and run `Doorways.exe` in the engine repository | manual | The engine still works without a game |
| Engine unit tests | `DoorwaysRuntimeTests` in the engine repository | yes | Engine logic only; nothing game-specific |

There are **no automated tests of game code**. `NyxaraKit` has no unit tests, and nothing checks the game with real keyboard and mouse input other than a person playing it.

## 11.2 What was verified for v0.1.0

- [confirmed] The solution builds in Release x64 from the game repository against the engine submodule.
- [confirmed] The staged game starts from `build\stage\CaucasianEagle`, using only assets tracked in the two repositories, and runs the showcase (frame checked: hero, shades, barrier, HUD with F/V/C slots).
- [confirmed] The engine sandbox builds and reaches its main menu after the split.
- [not verified] Playing with real input after the split (the pre-split build was played by the author).
- [not verified] A clean clone on a second machine.
- [not verified] An installed copy outside the repository folder.

## 11.3 Manual test checklist (run before each release)

1. Main menu appears; window title reads "Caucasian Eagle"; Play enters Moonlit Sanctuary.
2. WASD moves her in upright flight; releasing the keys lands her; Shift is faster and leaves a feather trail; Ctrl walks.
3. Space jumps; holding Space flies up.
4. LMB alternates left sword, right sword; RMB is the charged slash.
5. F, V and C each fire at once, including in the middle of a sword attack; each shows its cooldown in the skill bar.
6. After each skill, LMB and RMB give that skill's two combos while the gold COMBO prompt shows.
7. Q swaps the swords and the HUD sword labels swap; E raises the barrier and blocks shade orbs ("BLOCKED").
8. Shades take damage, their bars flash, they die and return after about 8 seconds.
9. Taking orb hits lowers the health bar; death and respawn work.
10. Mouse look: the view can be raised to level and a little above, and lowered to steeply downward.
11. Settings screens open and a changed setting survives a restart (`%LOCALAPPDATA%\CaucasianEagle\PlayerPreferences.ini`).
12. Exit from the menu closes the process.

## 11.4 Planned

| Item | Milestone |
|---|---|
| Clean-machine build and install test | M2 |
| A headless smoke test: launch with the showcase variable, exit code 0 after N seconds, required log line present | M2 |
| Unit tests for the kit's pure logic (action table, cancel rules, cooldowns) after it is separated from rendering | M4 |
| Performance budget check (frame time in the showcase) | M5 |
