# Caucasian Eagle: road to v1.0

Follow the milestones in order, and tick the boxes as you go. Each file ends with a **✅ Done when** check.

| # | Milestone | Version | Who | Status |
|---|---|---|---|---|
| [M1](M1-engine-game-split.md) | Engine and game split, own repository, documentation | v0.1 | Claude | ✅ 2026-11-10 |
| [M2](M2-installable.md) | Installable build: installer, smaller package, game title and icon, clean-PC test | v0.2 | Claude + you (install Inno Setup, test on a second PC) | ⬜ next |
| [M3](M3-story-spine.md) | Story spine: the map with the chained mountain, the Open Cave prologue, dialogue, saving, real enemies | v0.3 | You (story decisions) + Claude | ⬜ |
| [M4](M4-data-and-structure.md) | Tuning in data files, kit split into testable parts, narrow engine interface | v0.4 | Claude | ⬜ |
| [M5](M5-content-and-feel.md) | The seven years: maps, enemies, the Eagle fight, audio, game menus, key rebinding | v0.5 to v0.9 | You + Claude | ⬜ |
| [M6](M6-release.md) | Release: signing, licences and credits, store page, final checks | v1.0 | You + Claude | ⬜ |

M2 can run alongside M3: one is packaging, the other is content.

**How versions work.** Each milestone ends with a tagged version. `GameVersion` in `game/src/GameConstants.h` is the single source; `docs/changelog.md` records what changed; `techplan/` is updated in the same commits.

**Where the engine fits.** Engine work happens in the `Doorways-D3D11` repository and arrives here by moving the `engine/` submodule ([techplan/03-workflows.md](../techplan/03-workflows.md)). A milestone step marked **(engine)** needs an engine change first.
