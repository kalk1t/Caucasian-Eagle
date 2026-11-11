# 11. Testing

[← Index](TECHNICAL_DOCUMENTATION.md)

## 11.1 What exists

| Check | How | Automated | Covers |
|---|---|---|---|
| Build | `tools\build.ps1` | yes (one command) | Engine library and game compile and link with warnings-as-errors for the listed warning numbers |
| Character pipeline gate | `DoorwaysAssetBuilder character-pipeline ...` prints `[PASS]` and the rig signature | yes | Mesh, skeleton and clips are consistent; altered-bind and reordered-joint rejection |
| Start-up | Run the staged exe; debug output contains `[PASS] Caucasian Eagle: Nyxara (EagleV1) is the player and her combat kit started.` | manual | Assets found, hero loaded, kit started |
| Scripted showcase | `CAUCASIAN_EAGLE_DEMO=1`, watch or capture frames | manual review | Skills, combos, barrier, swap, travel, HUD on screen |
| **Test pilot** | `CAUCASIAN_EAGLE_DEMO=1` and `CAUCASIAN_EAGLE_PILOT=1`; read the `[JOURNEY]` lines in the debugger output ([03-workflows.md](03-workflows.md), 6.11) | unattended; a person reads the log | The whole journey: road, gates, every arena and wave, roaming packs, chests, bosses, the Eagle, the chain, the ending. Not difficulty (the pilot cannot die), not death and respawn, not saving. |
| Engine sandbox | Build and run `Doorways.exe` in the engine repository | manual | The engine still works without a game |
| Engine unit tests | `DoorwaysRuntimeTests` in the engine repository | yes | Engine logic only; nothing game-specific |

There are **no unit tests of game code**. The test pilot is the only automated exercise of `Journey` and `NyxaraKit`, and it drives the kit through its scripted-key path, not through the keyboard. Nothing checks the game with real keyboard and mouse input other than a person playing it.

## 11.2 What was verified for v0.1.0

- [confirmed] The solution builds in Release x64 from the game repository against the engine submodule.
- [confirmed] The staged game starts from `build\stage\CaucasianEagle`, using only assets tracked in the two repositories, and runs the showcase (frame checked: hero, shades, barrier, HUD with F/V/C slots).
- [confirmed] The engine sandbox builds and reaches its main menu after the split.
- [not verified] Playing with real input after the split (the pre-split build was played by the author).
- [not verified] A clean clone on a second machine.
- [not verified] An installed copy outside the repository folder.

## 11.2a What was verified for v0.2.0

PILOT_RESULTS

Problems the pilot found, all fixed in this version:

| Problem | Cause | Fix |
|---|---|---|
| Stuck at the end of an arena in the Snow Pass | The engine stops movement dead at the edge of walkable ground; heading for a roadside chest pressed her into the road edge | Collision 3 m wider than the road; `Journey::Leash` slides her along the edge |
| Stuck at 4,148 m | Engine play-area box of ±999 m | Engine `d9028b6`: `SceneCollision` switches the box off |
| A fight that could not end on the Witches' Hill | A witch was knocked out of the arena and could not come back; the player could not follow through the gate | Arena creatures outside the arena are put back on its edge |
| Rokapi's fight lasted 37 minutes | She summoned two witches every 7 to 10 seconds without limit | She summons on every third step and only when fewer than four creatures stand |
| A 4 m wall across the Eagle's terrace | The last path samples were forced to ledge height | The terrace is level with the ledge; the climb comes before it |
| Could not step from the terrace onto the ledge | 0.6 m gap between two collision meshes | They overlap |
| A save made after the Eagle fell but before the chain broke could not continue | The chain phase was only entered from the fight | `Journey::Initialize` resumes it |

- [not verified] **Playing with keyboard and mouse.** Nobody has played v0.2.0 by hand.
- [not verified] **Difficulty.** The pilot's health is refilled every frame. Whether a person survives the later regions at `MonsterDamageScale` 0.8 is not known.
- [not verified] **Death, respawn and the return to the last arena.** The engine's respawn needs a click, and no input is sent to the desktop in testing.
- [not verified] **Save and load in a normal run.** Unattended runs never write the save. The load path was exercised only through the test start variable, which sets the same fields.
- [not verified] **F9 restart**, for the same reason.
- [not verified] **Frame rate** beyond "the pilot's log showed the 60 fps cap in a 1280 x 720 window on the development PC".
- [not verified] A clean clone, a second machine, an installed copy (unchanged from v0.1.0).

## 11.3 Manual test checklist (run before each release)

1. Main menu appears; window title reads "Caucasian Eagle"; Play enters Moonlit Sanctuary; four prologue cards show.
2. WASD moves her in upright flight; releasing the keys lands her; Shift is faster and leaves a feather trail; Ctrl walks.
3. Space jumps; holding Space flies up.
4. LMB alternates left sword, right sword; RMB is the charged slash.
5. F, V and C each fire at once, including in the middle of a sword attack; each shows its cooldown in the skill bar.
6. After each skill, LMB and RMB give that skill's two combos while the gold COMBO prompt shows.
7. Q swaps the swords and the HUD sword labels swap; E raises the barrier and blocks orbs ("BLOCKED").
8. Creatures take damage, their bars flash, they die and do not return.
9. Taking hits lowers the health bar; health returns slowly outside fights.
10. Mouse look: the view can be raised to level and a little above, and lowered to steeply downward.
11. Settings screens open and a changed setting survives a restart (`%LOCALAPPDATA%\CaucasianEagle\PlayerPreferences.ini`).
12. Exit from the menu closes the process.

The journey (new in v0.2.0):

13. The south gate leads onto the causeway; the road panel shows the region and the distance.
14. She cannot leave the road; pushing into its edge slides her along it.
15. The first arena shuts both gates, sends two waves, opens when they are beaten and heals her.
16. A roadside chest breaks under any attack and shows what it held; the gold counter rises.
17. **Die on purpose in the second arena**: the death screen appears, respawn returns her to the first arena, the second arena's fight starts again.
18. Quit after an arena and start again: she stands at that arena, with the same gold, feathers and memories, and opened chests stay open.
19. Hold F9 for two seconds: the journey starts again from the sanctuary.
20. Each of the twelve regions shows its card and looks different from the one before.
21. The three bosses show a health bar and their line; the way opens when they fall.
22. On the terrace the Eagle leaves Amirani's side to fight; after he falls the ledge can be walked, both anchors break, the chains disappear and the ending cards show.

## 11.4 Planned

| Item | Milestone |
|---|---|
| Clean-machine build and install test | M2 |
| A headless smoke test: launch with the test pilot, required `[JOURNEY] COMPLETE` line present within a time limit, as one script | M2 |
| Unit tests for the kit's pure logic (action table, cancel rules, cooldowns) after it is separated from rendering | M4 |
| Performance budget check (frame time in the showcase) | M5 |
