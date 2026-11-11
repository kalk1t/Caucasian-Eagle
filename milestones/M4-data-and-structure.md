# M4 · Data and structure (v0.4)

**Who:** Claude · **Needs:** nothing · **Next:** [M5](M5-content-and-feel.md)

## Goal
Adding a skill, an enemy or a chapter is a data change plus focused code, not an edit to one large file. The game depends on a small, named part of the engine.

## Steps

### 1. Tuning in data (debt D4)
- [ ] Move the action table, damage shapes, cooldowns and enemy stats from `NyxaraKit.cpp` into a data file under `game/assets/` loaded at start-up.
- [ ] `techplan/11-constants.md` points at the data file instead of code lines.

### 2. Split the kit (debt D11, D3, D12)
- [ ] Separate files: input, action state machine, enemies, effects, drawing.
- [ ] Move game code into the `CaucasianEagle` namespace.
- [ ] Rename the showcase script's internal key letters.

### 3. Tests (see [techplan/08-testing.md](../techplan/08-testing.md))
- [ ] A test project for the kit's pure logic: cancel rules, press buffer, cooldowns, left/right alternation, combo windows.

### 4. Narrow the engine interface (debt D2) **(engine)**
- [ ] List every engine header the game includes beyond `DoorwaysGame.h`.
- [ ] For each, either add a general function to the game interface or accept it and record it as part of the supported surface in `engine/techplan/06-game-interface.md`.

### 5. Art workflow from this repository (debt D14)
- [ ] Build `DoorwaysAssetBuilder` from the submodule in `tools/build.ps1 -Tools`, and add `tools/build-hero.ps1` and `tools/build-map.ps1` wrapping the commands in the workflows chapter.

## ✅ Done when
- [ ] Changing a skill's damage or cooldown needs no recompile.
- [ ] The kit's rules have automated tests that run in the build script.
- [ ] The engine headers the game depends on are a written list.
