# Changelog

[← Project index](../README.md)

Newest first. Each version names the engine commit it was built against.

## 0.3.0 · 2026-11-11 · The living road

Engine: Doorways-D3D11 `d86a39b` (`Renderer::LoadUiTexture`, `Renderer::DrawUiSprite`, game controller input, the game's own settings, spawn point, pointer and start screen).

**Installer.** The first version that installs: `CaucasianEagle-0.3.0-Setup.exe` (Inno Setup, for one Windows user, no administrator question, Start-menu entry, uninstaller that keeps settings and saves) and `CaucasianEagle-0.3.0-win64.zip`, each with a SHA-256 file. The package is assembled fresh from the two repositories and carries the Visual C++ runtime beside the executable. The executable has its own icon (the Eagle before the moon, `art/ui/build_icon.py`) and version information; the game window shows the icon. Neither file is signed, so Windows warns before running the installer. Installed and started on the development PC only; no second PC has been tried.

**Who made it.** The start screen, its Credits page, the last page of the guide, the README and the manual now say it plainly: the game is created by Giorgi Samkharadze and built in Doorways, a custom engine founded by Giorgi Samkharadze in February 2026.
**The journey begins on the road.** The Moonlit Sanctuary is gone from the world; the night stays. Nyxara begins on the first stones of the road, before its first gate, with Telavi ahead. The chasm that ringed the sanctuary is filled, the land there can be walked, and a fall before the first arena is won returns her to the start of the road. (Engine: `DoorwaysGameSession::OwnSpawn`.)

**The game's own settings.** Settings, from the start screen and from her menu, is now the game's own window in her panel, gold and ink, with only what this game uses: window and its size, vertical sync, shadows, four volumes, mouse speed, reversed look, interface size, frame counter. The engine's settings screens no longer open for the game. (Engine: `DoorwaysGameSession::OwnSettings` and `Settings`.)

**Pointer and mouse.** The game has its own mouse pointer. It is free while one of her windows is open; her menu rows, the cards of Nyxara's window and the runes can be chosen and taken with a click. (Engine: `Cursor`, `FreeCursor`.)

**A calmer camera.** The mouse and the right stick turn the camera at 0.7 of the engine's speed (`CameraLookScale`; engine: `LookScale`).


**The start screen is the game's own.** It was the engine's menu with the game's words. It is now drawn by the game in the same panel, gold and ink as Nyxara's windows: five rows (begin or continue, settings, guide, credits, leave), keyboard or mouse, the rendered picture behind, and who made it along the bottom. Engine: `OwnStartScreen`, `StartRequest`, `IDoorwaysGame::DrawStartScreen`.

**Trouble on the road.** Fighting is no longer all there is. Between the arenas lie cursed ground to walk round, stretches where stones fall on her (a ring shows first), and wisps that swing across the road. They harm her through the same scale as creatures' blows.

**The whole land is open, and the way is shown.** Nyxara is no longer held to the road nor to a strip beside it: the whole map can be walked and flown over. Water, cliffs and the far mountains stop her, and she walks round trees, boulders, towers, gate piers and the houses of cities. Off the road a compass at the top points back to it with the distance and a trail of gold lights runs along the ground toward it; past a fight not yet won it points back to that fight. A fight still holds her in its clearing. A clearing can be walked round, but the arenas are won in order and the mountain opens only when all are won.

**Attack timing.** A combo can no longer cut off the last hit of its skill (it could: the combo opened at 55 % of the skill, before Talon Dash's lance and Feather Cyclone's last turn). Early clicks are kept: slash into slash, slash into Charged Slash, Charged Slash into slash, and a click during a skill starts its combo when the last hit lands. The flourish after a skill or combo can be cut by a sword attack, the barrier or the swap. The barrier cuts any sword attack at once. The combo window after a skill is 1.2 s (was 1.0).

**Gates.** The gate at the start of each region was two posts and three stones. It is now built as a gate: stepped plinths, banded piers with buttresses and capitals, a round arch of fifteen wedge stones with a keystone, a wall over it and seven merlons.

**Models refined one by one.** Every Meshy model (ten creatures, the chest, dead tree, fir, rocks, tower) goes through Blender with its own settings before it is compiled: seams welded (each arrived as thousands of unconnected faces), stray fragments deleted, normals made consistent, hard edges kept by angle, scenery stood on the ground. Shapes and textures are unchanged; the files are 10 to 20 % smaller.

**Fixed.** The world did not stop behind her menu. Her menu closed when Settings was left. A giant could start a blow while already striking the ground. A boulder beside the road could push her on the road.


**A rendered start picture.** The picture behind the start screen is now rendered in 3D from the game's own models (`art/ui/render_cover.py`, Blender): Nyxara hovering on a cliff with her wings spread, the Eagle before the moon, the mountain of Amirani, a chest, cloud banks. `build_start_screen.py` adds the title over it. The painted version remains as the fallback when no render exists.

**Her own menu.** Esc no longer opens the engine's pause menu. It closes whatever window is open, or opens Nyxara's menu in the same panel, gold and ink as her other windows: continue, Nyxara, the story, the map, settings, guide, to the start screen, leave. The road stands still while it is open. Settings still opens the engine's settings screens.

**No dodge.** The dodge and its key (O; B on a controller) are gone. The engine action is suppressed by the game.

**Her window shows every stage.** Each of her three things (wings, swords, armour) now lists all four of its stages by name: those behind her, the one she wears (marked WORN, in the colour of its tier) and the one that comes next (marked NEXT, with its price under the list). The engine's old inventory, which slid in from the left on the I key, no longer opens at all: I opens only Nyxara's window. The window was seen in a capture; that the old inventory stays shut was read in the code, not pressed.

**Georgian music.** All five pieces were rewritten. The first version moved in a gapped five-note scale and sounded East Asian. Now: three voices as in Georgian song (a held bass that steps down to the seventh and comes home, a tune that moves by step, a second voice a third under it), chords of first, fourth and fifth that close on a bare fifth, a strummed panduri, a flute and a drum. The city is a round dance in 6/8 and the fight is the war dance in 5/8. Still made by a script, not played or sung by people, and not heard by its author: judge it by ear.

**Old earth roads and rolling land.** The road is no longer paved: it is an earth road with wheel tracks, stones trodden in and grass creeping over it, dark in Kajeti and on the witches' hill, snowed over in the pass, mud across the marsh. The plank walk over the marsh and the stone bridge over the lake became earth banks without posts or parapets. Only the bridge out of the sanctuary and the last climb to the mountain are still laid stone. The land beside the road now rises and falls in long soft swells, and grass grows right up to the road's edge. The whole world, the cities and the map were rebuilt for this. Seen in captures in three regions.

**Creatures fight with more variety and more sense.** Heavy blows (yellow ring), second blows from quick creatures, Chinkas that spring back and jump aside, wounded small creatures that fight hit-and-run, creatures that run round a guard, giants that break the ground under her from a distance, shots aimed ahead of her, fans of three from spirits and witches, a ring of fire from the Kaji. Details in the manual, How creatures fight. The test pilot won arenas against them at 60 frames per second; nobody has played against them by hand, and the numbers are not tuned.

**Health returns slowly; chests no longer heal.** Nyxara gets 1 health back every 2 seconds. Chests give no health any more: the Moon Tear became a Moon Shard (15 crystals), feather chests and hoards no longer close her wounds, and a potion she cannot carry is left behind instead of drunk. Potions in chests remain, to carry and drink with Z.

**What she carries.** Creatures now drop charms, and Nyxara keeps what she picks up. Her window (I) has a second page (D) that shows it: charms of the eight kinds of creature (three of a kind are a set that helps against that kind), the three links of Amirani's chain (one from each guardian; all three add a fourth part to the end of the story), the verses of the road song (one per city), twelve feathers of her father hidden at the road's edge, a keepsake from each city, and fire, frost and moon runes from gold chests that she sets in her blades. All of it is saved. Seen in a screen capture with sample items; the drops, the pickups and the changed ending were not played through.

**Grass, trees and clouds.** Grass now grows beside the whole road, trees stand in woods back from it (broadleaf in the lowlands, firs in the high country), and a layer of cloud drifts across the sky. What grows differs by region. They are made by the game from four generated pictures and a height map of the land (`art/world/build_nature.py`); no outside models. Seen in screen captures in three regions at 60 frames per second; the clouds and the far regions were not looked at closely.

**Sound.** The game has sound for the first time: music for the start screen, the road, cities, fights and guardians; sword, skill, hit, footstep and wing sounds for Nyxara; growls, attacks and death cries for creatures; chests, crystals, city bells, wind, city voices and the fire of the walls; window sounds; and a storyteller who speaks the regions, the cities, the memories and the end. Nyxara says a few words in fights. Press **R** on the Legend page of the journal to hear the legend. The effects and music are made to be easy on the ear: soft starts, room reverb, nothing above 9 kHz, a flute and plucked strings over held strings in the music, and the same sound is never started twice within 0.07 seconds. All 106 sound files are placeholders made on the computer (synthesised effects and music, Windows voices) and are in `game/assets/sounds`; replacing a file replaces the sound. The engine gained playback of a game's WAV files (`DoorwaysGameAudio.h`).

**Walls of a fight.** A wall block that the camera stands next to now sinks, as blocks between the camera and Nyxara already did, so a block no longer fills half the screen when she fights at the wall. A death that the game only notices as "she is back in the sanctuary" is counted once, not once per frame.

**Start screen picture.** The painted picture now shows behind the menu (the menu's own dark layer was drawn over it).

Tested by the unattended test pilot and by screen captures at 1280 × 720. The F1 and F9 keys themselves were not pressed by hand in testing: the journal was held open by a test switch and the restart by a simulated hold.

- **Seven Georgian cities on the road**: Telavi, Tbilisi, Mtskheta, Gori, Kutaisi, Batumi and Mestia. Each has its own houses, gate and landmarks (wine jars and pergolas, the sulphur baths and the fortress, the walled cathedral and the church on the rock, the hill fortress and the cave town, the columned hall of the Golden Fleece, the lighthouse and boats, the Svan towers). No creatures wait inside a city. The first time Nyxara enters one she is healed and keeps that city's gift for the rest of the road (see the manual).
- **Arena walls.** The curtains of light that closed an arena are gone. When a fight starts, walls of stone blocks rise out of the ground across the road at both ends of the arena, with a band of fire along them; they sink back when the last wave is beaten.
- **Harder creatures, health in chests.** Creatures hit about 40 % harder, strike a fifth more often, and three may wind up a blow at once instead of two. Felling a creature now gives back 10 health (it was 7 to 34). Every chest gives 20 health besides its contents. These numbers were set by calculation, not by play.
- **Creatures move better.** They are a fifth faster, sprint to catch up when left behind, turn faster, run to where Nyxara is heading instead of where she was, swing out to both sides as they close in so a pack surrounds her instead of queuing, and circle her between blows. Spirits and witches fall back faster and strafe more.
- **Creatures move their bodies.** Every creature now has a skeleton: legs step and knees bend when it walks, arms swing, hips and shoulders twist, it draws an arm back and brings it down to strike (giants raise both arms, witches and spirits thrust their hands forward), it is knocked back when hit and its knees give way when it dies. Robes trail, the serpent's neck waves and whips, the Eagle's wings beat. Before, each creature was one rigid figure that was tilted and slid. The skeletons are worked out from the models by `art/monsters/rig_models.py`; no animation files and no Meshy credits were used. Checked in a rendered preview of the poses and by screen capture in the game; not every creature was looked at in every pose.
- **No more healing by itself.** Health no longer comes back between fights (it was 3 per second). Each creature felled gives 10 health. Telavi's gift changed with it: Kakhetian wine now adds 5 health per creature felled instead of doubling the old regeneration.
- **Start screen.** The game opens on its own start screen: a painted night picture of the Eagle before the moon over the mountain of Amirani with the title, and at the left Begin the journey / Continue the journey, Settings, Credits, Exit. The picture is drawn by `art/ui/build_start_screen.py`; replacing `game/assets/ui/Menu/StartScreen.png` with any other picture changes it. It is the engine's main menu with the game's words, without the Website and Forum buttons.
- **Crystals, evolutions and training.** Creatures leave crystals. The I key opens Nyxara's own window (the engine's inventory is no longer used): evolve her wings, swords and armour three times each, train power and defence eight levels each. When all three items reach a tier she herself evolves (three forms). Details in the manual, Growing stronger.
- **Stronger creatures along the road.** Health now grows 20 % per chapter (it was 12 %) and damage 7 % per chapter (it was 3 %), to match her growth. These numbers were set by calculation, not by play.
- **Potions.** Chests now hold healing potions instead of healing at once. She carries five and drinks one with Z (35 health). Mtskheta's gift is now 15 more health per potion.
- **Bigger fight cage.** The walls of a fight now rise 20 m beyond the clearing at each end, about 75 m apart instead of 33 m. The clearing itself is not wider: that needs the world to be rebuilt.
- **Fixed: jerking at a wall.** Nyxara was pulled back in 2 m steps when she pushed against a wall, which made her and the camera jump. She is now held exactly at the wall.
- **Fixed: the wall hiding Nyxara.** Wall blocks between the camera and her now sink out of the way.
- **Minimap** at the top right: the road, the next fight, chests, creatures and cities, turned the way the camera looks, with marks on the rim for the road ahead, the next city and the mountain.
- **World map on M**: the whole road with all seven cities, the fights won and ahead, where Nyxara is, and what each city is known for and gave her. The road stands still while it is open.
- **Game controller** (Xbox layout, XInput): the whole game can be played with it, menus included. This is an engine feature. It was built and the game was run with it compiled in, but **no controller was connected during testing**, so button behaviour is untested.
- **Story journal on F1.** The story is no longer written over the middle of the screen. F1 opens a journal with four pages (The Legend, The Road, Memories, The End) that fill in as the road is walked. The road stands still while it is open.
- **Notices are small and out of the way.** A new region shows its title at the top for five seconds. What a chest held shows in a small panel at the right edge. The name of a move shows just above the skill bar.
- **New HUD artwork.** Framed panels, bars, skill slots with icons and key badges, an emblem, a health trail, a red screen edge when hit or low on health. The artwork is generated by `art/ui/build_hud.py`.
- **Text in sentence case** instead of capitals, except labels.
- **Restart is on screen.** Hold F9: a bar fills for a second and a half and the journey begins again. The key is listed at the bottom right.

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
