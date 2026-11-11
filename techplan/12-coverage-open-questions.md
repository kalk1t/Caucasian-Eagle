# 14. Coverage and open questions

[← Index](TECHNICAL_DOCUMENTATION.md)

## 14.1 What this plan covers

| Area | Coverage | Basis |
|---|---|---|
| Game source (`game/src`) | All files read | Written and moved during the split |
| Build, stage and package scripts (`tools/`) | All files read | Written for v0.1.0; build and stage were run, package was not |
| Engine interface (`DoorwaysGame.h`, `DoorwaysApplication`, `AppGame.cpp`) | All read | Written during the split |
| Engine internals | Only the parts the game touches | The engine's own plan is in `engine/techplan/` |
| Hero art scripts (`art/nyxara`) | `assemble.py` read in full; `build_base.py`, `meshy.py`, `make_vfx_textures.py` known from use, not re-read for this plan | |
| Map art (`art/moonlit-sanctuary`) | Export script read; build scripts not read for this plan | |
| World, creature and prop scripts (`art/world`, `art/monsters`, `art/props`) | All read | Written for v0.2.0 |
| Story | Author's premise recorded; the rest is proposal | [story/STORY.md](../story/STORY.md) |

## 14.2 Open questions

Technical:

| # | Question | Why it matters | Where to look / who answers |
|---|---|---|---|
| Q1 | Which engine assets are required at start-up? | Package size (D1) | Trace file opens during start-up, or remove folders from the stage until it fails |
| Q2 | Does a release build restrict the engine's developer and validation modes for players? | A shipped game should not expose them | Engine access model (`engine/src/DOORWAYS/DoorwaysAccess.*`) |
| Q3 | What do the engine menu's "Website" and "Forum" entries open? | They must point at the game's pages or be removed | Engine UI host |
| Q4 | Does the game run on a PC without Visual Studio installed? | Runtime dependencies (R4) | Clean-machine test |
| Q5 | Does the asset builder build and run from inside the `engine/` submodule? | Art workflow from the game repository (D14) | Try it |
| Q6 | What is the frame time in the showcase on a mid-range GPU? | Minimum requirements for the store page | Measure |
| Q7 | Under which Meshy plan and licence were the hero assets generated? | Commercial release (R5) | Author's Meshy account |
| Q8 | Is the road winnable and fair for a person at the current health and damage scales? | The main unknown of v0.2.0 (D20) | Play it |
| Q9 | Does death, respawn and the return to the last arena work as written? | Could not be tested unattended (D21) | Die on purpose in the first region |
| Q10 | What is the frame rate at 1080p and above on the author's and on a mid-range GPU? | The pilot ran at the 60 fps cap in a 1280 x 720 window on the development PC; nothing else was measured | Measure |

Product (for the author):

| # | Question |
|---|---|
| P1 | The story questions in [story/STORY.md](../story/STORY.md) section 6, and whether the road as built (section 5) is the story the author wants |
| P2 | Is v1.0 a paid game, a free game or a demo, and where is it distributed (itch.io, Steam, own site)? This decides signing, store assets and legal pages. |
| P3 | Languages: English only, or Georgian as well (text, voice)? The engine's font atlas covers ASCII 32 to 126 only, so Georgian text needs engine work. |
| P4 | How long should v1.0 be? v0.2.0 is one road of about two hours. |
| P5 | Should the engine's level, experience and inventory systems be part of the game or hidden? |
| P6 | Publisher name and game icon for the installer. |

## 14.3 Keeping this plan true

- When a value in [11-constants.md](11-constants.md) changes in code, change the table in the same commit.
- When a debt item is paid, move it to the changelog and delete the row.
- When a question is answered, replace the `[unknown]` label where it appears and delete the row here.
- The scope line at the top of [TECHNICAL_DOCUMENTATION.md](TECHNICAL_DOCUMENTATION.md) names the version the plan was last checked against.
