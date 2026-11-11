# 13. Technical debt and risks

[← Index](TECHNICAL_DOCUMENTATION.md)

Each item has an id, what it is, why it matters, and the milestone that addresses it ([milestones](../milestones/README.md)). Severity: **High** blocks a v1.0 release, **Medium** costs time or quality, **Low** is tidiness.

## 13.1 Debt

| Id | Item | Why it matters | Severity | Milestone |
|---|---|---|---|---|
| D1 | **The package ships engine demo assets.** `tools/stage.ps1` copies all of `engine/assets/Compiled` (about 490 MB, mostly textures of the engine's own scenes). Which files the engine actually needs at start-up is not mapped. | The download is roughly three times larger than needed. | Medium | M2 |
| D2 | **The game includes engine internals.** `NyxaraKit` and the game module include `WorldState.h`, `Renderer.h`, `WorldRenderer.h`, `CombatSystem.h` and asset loaders directly. Only `DoorwaysGame.h` is a designed interface. | Any engine refactor can break the game build. | Medium | M4 |
| D3 | **Game code sits in the `Doorways` namespace.** The kit and HUD were moved out of the engine unchanged. | Confusing ownership; name clashes are possible. | Low | M4 |
| D4 | **Tuning lives in code.** Skill timings, damage, cooldowns and enemy stats are literals in `NyxaraKit.cpp`. | Every balance change needs a programmer and a rebuild; chapters cannot vary them. | Medium | M4 |
| D5 | **Skill keys are hard-coded and not rebindable.** F, V, C, Q, E and the mouse buttons are read with `GetAsyncKeyState`. | Players cannot rebind; the engine's Controls screen does not show them. | Medium | M5 |
| D6 | **Enemies exist only inside the kit.** Shades are not engine actors: no navigation, no collision with the world, one behaviour. | The story needs varied enemies and bosses. | High | M3 |
| D7 | **Leap and glide heights are visual only.** Collision stays on the ground while she appears in the air. | She can appear to hover through low obstacles; aerial gameplay is not real. | Medium | M5 |
| D8 | **Menus and screens are the engine's defaults.** The main menu is titled "DOORWAYS"; it shows Website, Forum and Credits entries, a level/XP system and an inventory that the game does not use. | Not presentable as a product. | High | M2 (title), M5 (rest) |
| D9 | **No game save.** Only engine settings and the engine's character snapshot persist. | A story game needs chapter progress. | High | M3 |
| D10 | **No audio.** The engine audio service starts but the game plays no music or effects. | Core to feel. | High | M5 |
| D11 | **One 1,200-line kit file** mixes input, rules, enemies, effects and drawing. | Hard to test and extend. | Medium | M4 |
| D12 | **The showcase script uses the old key letters internally** (`'1'`, `'2'`, `'3'` for the skill steps). | Harmless, but misleading when reading the script. | Low | M4 |
| D13 | **Facing snaps** back to the travel direction when an action ends. | Visible pop. | Low | M5 |
| D14 | **The asset builder is not built by this repository.** Rebuilding art needs the builder from an engine checkout. | Extra setup step for art changes. | Low | M4 |

## 13.2 Risks

| Id | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Source art is not backed up.** About 2 GB under `art/nyxara` (Meshy downloads, Blender files) exists only on the author's PC. Losing it means paying Meshy credits again and redoing the assembly. | Medium | High | Copy `art/nyxara/meshy/out` and the `.blend` files to a second disk or cloud drive (see [art/README.md](../art/README.md)). |
| R2 | **Engine and game drift.** The game pins an engine commit; the engine keeps changing for its own milestones. | High | Medium | Bump the submodule deliberately, one commit per bump, rebuild and run the checklist ([03-workflows.md](03-workflows.md)). |
| R3 | **Unsigned executable.** Windows SmartScreen warns on unsigned installers. | Certain | Medium | Code signing in M6. |
| R4 | **Clean-machine failure.** The game has only run on the development PC. Missing Visual C++ runtime, a different GPU or missing DirectX components may stop it. | Medium | High | Clean-machine test in M2. |
| R5 | **Meshy licence terms for a commercial release** are not recorded. | Unknown | High | Record the plan and terms before selling the game (M6). |
| R6 | **Repository size.** Compiled hero textures are 163 MB of PNG; every re-export adds another copy to git history. | High | Medium | Re-export only when needed; consider compressed textures or Git LFS before many more characters arrive. |
| R7 | **Single author, single machine.** Build paths (`engine/Direct3D.props` names an SDK version and an absolute root) assume this PC. | Medium | Medium | Verified by the first clean clone in M2. |
