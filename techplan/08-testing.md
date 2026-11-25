# 11. Testing

[← Index](TECHNICAL_DOCUMENTATION.md)

## 11.1 What exists

| Check | How | Automated | Covers |
|---|---|---|---|
| Build | `tools\build.ps1` | yes (one command) | Engine library and game compile and link with warnings-as-errors for the listed warning numbers |
| Character pipeline gate | `DoorwaysAssetBuilder character-pipeline ...` prints `[PASS]` and the rig signature | yes | Mesh, skeleton and clips are consistent; altered-bind and reordered-joint rejection |
| Sound | Run the unattended pilot with the game window in front; debug output contains one line `Doorways Audio: game sound loaded: <file>` per file the first time it plays, and no line `sound file missing or not PCM 16-bit WAV`. Seen on 2026-11-11 for music, wind, flight, swings, skills, hits, blows, creature sounds, crystals and a memory line. Nobody has listened to the result: levels, timing and quality are untested | manual |
| Start-up | Run the staged exe; debug output contains `[PASS] Caucasian Eagle: Nyxara (EagleV1) is the player and her combat kit started.` | manual | Assets found, hero loaded, kit started |
| Scripted showcase | `CaucasianEagle.exe --road` with `CAUCASIAN_EAGLE_DEMO=1` (since 0.5 the exe alone starts the Ascent), watch or capture frames | manual review | Skills, combos, barrier, swap, travel, HUD on screen |
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

## 11.2c Flight, sounds and health after v0.3.0 (2026-11-12)

| What | Result |
|---|---|
| Free flight, scripted (`CAUCASIAN_EAGLE_FLY=1` with the pilot; 2026-11-12, flight as a mode) | `[FLY]` log: R asked at 4 s, she rises at 2.0 m/s and stops at 6.45 m at about 7.2 s; Space held from 8 to 9 s changes nothing; from 10 s she is pushed down onto the ground (a test-only sink key) and stays in flight, grounded, for five seconds while the pilot fights; landing asked at 16 s, flight ends at 19 s (3 s). No warnings or errors. |
| The bow, scripted (`CAUCASIAN_EAGLE_BOW=2` with the pilot; 2026-11-12) | `[BOW]` log after 55 s: 241 arrows loosed without aiming, 148 hits; captures with `=1` show the over-shoulder camera, the mark, the SHOOT / AIM slots; with `=2` arrows in flight toward creatures. Not checked by hand: aiming with the mouse, the pose close up |
| Aiming, scripted (`CAUCASIAN_EAGLE_BOW=1`: she lets the bow down every 2 s and aims again; 2026-11-12) | `[BOW]` log: the view's pitch goes from −0.50 to −0.10 with no creature near and to about −0.3 on a close one, and back to −0.5 between; 72 hits of 241 with the pilot walking her about (14 of 130 before the view turned to targets). Captures: level view, the mark, hit ticks, the string of light. Not checked by hand: the feel with a mouse |
| Roofs (`CAUCASIAN_EAGLE_ROOF=4` with the pilot, 2026-11-12) | `[ROOF]` log: dropped from 4 m over the middle of a Telavi house, she comes to rest at 6.01 m (the roof; the land there is -4.33 m), grounded, and stays. One captured frame: she stands on the ridge of the tiled roof. Not tested: walking on a roof, landing on one with R, towers, gates and the other six cities. |
| Free flight, one captured frame | High over the road, wings wide, shadow far below. |
| No moon (2026-11-12) | One frame looking up at the sky from the road (`CAUCASIAN_EAGLE_VISTA=1`): stars and cloud, no moon, no halo. The engine's own scenes were not run. |
| Soaring pose, wings 10 cm lower | Rendered in Blender from the side, behind and above: body level, wings straight out, arms and swords swept back. Not caught in a game frame (the captures fell on fights, where she is upright by design). |
| New fighting sounds | Files written; levels measured (hit peak 0.92, kill 0.98, rage 0.90, against 0.45 for a footstep). **Not listened to.** |
| No healing by itself | Code removed; not measured in a run. |

- [not verified] Steering in free flight by hand, landing where wanted, flying over arenas and cities, the blend between the level and the upright pose.

- [not verified] The installer's wizard pages, the Start-menu entry and the desktop shortcut, by hand.
- [not verified] A PC or Windows account that never had the project or Visual Studio. The Visual C++ runtime is packed beside the executable for that case, but the case itself was not tried.
- [not verified] The zip, unpacked and run (it holds the same folder the installer packs).
- [not verified] **Difficulty for a person.** The only measure is the pilot that stands still (table above). A person who dodges and guards should do better than it; nobody has tried.
- [not verified] **The death screen's respawn button.** The respawn itself is tested (table above); the click on the engine's screen is not, because no input is sent to the desktop in testing.
- [not verified] **Save and load in a normal run.** Unattended runs never write the save. The load path was exercised only through the test start variable, which sets the same fields.
- [not verified] **F9 restart**, for the same reason.
- [not verified] **Frame rate** beyond "the pilot's log showed the 60 fps cap in a 1280 x 720 window on the development PC".
- [not verified] A clean clone, a second machine, an installed copy (unchanged from v0.1.0).

## 11.2g What was verified for 0.5.3 (2026-11-25, released)

| Check | Result |
|---|---|
| The quests' test, `CAUCASIAN_EAGLE_DEMO=11` | Every step ok: Q opens the first three; bought by Enter, a click and S then Enter; too few crystals refused; counted only once taken; a nest above the one asked counts; all three done: THE HIGHLANDS at once and saved; read back; the next six offered; a quest after the save rolled back; `QUESTS_PASS` |
| The pointer test, `DEMO=7` | `POINTER_PASS` (her window with her gears, CLOSE, Esc, her menu) |
| The crystals' ledger, `DEMO=8` | `LEDGER_PASS` |
| The jars' test, `DEMO=4` | `WAYS_PASS` |
| Her showcase, `DEMO=9` | `SHOWCASE_PASS` |
| The map, `DEMO=10` | `MAP_PASS` |
| The nature test, `DEMO=5` | `NATURE_PASS` |
| The climb, `DEMO=2`, on an empty save (`CAUCASIAN_EAGLE_ASCENT_SAVE`) | `CLIMB_PASS` shore to summit in 1,458 s; the game's rock rules held her 0 times, the engine's steep walls 28 moves of 407,370 faces. (Run on the developer's own save, at nest 9, it starts at the top and never passes the shore's station: `climb_stuck`. The climb reads the player's save; give it an empty one.) |
| The creatures' tour, `DEMO=3` | `CREATURES_FAIL bad=3`: only way-4500's Kaji (off its ground, never roused, never came), as since 0.5.2; the Eagle's row: woke, came, beaten, 25 ice crystals |
| Off the way, `DEMO=14`; Character Lab check | 2026-11-23 on the final map: 78 runs, 0 FELL_THROUGH; `CHECK_PASS` (not run again) |
| What it uses (the climb, RTX 5080) | about 1.3 GB working set, 1.0 GB private; about 2.0 GB dedicated graphics memory (`GPU Process Memory` counter, one adapter); installed 1.46 GB |
| The release gate | `Test-Release.ps1 -Version 0.5.3 -AllowUnsigned -Scan`: versions, hashes, sizes (zip 849 MiB, installer 750 MiB), shipped programs, Defender clean; unsigned, as decided |
| **Not tested** | The installer installed and run; a weaker computer. (The owner checked the game by hand before release, 2026-11-25: "its pretty much all checked") |

## 11.2f What was verified for 0.5.2's nine levels at the nests (2026-11-21, not released)

| Check | Result |
|---|---|
| The crystals' ledger, `CAUCASIAN_EAGLE_DEMO=8` (`ledger.tsv`) | Every stretch holds at least three times its level's price (25 for 24, then just 42, 63, 81, 99, 117, 138, 156, 174 crystals), 64 to 65 % of it on the way; the jars hold the map's crystals to the last (175 beside the way, 132 off it), none past the last nest, none left unlaid; 17 of 17 groups around the way placed, none closer than 140 m to the one before (148 m the least), six gaps over 400 m, each 26 m or more across the ground from any other group, the nearest from where its path leaves the way (by 8 m the least), no group further up the way within 26 m of there, and 100 m or more from a cave's group when the two are more than three (115 m the least); `LEDGER_PASS` |
| The pointer test, `CAUCASIAN_EAGLE_DEMO=7` | Away from a nest LEVEL UP buys nothing; in the first nest each card's LEVEL UP buys one level (8 crystals), worn and saved (`rules 2`, `level`, `look`); mark 0 puts her own look on, a mark not reached is not there; then CLOSE, the menu and Esc as before; `POINTER_PASS`, with the looks loaded from her game bundle (a picture of the window: golden eagle wings, shashkas of Svaneti, Svan mail) |
| The jars' test, `CAUCASIAN_EAGLE_DEMO=4` | 3,710 jars in 936 groups; 307 crystals in them; a save, a rollback and the save read back in the new form; `WAYS_PASS` |
| The Ascent smoke (`DEMO=1`) | `ASCENT_PASS` (a player's old save is read, migrated in memory and never written by an unattended run) |
| The creatures' tour, `CAUCASIAN_EAGLE_DEMO=3` | Run four times as the rules for the groups around the way were found (the first `bad=8`: groups 10 to 26 m apart one fight; the second `bad=3`: a Devi above seeing her where a path leaves the way, and cave-14 waking and sleeping every frame; the third `bad=5`: around-4374 holding the three places beside cave-14, and way-4500's three). The last: all 95 stops; every group around the way woke on its ground, came at her and was beaten, and so did cave-14; 158 beaten and saved, none awake again after the reload; four groups gave way to a nearer one, each once (six creatures), and all were beaten later; `CREATURES_FAIL bad=3`, all three way-4500's (off its ground by 3.1 m, never roused, never came: the Kaji 0.5.0's tour found placed off its ground and never waking; known, not fixed) |
| Character Lab `-NyxaraCheck` (on the new bundle) | `CHECK_PASS`; the pauldrons filmed at armour tier 3 of each example, standing, running and cutting, from her left and her right |
| Map Lab | `tools/map-lab.ps1 -Build -Smoke`: built with the new map code (`PlaceAround`); its smoke PASS (ground at every station at its height, a walk up the trail, a take-off) |
| The placing model | Run again: the same `EaglesAscent.creatures.txt` byte for byte |
| Her bundle | Imported again with the looks: her 31 clips, her skeleton and her own textures byte for byte the same; 46 MB to 103 MB |
| **Not tested** | A climb from the shore to a nest by hand, buying with crystals found on the way; the keys A and D in the window (the pointer test clicks the marks); the window and the looks seen by a person on another screen size |

## 11.2e What was verified for v0.5.1 (2026-11-20)

The Windows trust audit, with its evidence: [`docs/release-audits/v0.5-to-v0.5.1/REPORT.md`](../docs/release-audits/v0.5-to-v0.5.1/REPORT.md).

| Check | Result |
|---|---|
| The nature test with her menu, `CAUCASIAN_EAGLE_DEMO=5` | Trees: 24 tries, 23 reached, 0 through. Then Esc's menu (8 rows) and its settings window open: `NATURE_PASS`. Pictured with `tools/release/Save-GameWindow.ps1` (the game's window only): the notes of three rows ran into their names and were shortened |
| The package (`dist/CaucasianEagle-0.5.1-win64`) | Ascent smoke `ASCENT_PASS`, jars `WAYS_PASS`, `--road` opens the road's window |
| The release gate, `tools/release/Test-Release.ps1` | With `-AllowUnsigned -Scan`: passed (versions 0.5.1, checksums, under 2 GiB: zip 2,032 MiB, no other program, Microsoft's DLLs signed, Defender: no threat); without it: fails the three unsigned files |
| Install, upgrade, uninstall, as the ordinary user | Public 0.5.0 installed (no administrator mode), first launch `ASCENT_PASS`; 0.5.1 over it: one uninstall entry, now 0.5.1, saves and settings unchanged (hashes); the installed 0.5.1 from its Start-menu shortcut `ASCENT_PASS`; uninstalled: the system equal to before the first install (startup entries, tasks, services, firewall rules, shortcuts, folder) |
| Opening a downloaded installer | A copy marked as from the internet opened through the shell: the wizard opened, no SmartScreen window on this PC (0.5.0 and 0.5.1). Not a browser download |
| **Not tested** | Signing with a real identity; Smart App Control on; a browser download; a second PC |

## 11.2d What was verified for v0.5.0 (2026-11-20)

| Check | Result |
|---|---|
| The jars' test, `CAUCASIAN_EAGLE_DEMO=4` (`ways.tsv`) | 3,710 jars in 936 groups (way 433, caves 843, rings 2,434), 73 places. Her own cuts (the kit's test-pilot keys) broke jars at 8 groups of the way and rings and 2 in caves. Saved at the first nest, 12 more jars broken and a place found (14 crystals gained), then `RollBack`: crystals, potions, broken jars and found places exactly as saved; the save read back the same. `WAYS_PASS` |
| The caves' walls, Map Lab `-CaveWalls` | Before the fix 42 failures in the first three caves (past the wall, one out of the tube). With `EaglesAscent::HoldInCave`: 558 tries in 89 caves (every 30 m, a run at the right wall, a run and two jumps at the left), 0 failures; `CAVES_PASS`; then `-Smoke` passed |
| The finishers | Character Lab: the four on each of 12 creatures, 48 of 48. Mountain tour with finishers (after the cave fix): 123 finishing kills (Reaper's Wheel 28, Mountain Breaker 28, Tempest Flurry 29, Headsman 38), bosses too, none without room |
| The creatures' tour, `CAUCASIAN_EAGLE_DEMO=3` | `CREATURES_FAIL bad=4`, the same before and after the cave fix: two groups placed off their ground (way-0800; way-4500, a Kaji that never wakes) and two stops out of reach. Known, not fixed in 0.5.0 |
| The Ascent smoke (`DEMO=1`) | `ASCENT_PASS` |
| Map Lab `-Smoke`, Character Lab `-NyxaraCheck` | `SMOKE_PASS`, `CHECK_PASS` |
| The nature test, `CAUCASIAN_EAGLE_DEMO=5` (`nature.tsv`, pictures) | Grass at eight spots from 59 m to 3,044 m: 4,748 to 22,579 tufts drawn, the frame at the 60-a-second cap (16.7 ms). 24 trunks beside the way, run at for 2.4 s: 23 reached, none entered (held at the bark, most then sliding round it), `NATURE_PASS`. Baseline with `CAUCASIAN_EAGLE_TREES=0`: 13 of 24 run through |
| Start-up | `CaucasianEagle.exe` with no switch: the Ascent's smoke run, `ASCENT_PASS`; the jars' test `WAYS_PASS` on the final build; `--road`: the road's window ("Caucasian Eagle") |
| **Not tested** | The I window, the crystal and potion panel and the H prompt seen on screen by a person; a whole climb by hand; the installer installed and run |

## 11.3 Manual test checklist (run before each release)

1. Start screen appears: the rendered picture behind, the game's own panel at the left titled CAUCASIAN EAGLE with five rows, the makers' line along the bottom ("A GAME BY GIORGI SAMKHARADZE", "Built in Doorways, a custom engine founded by Giorgi Samkharadze in February 2026"); W / S and the mouse move the selection; Guide and Credits open the guide; Settings opens the engine's settings and Back returns; window title reads "Caucasian Eagle"; the first row enters Moonlit Sanctuary; four prologue cards show.
1a. Growth: fell creatures and see the crystal count rise; press I, buy a level and an evolution, see the count fall and the numbers under All together change; evolve all three items once and see the NYXARA EVOLVES banner; break a chest and drink the potion with Z after taking damage.
1b. The Ascent's quests and gears (0.5.3): press Q anywhere and see the first three quests and their price in wind crystals; TAKE one with too few crystals and see it refused; take one, beat creatures or break jars and see its count rise in the HUD box (*Q: n of m done*); finish all three and see THE HIGHLANDS put on her at once, *Saved*, her window (I) showing the new gear and its numbers, and six fire quests offered; fall and wake at the saved nest wearing it.
1b. Cage: in a fight, walk to each wall; she stops without jumping; with the camera turned so the wall is between it and her, the blocks in the way sink.
2. On foot WASD moves her at a jog, back low, wings half open; Shift runs and Ctrl stalks; standing she looks about and shakes her wings.
3. Space jumps and never flies. R on the ground: she rises for 3 s and flies; W goes where the camera looks; Space held climbs straight up (to the ceiling), X held sinks. R in the air, low: she lands in 3 s and walks; high: "too high to land". Fighting or touching the ground in flight does not end it.
4. LMB alternates right axe, left axe (as the screen shows them); RMB is the charged slash.
5. F, V and C each fire at once, including in the middle of a sword attack; each shows its cooldown in the skill bar.
6. After each skill, LMB and RMB give that skill's two combos while the gold COMBO prompt shows.
7. G swaps the axes and the HUD labels swap; E raises the barrier and blocks orbs ("BLOCKED").
   Q takes the bow out: bow in her left hand, axes crossed on her back, HUD slots SHOOT and AIM. Right button held: the camera comes in over her shoulder, a mark at the middle of the screen. Left button held: arrows, five a second; creatures on the mark are hit. Q again: axes in her hands, bow on her back. F, V or C with the bow out takes the axes out.
8. Creatures take damage, their bars flash, they die and do not return.
9. Taking hits lowers the health bar; health does not return by itself.
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
| Places | Pilot with `CAUCASIAN_EAGLE_AT` near a site; `CAUCASIAN_EAGLE_JOURNAL=14`; captures | The arch stands over the road, the name shows on arrival, the journal's PLACES page lists 26 of 26 |
| Traps, each kind | Pilot with `CAUCASIAN_EAGLE_TRAP=1..9` and `-1..-9`; captures and log | Every trap seen; gate run begun, lost and won; watchers, rune stones and wind teeth won by passing; a seal broken by three stones. Log: `traps: 9; small troubles left: 30` |
| Creatures against flight | Pilot held in the air (`CAUCASIAN_EAGLE_FLY=2`) beside a pack | Flyers rise, small creatures leap, giants throw; no error |
| Pilot from 1500 m, speed 12, 240 s | `AT=1500 GO=1 SPEED=12` | Six traps passed to 4090 m, 60 fps; `under the land` logged twice near 4080 m and she went on (before the lift she fell without end in a whole-road run) |
| Whole road in three runs, speed 12 (from the start; `START=13`; `START=15`) | `DEMO=1 PILOT=1 SPEED=12`, 590 s each | Arenas 0 to 20 won, the Gveleshapi among them, 0 deaths, 60 fps, no FAIL; `under the land` 9 times in all, each time she went on. |
| The end of the road | `START=20 SPEED=12`, 585 s | Arenas 20 to 24 won (the Rokapi and the Devi lord among them), the Eagle, the chain: `[JOURNEY] COMPLETE`, 0 deaths |
| The lift switched off (temporary switch, removed) | `START=21 SPEED=12`, 200 s | She passed 6 m under the land five times, falling 14 to 18 m/s: true falls. With the lift (0.8 m): 13 lifts from arena 20 to 24, no fall |
| F8, death, waking | `SAVE=<file> SAVEKEY=8 MORTAL=1 TRAP=-6` (in the press), 60 s | `saved by key at 3588 m`; three deaths, each followed 3.5 s later by `respawned: back at 3588 m` (a new journey without the point would wake at 0 m); the file's last line is the point |
| A session from a save | The same file, no other switch | She stands at 3588 m in the first pilot line |
| A format 1 save | A copy of a real one (8 arena rows won), `SAVE=<copy> SAVEKEY=9` | `cleared 6`; she stands at 1422 m, the last fight won; rewritten as format 2 with a point |
| **Not tested** | | The banner and the absence of the engine's panel on screen (captures were not possible while the author's own game was running); the ceiling in flight (read in code only); F8 pressed by a person |
| The lift at speed 3 | `START=15 SPEED=3`, 230 s | `under the land` once at 3874 m (not grounded, falling 15 m/s, on foot): a true fall, not an effect of the speed |
| **Not tested** | | The press's link broken in play; a format 1 save converted; the trap line of the save read back; the traps played by a person |
| Pilot over the whole land | `START=5 ROAM=45 SPEED=3`, 200 s | Arenas 5 to 7 won, no deaths; held once at the marsh's water, the pilot now turns back to the road when held |
| The new gate | Rendered alone in Blender from the exported world | Piers, arch of wedge stones, keystone, wall and merlons as designed |
| Refined models | `refine_assets.py` report and before / after pictures of all 15 | All welded into one piece (1 to 5 islands instead of thousands), outline unchanged |

Not verified, because nobody has played it by hand: walking the land with the keyboard, the slide along its edge, the easing back before a city, walking round trees and boulders, every timing change to her attacks (kept clicks, slash to Charged Slash, cutting a flourish, the barrier cutting an attack), and that her menu is still open after the settings are left.

## The new body, 2026-11-13

| Test | How | Result |
|---|---|---|
| Load | Debug output | `her two axes and her bow are loaded as attachments`; `Nyxara (RodinV1) is the player` |
| Axes in hand, exchanged | `VIEW`, `LOOK`, `ARMS=0` and `3`, pictures | Each axe in a hand, upright; G exchanges them |
| Bow out | `ARMS=1`, pictures from behind and the side | Bow in her forward hand, both axes on her back |
| Bow shot | `ARMS=5`, pictures from three sides | Bow upright in her extended hand, string bent from both tips to her other hand, arrow of light; the bow is in her shadow |
| The body before | `HERO=EagleV1`, picture | The old body with its weapons, as before |
| Play | Pilot, speed 6, mortal, 150 s | Two arenas won, three deaths each followed by waking, 60 fps |
| Asset builder | Built with the two patches into a scratch folder | Compiles and links; not run |
| **Not tested** | | Played by a person; aiming with the right mouse button seen from outside (the aiming camera could not be turned in the test); flight with weapons seen up close; every combo and skill looked at one by one; foot planting; the engine's inventory view (this game does not use it) |

| The detailed body with weapons apart (EagleV2) | `VIEW`, `LOOK`, `ARMS=0`, `1`, `5`, pictures | Her old face, helm and armour; axes in hand; her own bow on her back, in her hand, drawn |
| The bow follows the shot | `ARMS=5 BOWPITCH=35`, `0`, `-35`, side pictures | Her upper body and the bow pitch up, level, down |
| **Not tested** | | The aim against a real creature above or below her in play; the aim while aiming with the right button, seen from the side |

## Closed hands, finishing blow, effects, 2026-11-13

| Test | How | Result |
|---|---|---|
| Closed hands | Blender render of the body at rest with both axes placed by their hand sockets, open hands beside closed | Open: fingers hang straight past the grip. Closed: fingers wrap the grip, both hands |
| Closed hands in the game | `VIEW=2.2`, pictures | Loads, plays, no torn wrist seen at that distance; too far to judge fingers |
| Finishing blow | Pilot, speed 4, mortal, 110 s, log | 5 `[FINISH] start`, 5 `[FINISH] kill`; two arenas won; 60 fps |
| Dash, cyclone, leap | `ARMS=6`, `7`, `8`, 16 pictures each, 0.25 s apart | Dash: no blow after it, no lance. Cyclone: pale arcs and wind lines, no green light. Leap: circle, dust, no pillar |
| **Not tested** | | The finishing blow seen in pictures (the pilot's fights were not photographed); on a boss; the look of the Finisher clip's cross-fade; the bow in the closed hand in a close render; arrows' new look; the third slash seen; played by a person |

## Map Lab, 2026-11-13

| Test | How | Result |
|---|---|---|
| Ascent builds and compiles | `build_mountain.py` (then `build_ascent.py`), `scene-pipeline` | Trail 10 198 m, steepest 0.450; 951 objects, 427 090 faces; scene compiled, PASS |
| Smoke | `Test-Map.cmd -Smoke` | `SMOKE_PASS`: ground at all 13 stations at their heights (0 to 4200 m), 32 m run up the trail, take-off to 30 m; 16.7 ms a frame |
| Pictures | Stations 0, 6, 12 by day | Shore with the trail and Terrace 1 in view; terrace 6 with its posts and labels; the ledge at 4200 m. Screen text and the height ruler read correctly |
| **Not tested** | | Played by a person; the whole trail walked end to end; flight to the summit; dawn, night and the fog steps seen; the speed steps on foot (x8 on the 24 degree trail may be stopped by the engine's 0.45 m step limit); the seam between near and far land seen from the air |

## Map A in the Map Lab, 2026-11-13

| Test | How | Result |
|---|---|---|
| Map A builds | `Test-Map.cmd -PrepareAssets` | Path 8164 m in 32 legs, mean grade 0.51, steepest 0.76; 348 tiles, 20 landmark pieces; 1105 objects, 1.69 million faces, 60 FPS; scene compiled |
| Smoke | `Test-Map.cmd -Smoke` (first build of Map A, grade 0.72) | `SMOKE_PASS`, 11 stations, 60 FPS |
| Walk, first builds | `-Walk` | `WALK_FAIL` three times at the first hairpin, 30 m before the terrace (y 179.53 of 180): the upper leg's terrace lay 0.47 m over the lower leg. Also `WALK_FAIL` at x3 speed where the climb begins (height lag). Both fixed, see `techplan/04-components.md` |
| Walk, whole trail, first build (even zigzag, no models) | `Test-Map.cmd -Walk -Speed 0` | `WALK_PASS`: 7520 m walked, 4200 m climbed, 23 min 16 s |
| Walk, the winding path with pieces | `Test-Map.cmd -Walk -Speed 0` | `WALK_PASS` (`-Walk -Fast 30`): 7941 m walked, 4200 m climbed, 24 min 35 s of game time, about a minute of real time |
| Walk fails found on the winding path | | At 96 m: land between two close legs stood through the ribbon (fixed: land under the ribbon is held to its own leg). At 448 m: a stone tile 0.2 m over the ribbon on a 0.74 pitch (fixed: `STONE_GRADE`). At 1360 m: the land's collision beside a leg was drawn up 0.7 m by the next leg (fixed: land takes its height from its own leg only). Several falls were the tester's own keyboard: unattended runs read it (fixed in the engine) |
| Start screen | Picture of the staged game at 1920 x 1080 | The painting covers the screen; the menu is at the right; shrine and summit in view |
| Pictures of Map A | Stations 0, 2, 5, 10 (grade 0.72 build) | Tide Gate arch and the first flight line's labels; terraces with posts; the summit court with its gate posts and rock |
| **Not tested** | | The flight lines flown; updraft and crosswind felt; Backspace and the rescue; the gorges and bridges seen and crossed by a person; dawn, night, fog; Ascent rebuilt with the hairpin fix was not walked |

**2026-11-13, Map A route version 3.** `tools/map-lab.ps1 -Build -PrepareAssets -Walk -Fast 30`: `WALK_PASS` (7.9 km on foot, carried over 21 flight stretches). Looked at in pictures taken with `-Along`: a rampart, the Eagle's Leap, a cleft, the shoulders. Not tested: flying the cliffs, breaks, clefts and ramparts by hand; `-Smoke` on this version; the campaign's own tests with the three engine changes (steps per picture, no keyboard in unattended runs, step tolerance).

**2026-11-14.** `-Walk -Fast 30`: `WALK_PASS` on the 10-to-30-degree route (10.0 km). `CAUCASIAN_EAGLE_MAPLAB_FLYTEST=1` with `_ALONG=1040`: flying straight at a cliff she stops at its foot. Not tested by hand; models do not block flight.

**2026-11-18, while the owner was away.** New: `-Pilot` (every wing-only stretch flown with the player's keys), `-PilotProbe`, `-CamLog` + `survey_camera.py`, and the game's whole climb (`CAUCASIAN_EAGLE_DEMO=2`). On the final build: lab smoke PASS, crossing 30× PASS 20/20, walk 30× PASS, miss 30× PASS 32/32 (8 rescues), pilot PASS 20/20 at normal speed and with the probe; game ASCENT_PASS and CLIMB_PASS (24 min 22 s, running every bend, no fall). Not measured: frame times (the review shots ran locked to 60 Hz). Not tested by a person. Report: `art/map/MapA_MapLab_Package/away/AWAY_REPORT.md`.

**2026-11-19, Nyxara's feather storm (C); the finishing throw taken out.** Map Lab films of the cue files `storm`, `stormabove`, `stormbehind` and `stormfar` (`testing/MapLab/anim-cues/`; the cue word `ring` puts three creatures 6 m about her, `farring` 22 m; the camera `above` looks down from aside): she crouches, leaves the ground at 0.25 s of the clip, turns 1089 degrees in all (three turns; at the fastest 1282 degrees a second, the pace changing by at most 268 degrees a second from one frame to the next), her lowest foot 1.95 m over the ground at the top, and lands at 1.36 s; her place does not move. Her feather daggers fly out straight all round and stick in the ground 6 to 27 m off (seen in pictures, small at that distance). At each of the three half-turns three fly at each creature, and all nine blows land, at 6 m and at 22 m (`feathers struck` in the kit's log; at 22 m 0.6 s after the throw). Out of standing at ease she draws her swords as she crouches. `skills` films F (the slide) out of standing and out of her guard. Not tested: the storm in the game itself (the camera follows 40 % of her lift), on stairs or a slope (a dagger sticks where the ground under it is found, else at the height of her feet), at creatures that fly; by a person.

**2026-11-19, her cuts as one move; the cast's twist.** Map Lab films `cutsquarter` (left, right, three left, four right, then left and right in turn) and `fightrun`; for each cut the kit's log gives the lead it chose (`cut lead`), and the cutting hand's speed against her hips was measured frame by frame from the entry to the blow (the way in, the least speed after it, the swing). Before (the same cues filmed with the clips as made and the lead by a straight-line guess) and after: L1 out of her guard 9.2 m/s in, down to 2.2, swing 10.7, now 5.2 in, down to 1.1 (lead 0.42 s, was 0.26); again later 9.1, now 5.2; R1 5.3, 5.4 and 4.7, now 3.1, 3.2 and 3.8 against 10.4; L3 6.1 (down to 5.5), now 3.2 (3.0) against 14.7; L2 3.3 and 4.6, now 2.7 and 4.5 against 6.7; R2 9.8, now 5.1 against 13.8; R4 after L2 11.0, now 7.3, and after R1 5.7, now 6.2 (a little more), against 12.4; R3 6.8 against 16.3, as it was; the first cut, which draws her swords from her back (her arm up to the handle at 8.2 m/s, then the cut down from it), as it was. The re-timed clips (`EvenWindUps`, logged at load as `cut wind-ups made one move`): R2, R4, L3 and the step and run cuts (which no row plays); left as they were L1, L2, R1 (calm), R3 (its wind-up 6.8 m/s against a blow of 15.5). Every frame of a re-timed clip is one of the clip's poses (the clip dump `CAUCASIAN_EAGLE_NYXARA_CLIPDUMP` shows them). `cast`: the twist toward her swords gathers speed (her left hand 4.4, 9.8, 15.0, 17.9, 18.9 m/s in its first frames; it was 4.4, 27.4). The game builds without a warning; the Map Lab's tests pass. Not tested: a person playing the cuts for their feel (the first cut out of her guard now lands up to 0.16 s later after the click).
