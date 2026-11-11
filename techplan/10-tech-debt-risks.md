# 13. Technical debt and risks

[← Index](TECHNICAL_DOCUMENTATION.md)

Each item has an id, what it is, why it matters, and the milestone that addresses it ([milestones](../milestones/README.md)). Severity: **High** blocks a v1.0 release, **Medium** costs time or quality, **Low** is tidiness.

## 13.1 Debt

| Id | Item | Why it matters | Severity | Milestone |
|---|---|---|---|---|
| D1 | **The package ships engine demo assets.** `tools/stage.ps1` copies all of `engine/assets/Compiled` (about 490 MB, mostly textures of the engine's own scenes). Which files the engine actually needs at start-up is not mapped. | The download is roughly three times larger than needed. | Medium | M2 |
| D2 | **The game includes engine internals.** `NyxaraKit` and the game module include `WorldState.h`, `Renderer.h`, `WorldRenderer.h`, `CombatSystem.h` and asset loaders directly. Only `DoorwaysGame.h` is a designed interface. | Any engine refactor can break the game build. | Medium | M4 |
| D3 | **Game code sits in the `Doorways` namespace.** The kit and HUD were moved out of the engine unchanged. | Confusing ownership; name clashes are possible. | Low | M4 |
| D4 | **Tuning and text live in code.** Skill timings in `NyxaraKit.cpp`; creature stats, rosters, wave budgets, story cards and memories in `Journey.cpp`. | Every balance or wording change needs a rebuild; no translation path. | Medium | M4 |
| D5 | **Skill keys are hard-coded and not rebindable.** F, V, C, Q, E and the mouse buttons are read with `GetAsyncKeyState`. | Players cannot rebind; the engine's Controls screen does not show them. | Medium | M5 |
| D6 | **Creatures are not engine actors.** They live in the kit's enemy slots and are moved by `Journey`: no navigation, no collision with the world other than staying on the road, no engine combat events. | Fine on a road; a limit for open areas, and engine features (loot, experience) do not see them. | Medium | M4 |
| D7 | **Leap and glide heights are visual only.** Collision stays on the ground while she appears in the air. | She can appear to hover through low obstacles; aerial gameplay is not real. | Medium | M5 |
| D8 | **Menus and screens are the engine's defaults.** The main menu is titled "DOORWAYS"; it shows Website, Forum and Credits entries, a level/XP system and an inventory that the game does not use. | Not presentable as a product. | High | M2 (title), M5 (rest) |
| D10 | **No audio.** The engine audio service starts but the game plays no music or effects. | Core to feel. | High | M5 |
| D11 | **Two large files.** `NyxaraKit.cpp` (about 1,300 lines) mixes input, rules, effects and drawing; `Journey.cpp` (about 1,450 lines) mixes the road, creature behaviour, bosses, chests, story text, saving, drawing, HUD and the test pilot. | Hard to test and extend. | Medium | M4 |
| D12 | **The showcase script uses the old key letters internally** (`'1'`, `'2'`, `'3'` for the skill steps). | Harmless, but misleading when reading the script. | Low | M4 |
| D13 | **Facing snaps** back to the travel direction when an action ends. | Visible pop. | Low | M5 |
| D14 | **The asset builder is not built by this repository.** Rebuilding art needs the builder from an engine checkout. | Extra setup step for art changes. | Low | M4 |
| D15 | **The game draws its own sky and moon** because the engine lights its sky panels like scenery. The engine's panels are still drawn, in black. | Wasted draw calls; two sky systems. | Low | Engine: an unlit sky option, then delete the dome trick |
| D16 | **The cave is a rectangular cut in a height field.** Its walls are flat and the mountain has no overhang. | The landmark looks artificial up close. | Medium | M3: sculpted cave mesh |
| D17 | **Amirani, the eagle, the dog and every creature are rigid models** moved by code (breathing, bob, lean, lunge). No skeletons, no wing flap, no walk cycles. | The biggest visual gap: the hero is fully animated and nothing else is. Most visible in the Eagle fight. | High | M5: rig and animate, starting with the Eagle and the bosses |
| D19 | **Moonlight direction does not match the moon's position.** The light is aimed to show the mountain face. | Shadows point slightly wrong for anyone who checks. | Low | Accept, or move the moon behind the viewer |
| D20 | **Difficulty and length are untested by a person.** Creature health and damage were set by calculation and by a test pilot that stands still in every fight. | The road may be too hard, too easy or too long in places. | High | First hand play; then adjust `MonsterHealthScale`, `MonsterDamageScale` and the wave rules |
| D21 | **Death uses the engine's flow.** The engine shows its own death screen and respawns the player at the scene's spawn point; the journey then moves her to the last arena won. | One visible jump through the sanctuary on respawn; untested (needs a click). | Medium | Engine: a respawn-point option in `DoorwaysGame.h` |
| D22 | **The kit's shade enemies are dead code** while the journey runs. | Confusing to read. | Low | M4: remove or keep as a practice mode |
| D23 | **Creatures and the player can overlap.** Creatures stop at striking distance but nothing pushes the player out of a large body. | Bosses can hide her. | Low | M5 |
| D24 | **World scenery is generated.** Terrain is a height field, architecture is boxes and prisms, textures are 512 px procedural. Props are four Meshy models repeated. | Reads as a prototype next to the hero. This is the gap to a commercial look. | High for release | M5: hand-made or kit-bashed sets per region, larger textures, vegetation |
| D25 | **Every world rebuild adds about 70 MB to git history** (compiled mesh and textures). | Repository growth (R6). | Medium | Commit world rebuilds sparingly; Git LFS before v1.0 |
| D26 | **Story cards queue and cannot be skipped or re-read.** | A card during a fight is easy to miss. | Low | M3: a journal screen |
| D27 | **Gold has no use.** | A reward with no purpose. | Low | M5: something to spend it on, or remove the counter |

## 13.2 Risks

| Id | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Source art is not backed up.** About 2.8 GB under `art/` (Meshy downloads for the hero, the mountain figures, the creatures and the props; Blender files) exists only on the author's PC. Losing it means paying Meshy credits again and redoing the assembly. | Medium | High | Copy the four `meshy/out` folders and the `.blend` files to a second disk or cloud drive (see [art/README.md](../art/README.md)). |
| R2 | **Engine and game drift.** The game pins an engine commit; the engine keeps changing for its own milestones. | High | Medium | Bump the submodule deliberately, one commit per bump, rebuild and run the checklist ([03-workflows.md](03-workflows.md)). |
| R3 | **Unsigned executable.** Windows SmartScreen warns on unsigned installers. | Certain | Medium | Code signing in M6. |
| R4 | **Clean-machine failure.** The game has only run on the development PC. Missing Visual C++ runtime, a different GPU or missing DirectX components may stop it. | Medium | High | Clean-machine test in M2. |
| R5 | **Meshy licence terms for a commercial release** are not recorded. | Unknown | High | Record the plan and terms before selling the game (M6). |
| R6 | **Repository size.** Compiled hero textures are 163 MB of PNG and the journey added about 125 MB of creature, prop and world assets; every re-export adds another copy to git history. | High | Medium | Re-export only when needed; consider compressed textures or Git LFS before many more characters arrive. |
| R7 | **Single author, single machine.** Build paths (`engine/Direct3D.props` names an SDK version and an absolute root) assume this PC. | Medium | Medium | Verified by the first clean clone in M2. |
| R8 | **Meshy credits are spent.** 19 credits remain; one more model costs 30. | Certain | Medium | Any new generated asset needs a top-up; prefer improving what exists in Blender. |
| R9 | **Engine limits found only by walking into them.** The engine's ±999 m play-area box stopped the road at 4.1 km until `SceneCollision` switched it off. Other engine assumptions (render distance, float precision far from the origin) may show up as the world grows. | Medium | Medium | Run the test pilot end to end after every world or engine change. |
