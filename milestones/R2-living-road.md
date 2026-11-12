# R2 · The living road (v0.3.0)

[← Milestones](README.md)

**Goal.** Turn the bare road of R1 into a place: cities, sound, things to find and keep, windows and menus of the game's own, land to walk, attacks that link, and models cleaned one by one.

**Status.** Built and captured on 2026-11-11. **Only partly played by hand.** Released as v0.3.0 on 2026-11-11 with the first installer ([M2](M2-installable.md)).

How to read this file: each part lists what was built, then how it was checked. "Seen" means a screen capture of an unattended run; "log" means the test pilot's debug log; "not verified" means nobody pressed the keys.

## Parts

### R2.1 The world
- [x] Seven Georgian cities on the road, each with a gift, a verse and a keepsake.
- [x] Earth roads, rolling land, grass, trees, clouds.
- [x] A gate at the start of each region, built as a gate: piers, buttresses, a round arch with a keystone, merlons.
- [x] **Open land:** the whole map can be walked. Water, cliffs, fights, the causeway and the last climb limit it; city houses are walked round.
- [x] **The way is shown:** off the road a compass and a trail of lights point back; past an unwon fight they point to it.
- [x] **Trouble on the road:** cursed ground, falling stones, wisps between the arenas.
- Checked: cities, land, grass and trees seen; open land by log (40 m off the road, grounded on the land, held at the marsh's water); gate rendered in Blender; compass, trail of lights and the three troubles seen in captures; falling stones measured to hurt.
- Not verified: walking the land by hand; walking round city houses; harm of cursed ground and wisps; how hard the troubles feel in play; regions other than 1 to 4 after the last rebuild.

### R2.2 Fighting
- [x] Creature tactics: heavy blows, second blows, hit and run, round the guard, ground slams, aimed shots.
- [x] Health returns slowly; chests no longer heal.
- [x] No dodge. The barrier cuts any sword attack.
- [x] Attack timing: kept clicks, slash into Charged Slash, combos that never cost the skill a hit, flourishes that can be cut.
- Checked: the pilot wins arenas at 60 frames a second.
- Not verified: every timing change, by hand. Difficulty for a person.

### R2.3 What she has
- [x] Her window (I): four stages of wings, swords and armour; training; a second page with charms, runes, feathers, verses, keepsakes and chain links.
- [x] A fourth ending with all three chain links.
- Checked: both pages seen with sample items.
- Not verified: drops, pickups and the fourth ending in play.

### R2.4 Screens of the game's own
- [x] HUD, story journal (F1), minimap and world map (M).
- [x] Start screen drawn by the game in its own style (five rows, keyboard and mouse) over a rendered 3D picture.
- [x] Who made it: on the start screen, the Credits page, the guide, README and manual.
- [x] Her menu (Esc) in place of the engine's pause menu.
- [x] Guide: seven pages, with what the game needs to run and who made it.
- Checked: the game's own start screen, the credits page, guide page 1, her menu seen in captures. Keys and mouse on the start screen: not tried.
- Not verified: any key in them; the Health page.

### R2.5 Sound
- [x] Synthesised effects, Georgian-style music in five pieces, Windows voices for the story.
- Not verified: none of it has been listened to by the author of the code. Voices are placeholders.

### R2.6 Models
- [x] Every Meshy model refined in Blender with its own settings (`art/refine/refine_assets.py`).
- Checked: report and before / after pictures for all 15.
- Not verified: the refined creatures seen moving in a fight.

### R2.7 Engine
- [x] Game interface additions: menu texts and backdrop, own pause menu, guide button, scene collision, healing, suppressed inventory.
- All in `engine/techplan/06-game-interface.md`.

### R2.8 Documents
- [x] Manual, changelog, glossary, tech plan (both repositories), art notes, this file.

## ✅ Done when
- [x] It builds without warnings and the pilot plays arenas with it.
- [ ] **The author plays it** from the start screen: walks the land, fights with the new timing, opens every window and her menu, reads the guide.
- [ ] The music and sounds are listened to and accepted or sent back.
- [ ] It is committed in both repositories.

## Open after this milestone

| Item | Why it is open |
|---|---|
| Creatures on the land | They keep to the road. Off the road nothing can reach her except shots. |
| Going round a fight | The land lets her pass an unwon arena. Nothing is won by it and the compass points back, but nothing stops her either. |
| Camera on hills | The camera was made for the road; on steep land it may look into a slope. Not looked at. |
| System requirements | Estimates. The game has run on one fast computer. |
| Potions in chests, the road sunk into the fields, voices | Three questions put to the author, unanswered. |
| Everything listed "not verified" above | Needs a person at the keyboard. |
