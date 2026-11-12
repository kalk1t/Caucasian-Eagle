# 11. Testing

[← Index](TECHNICAL_DOCUMENTATION.md)

## 11.1 What exists

| Check | How | Automated | Covers |
|---|---|---|---|
| Build | `tools\build.ps1` | yes (one command) | Engine library and game compile and link with warnings-as-errors for the listed warning numbers |
| Character pipeline gate | `DoorwaysAssetBuilder character-pipeline ...` prints `[PASS]` and the rig signature | yes | Mesh, skeleton and clips are consistent; altered-bind and reordered-joint rejection |
| Sound | Run the unattended pilot with the game window in front; debug output contains one line `Doorways Audio: game sound loaded: <file>` per file the first time it plays, and no line `sound file missing or not PCM 16-bit WAV`. Seen on 2026-11-11 for music, wind, flight, swings, skills, hits, blows, creature sounds, crystals and a memory line. Nobody has listened to the result: levels, timing and quality are untested | manual |
| Start-up | Run the staged exe; debug output contains `[PASS] Caucasian Eagle: Nyxara (EagleV1) is the player and her combat kit started.` | manual | Assets found, hero loaded, kit started |
| Scripted showcase | `CAUCASIAN_EAGLE_DEMO=1`, watch or capture frames | manual review | Skills, combos, barrier, swap, travel, HUD on screen |
| **Test pilot** | `CAUCASIAN_EAGLE_DEMO=1` and `CAUCASIAN_EAGLE_PILOT=1`; read the `[JOURNEY]` lines in the debugger output ([03-workflows.md](03-workflows.md), 6.11) | unattended; a person reads the log | The whole journey: road, gates, every arena and wave, roaming packs, chests, bosses, the Eagle, the chain, the ending. With `CAUCASIAN_EAGLE_MORTAL=1` also death, respawn and the return to the last arena. Not saving, not real input. |
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

All of this was verified by the test pilot on the development PC (1280 x 720 window), not by a person playing.

| What | Result |
|---|---|
| Whole journey, one run from the sanctuary, pilot that cannot die, speed 8 | `[JOURNEY] COMPLETE` at **7517 s played (125 minutes)**: 34 arenas, three bosses, the Eagle (fight began at 7256 s), both chain anchors, ending. No stall. 60 fps (the cap) in every `[PILOT]` line. |
| The same with the first tuning (three waves until region 6, creature health 1.5) | Complete at 6385 s played (106 minutes). The tuning was then lengthened. |
| The same with four waves from region 3 and creature health 1.6 | Complete at 6888 s played (115 minutes). Creature health was then raised to 1.8. |
| Death, respawn, return to the last arena (`CAUCASIAN_EAGLE_MORTAL=1`) | Works: each death removes the fight, the respawn puts her at the last arena won, the arena starts again. Seen more than 40 times in the log. The respawn is asked for in code (`CombatSystem::RequestRespawn`), the same request the death screen's button makes; the button itself was not clicked. |
| Difficulty, measured with a pilot that can die and never dodges or guards | From the start: no death in arenas 0 to 11, first death in arena 12 (Stone Towers), then it wins. In arena 28 (Witches' Hill, six witches) it lasts 70 to 100 s and does not win. Before the mercy time, the attack turns and the healing on kills were added it died there in 10 s. |
| Mountain in view from the sanctuary, 1.9 km away | Yes (camera held on it with `CAUCASIAN_EAGLE_VISTA=1`). |
| Settings window, keys played into it on the start screen (`CAUCASIAN_EAGLE_SETTINGS_KEYS`), 2026-11-11 | Vertical sync, shadows, three volumes, mouse speed, reversed look, interface size and frame counter each changed in the log and in `PlayerPreferences.ini`; "Restore the defaults" reset them. Window: whole screen to 1280x720 and back, once reverted with Esc and twice kept with Enter (`Doorways Display: Keep`). Not tried by hand: the mouse on its rows. |
| No sanctuary, 2026-11-11: pilot from the head of the road, speed 3, mortal | Starts at 2 m, four arenas won in 360 s played, no death, no warning about the spawn point. The return to the start after a death before the first arena was not exercised. |
| Chest panel at the right edge, road panel, boss bar | Seen in captured frames; no overlap found. |

**Length.** 125 minutes is the time of a pilot that never stops, never dies, never looks around and always knows the way. A person will take longer; how much longer is not measured.

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

## 11.2b What was verified for the v0.3.0 package (2026-11-11)

All on the development PC, which has Visual Studio and has run the game before.

| What | Result |
|---|---|
| `tools\package.ps1 -SkipBuild` | Folder 916 MB, zip 781 MB, `CaucasianEagle-0.3.0-Setup.exe` 735 MB, both SHA-256 files written. Inno Setup 6.7.3 compiled the script without a warning shown. |
| Silent install (`/VERYSILENT /NOICONS /DIR=<test folder>`) | Exit code 0; the folder holds the executable, `assets`, the three runtime DLLs and the uninstaller. |
| The installed copy, started with no switches | Start screen drawn, "version 0.3.0" at the bottom right, "Continue the journey" offered from the existing save. |
| The installed copy with the test pilot (45 s) | In play on the Moonlit Causeway, "Follow the road to Telavi", HUD complete. |
| Settings and save after both runs and after uninstall | `PlayerPreferences.ini` and `journey.sav` in `%LOCALAPPDATA%\CaucasianEagle` byte-identical to before. |
| Silent uninstall | The install folder and the uninstall entry are gone; `%LOCALAPPDATA%\CaucasianEagle` is kept. |

- [not verified] The installer's wizard pages, the Start-menu entry and the desktop shortcut, by hand.
- [not verified] A PC or Windows account that never had the project or Visual Studio. The Visual C++ runtime is packed beside the executable for that case, but the case itself was not tried.
- [not verified] The zip, unpacked and run (it holds the same folder the installer packs).
- [not verified] **Difficulty for a person.** The only measure is the pilot that stands still (table above). A person who dodges and guards should do better than it; nobody has tried.
- [not verified] **The death screen's respawn button.** The respawn itself is tested (table above); the click on the engine's screen is not, because no input is sent to the desktop in testing.
- [not verified] **Save and load in a normal run.** Unattended runs never write the save. The load path was exercised only through the test start variable, which sets the same fields.
- [not verified] **F9 restart**, for the same reason.
- [not verified] **Frame rate** beyond "the pilot's log showed the 60 fps cap in a 1280 x 720 window on the development PC".
- [not verified] A clean clone, a second machine, an installed copy (unchanged from v0.1.0).

## 11.3 Manual test checklist (run before each release)

1. Start screen appears: the rendered picture behind, the game's own panel at the left titled CAUCASIAN EAGLE with five rows, the makers' line along the bottom ("A GAME BY GIORGI SAMKHARADZE", "Built in Doorways, a custom engine founded by Giorgi Samkharadze in February 2026"); W / S and the mouse move the selection; Guide and Credits open the guide; Settings opens the engine's settings and Back returns; window title reads "Caucasian Eagle"; the first row enters Moonlit Sanctuary; four prologue cards show.
1a. Growth: fell creatures and see the crystal count rise; press I, buy a level and an evolution, see the count fall and the numbers under All together change; evolve all three items once and see the NYXARA EVOLVES banner; break a chest and drink the potion with Z after taking damage.
1b. Cage: in a fight, walk to each wall; she stops without jumping; with the camera turned so the wall is between it and her, the blocks in the way sink.
2. WASD moves her in upright flight; releasing the keys lands her; Shift is faster and leaves a feather trail; Ctrl walks.
3. Space jumps; holding Space flies up.
4. LMB alternates left sword, right sword; RMB is the charged slash.
5. F, V and C each fire at once, including in the middle of a sword attack; each shows its cooldown in the skill bar.
6. After each skill, LMB and RMB give that skill's two combos while the gold COMBO prompt shows.
7. Q swaps the swords and the HUD sword labels swap; E raises the barrier and blocks orbs ("BLOCKED").
8. Creatures take damage, their bars flash, they die and do not return.
9. Taking hits lowers the health bar; health returns slowly outside fights.
10. Mouse look: the view can be raised to level and a little above, and lowered to steeply downward.
11. The game's settings window opens from the start screen and from her menu; every row changes with A / D or the mouse; a window change asks to be kept; a changed setting survives a restart (`%LOCALAPPDATA%\CaucasianEagle\PlayerPreferences.ini`).
12. Exit from the menu closes the process.

The journey (new in v0.2.0):

13. The south gate leads onto the causeway; the road panel shows the region and the distance.
14. She can leave the road anywhere except the causeway and the last climb, and walk the whole land; water and cliffs hold her; she walks round trees, boulders and city houses. Off the road the compass and the trail of lights point back to it. In a fight she cannot leave the clearing. Walked round an unwon arena, the compass says a fight waits behind her.
15. Trouble on the road: cursed ground hurts only while she stands in it; a ring shows before each stone and standing outside it is safe; a wisp hurts on touch; none of them acts during a fight.
15. The first arena shuts both gates, sends two waves, opens when they are beaten and heals her.
16. A roadside chest breaks under any attack and shows what it held; the gold counter rises.
17. **Die on purpose in the second arena**: the death screen appears, respawn returns her to the first arena, the second arena's fight starts again.
18. Quit after an arena and start again: she stands at that arena, with the same gold, feathers and memories, and opened chests stay open.
19. Hold F9 for two seconds: the journey starts again at the head of the road.
20. Each of the twelve regions shows its card and looks different from the one before.
21. The three bosses show a health bar and their line; the way opens when they fall.
22. On the terrace the Eagle leaves Amirani's side to fight; after he falls the ledge can be walked, both anchors break, the chains disappear and the ending cards show.

## 11.4 Planned

| Item | Milestone |
|---|---|
| Clean-machine install test (the installer exists since v0.3.0) | M2 |
| A headless smoke test: launch with the test pilot, required `[JOURNEY] COMPLETE` line present within a time limit, as one script | M2 |
| Unit tests for the kit's pure logic (action table, cancel rules, cooldowns) after it is separated from rendering | M4 |
| Performance budget check (frame time in the showcase) | M5 |

### Not yet tested: what she carries, and nature

The second page of Nyxara's window was seen in one capture with sample items (`CAUCASIAN_EAGLE_GROWTH=2`). Charm drops, feather pickups, runes from chests, the save line and the fourth part of the end were compiled and read, not played. Grass and trees were seen in captures in regions 1, 2 and 4 at 60 frames per second on the development machine; the clouds, the other regions, and slower machines were not checked.

### Not yet tested: creature tactics, slow healing, earth roads

The test pilot (which cannot die) won arenas in regions 2 and 3 against the new creature behaviour at 60 frames per second. No person has fought them; whether heavy blows, slams and dodges are fair is unknown. The slow healing and chests without healing were read in code, not measured in play. The rebuilt world (earth roads, rolling land) was seen in captures in regions 1, 2 and 3 only.

### Not yet tested: the I key by hand, the music by ear

That the engine inventory no longer opens on I was changed in the engine and read, not pressed in a normal launch (unattended runs press no keys). The rewritten music was checked for length, level and loop only.

### Not yet tested: her menu, the guide and the start picture by hand

Seen in captures of an unattended run (`CAUCASIAN_EAGLE_PAUSE=1`, `CAUCASIAN_EAGLE_GUIDE=1` to `6`, and the start screen with no switches): her menu with its eight rows, guide pages 1, 2, 3, 5 and 6, the start screen with the rendered picture and the Guide button. Page 4 (Health) was not captured. No key was pressed in any of them, so these are **not verified**: Esc opening and closing her menu, W / S and Enter on its rows, Settings and the way back from it, "To the start screen" and "Leave the game", the Guide button on the start screen opening the guide and Esc returning to the start screen, A / D turning the guide's pages, the road standing still behind her menu, and the dodge key (O, B on a controller) doing nothing.

The system requirements are estimates. The game has run on one computer (Windows 11, Ryzen 9 9950X3D, 64 GB, GeForce RTX 5080); the lowest figures were not tested on any weaker machine.

### Walking the land, attack timing, gates, refined models (2026-11-11)

| Check | How | Result |
|---|---|---|
| She stands and moves on the land beside the road | Test pilot with `CAUCASIAN_EAGLE_ROAM=45` from arena 5, debug log | At 40.3 m from the road's middle, grounded, her height equal to the land's height (-12.8 m). In the Drowned Marsh she was held at the water's edge, as intended |
| The game's own start screen | Run with no switches; capture | Panel with five rows at the left in the game's style, rendered picture behind, makers' line along the bottom. Keys, mouse, Settings and back: not tried (need a hand) |
| Credits page | `CAUCASIAN_EAGLE_GUIDE=7` with no DEMO; capture | "WHO MADE IT" over the start screen, page 7 of 7 |
| Way shown off the road | Pilot with `CAUCASIAN_EAGLE_TROUBLE=-2`; capture | Compass at the top with the mark and the distance, trail of gold lights on the grass toward the road |
| Trouble on the road | Pilot with `CAUCASIAN_EAGLE_TROUBLE=1`, `2`, `3`; captures | Cursed ground (violet ring on one half of the road), falling stones (ring on the ground, stone above), wisps (three lights across the road); the objective line names each |
| Falling stones hurt | The same with `CAUCASIAN_EAGLE_MORTAL=1`, standing still 12 s | Health 100 to 15 (at 15 harm a stone; it is 12 now). Cursed ground and wisps: harm not measured |
| Pilot regression after these changes | `DEMO=1 PILOT=1 SPEED=3`, 100 s, the real build | 32 troubles placed, arenas 0 to 2 won, no deaths, 60 fps, no FAIL line |
| Pilot over the whole land | `START=5 ROAM=45 SPEED=3`, 200 s | Arenas 5 to 7 won, no deaths; held once at the marsh's water, the pilot now turns back to the road when held |
| The new gate | Rendered alone in Blender from the exported world | Piers, arch of wedge stones, keystone, wall and merlons as designed |
| Refined models | `refine_assets.py` report and before / after pictures of all 15 | All welded into one piece (1 to 5 islands instead of thousands), outline unchanged |

Not verified, because nobody has played it by hand: walking the land with the keyboard, the slide along its edge, the easing back before a city, walking round trees and boulders, every timing change to her attacks (kept clicks, slash to Charged Slash, cutting a flourish, the barrier cutting an attack), and that her menu is still open after the settings are left.
