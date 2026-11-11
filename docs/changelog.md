# Changelog

[← Project index](../README.md)

Newest first. Each version names the engine commit it was built against.

## 0.2.0 · 2026-11-10 · The road to Amirani

Engine: Doorways-D3D11 `d9028b6` (scene collision for games, play-area box off, player healing, environment hook).

Not tagged yet: this version was tested by an unattended test pilot, not played by hand. See "Known gaps".

**The journey**
- **One road from Moonlit Sanctuary to the mountain**, 6.3 km through twelve regions: the Moonlit Causeway, the Whispering Wood, the Drowned Marsh, Ochokochi's Wood, the Stone Towers, the Snow Pass, Kajeti, the Giants' Valley, the Dragon Lake, Devi's Gorge, the Witches' Hill and the Chain Road. Each has its own ground, fog, light and landmarks.
- **Ten creatures of Caucasian folklore** to fight: Chinka, Ali, Kudiani, Ochokochi, Almasty, Kaji, Uaig, Devi, and the bosses Rokapi and the Gveleshapi, plus the Devi Lord. Melee blows show a warning ring first; spirits and witches keep away and throw orbs; goat-men leap.
- **34 arenas** where the way shuts until every wave is beaten, and **65 roaming packs** between them.
- **81 old gold chests** to break: coins, twelve memories of Amirani, Moon Tears (health), Eagle Feathers (permanent damage), Soul Embers (a burst of power).
- **The ending**: the fight with the Eagle on the terrace under the cave, then the climb to the ledge to break the two chain anchors.
- The story is told in short text cards: a prologue, a card for each region, boss lines, the ending.
- **Saving**: progress is saved after every arena and chest; a new start continues from the last arena won. Hold F9 to begin again.
- Health returns slowly outside fights; every creature felled, every wave beaten and every arena won gives health back.
- After a hit she cannot be hurt again for one second, and only two creatures strike at the same time.

**The world**
- The mountain of Amirani now stands at the end of the road, with a new night sky, moon and distant ridges. Its ledge can be walked on. The regions' fog does not cover it, so it is in view from the whole road.
- The white disc in the middle of the sanctuary is gone (it was a leftover "arena seal" object).
- The player stays on the road: the scene's own collision is used.

**Interface**
- Road panel (region, distance, gold, feathers, memories), objective line, boss health bar, story cards. What a chest held shows in a small panel at the right edge, so it does not cover the fight.
- Camera a little further back and flatter, so the road ahead and the mountain are in view.

**For development**
- `CAUCASIAN_EAGLE_PILOT=1` (with `CAUCASIAN_EAGLE_DEMO=1`): the game plays the whole road by itself. `CAUCASIAN_EAGLE_SPEED` and `CAUCASIAN_EAGLE_START` shorten test runs; `CAUCASIAN_EAGLE_MORTAL=1` lets the pilot die, to test death and respawn.
- `CAUCASIAN_EAGLE_VISTA=1` holds the camera on the mountain.

**Known gaps**
- Nobody has played this version with keyboard and mouse. Difficulty and length for a real player are estimates.
- Length: the test pilot, which never dies or stops, needs 125 minutes for the road. A person's time is not measured.
- Death and respawn were tested by the test pilot, not by clicking the death screen.
- Creatures are rigid models moved by code; they have no animation.
- No sound. No installer. Engine default menus.
- The look is not at commercial "AAA" level: generated terrain and textures, single-pass generated models.

## 0.1.0 · 2026-11-10

First version as its own project. Engine: Doorways-D3D11, see `git submodule status` at the `v0.1.0` tag.

**Project**
- Caucasian Eagle split out of the Doorways engine repository into `kalk1t/Caucasian-Eagle`; the engine is the `engine/` submodule.
- The game plugs into the engine through `DoorwaysGame.h` (`CaucasianEagle::Game`).
- Own window title, own settings and save folder (`%LOCALAPPDATA%\CaucasianEagle`).
- Build, stage and package scripts; installer script (not yet compiled).
- Documentation: technical plan, milestones, story bible, manual, glossary.

**Game (carried over from the work inside the engine repository)**
- Nyxara, daughter of the eagle: winged hero with the swords Moonfeather and Talonshade and a beaked eagle helm, 31 animation clips.
- Moonlit Sanctuary map in dark green and moonlight.
- Upright wing-beat flight for travel, Shift wing boost, lean and bank.
- Sword slashes alternating left and right, charged slash, sword exchange, sword barrier.
- Skills on F, V, C (Talon Dash, Feather Cyclone, Moon Leap) that cancel sword attacks instantly; six skill-plus-click combos.
- Hit-stop, camera shake and impact effects.
- Shade enemies with health bars; graphical HUD.
- Overhead orbit camera that can also look level and upward.
- Raised head pose, black eyes, brightened textures.

**Known gaps**: no story delivery, no sound, no installer, engine default menus. See `techplan/10-tech-debt-risks.md`.
