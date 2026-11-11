# M1 · Engine and game split (v0.1)

**Who:** Claude · **Status:** done 2026-11-10 · **Next:** [M2](M2-installable.md)

## Goal
Doorways stays a clean engine in its own repository. Caucasian Eagle is its own repository that builds one game executable on top of that engine, with its documentation in place.

## Steps

### 1. Engine baseline
- [x] Commit the full engine working tree before restructuring (`fc476ab` in Doorways-D3D11).

### 2. Engine: a general game interface
- [x] `DoorwaysGame.h`: session options, game callbacks, host objects.
- [x] `DoorwaysApplication`: shared entry point `RunDoorwaysApplication`.
- [x] Camera, movement, input map, HUD and world renderer read general options; no game names left in engine source.
- [x] Scene choice, window title and user-data folder are run-time game options.
- [x] `DoorwaysEngine.vcxproj` builds the engine as a static library; `Doorways.exe` is the stand-alone sandbox.
- [x] Sandbox and engine tests still build; sandbox reaches its main menu.

### 3. Game repository
- [x] `kalk1t/Caucasian-Eagle` with the engine as the `engine/` submodule.
- [x] Game code moved to `game/src` and converted to the interface (`CaucasianEagle::Game`).
- [x] Game assets (hero bundle, effect textures, Moonlit Sanctuary) moved to `game/assets`.
- [x] Art scripts under `art/`; heavy source files kept out of git.
- [x] `tools/build.ps1`, `tools/stage.ps1`, `tools/package.ps1`, installer script.
- [x] Game builds from its own solution and runs from `build/stage/CaucasianEagle` with its own window title and its own settings folder.

### 4. Documentation
- [x] `techplan/` (index plus 12 chapters), `milestones/`, `story/STORY.md`, `docs/` (manual, changelog, glossary), `art/README.md`.
- [x] Engine `techplan/` started in the engine repository.

## ✅ Done when
- [x] `tools\build.ps1 -Run` in a checkout of this repository starts Caucasian Eagle.
- [x] No engine source file names the game.
- [x] Both repositories are pushed.

## Not done in M1 (moved on)
- Playing through with real keyboard and mouse after the split: do this first in M2.
- The installer has not been compiled: M2.
