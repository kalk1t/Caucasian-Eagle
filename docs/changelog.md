# Changelog

[← Project index](../README.md)

Newest first. Each version names the engine commit it was built against.

## Unreleased

Engine: Doorways-D3D11 `22172eb` (adds the environment hook the sky uses).

- **The mountain of Amirani** stands beyond the play area: Amirani chained in the cave, the eagle lunging at his liver, his dog at the ankle chain.
- **New night sky**: stars, Milky Way, a green aurora, cloud and distant Caucasus ridges, with a large full moon beside the mountain. Still night.
- Moonlight, ambient colour and haze retuned so the mountain reads from the play area.
- Capture mode `CAUCASIAN_EAGLE_VISTA=1` holds the camera on the mountain.

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
