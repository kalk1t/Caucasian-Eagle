# Glossary

[← Project index](../README.md)

Names used in the story, the code and the assets. One name per thing.

## Story

| Name | Meaning |
|---|---|
| **Amirani** | The hero of the Georgian legend, chained in a cave on the Caucasus Range for fighting against God |
| **The Eagle** | The bird set to feed on Amirani's liver; Nyxara's father; the game's final opponent |
| **Nyxara** | The Eagle's daughter; the playable hero |
| **The Open Cave** | The day, once every seven years, when the cave opens and Amirani can be seen; the game's prologue |
| **The Chain** | What holds Amirani; renewed every year by the Smiths |
| **The Smiths** | Blacksmiths appointed by God who renew the chain each year on the Thursday of Passion week |
| **The Dog** | Amirani's faithful dog, licking the chain thin; named Q'ursha in the legend |
| **Shade** | The dark wraith enemy of v0.1; not part of the journey |
| **The Road to Amirani** | The journey: one road of twelve regions from its first gate to the mountain |
| **Region** (code: stage) | One of the twelve named parts of the road, each with its own ground, weather and creatures |
| **Memory of the cave** | One of twelve short recollections of Amirani, found in old gold chests |
| **Kajeti** | The fortress of the Kaji, who in this game are the Smiths |

## Creatures of the road

| Name | Meaning |
|---|---|
| **Chinka** | Small, fast wood spirit; attacks in packs |
| **Ali** | Hovering water spirit; keeps its distance and throws orbs |
| **Kudiani** | Witch; keeps its distance and throws green fire |
| **Ochokochi** | Goat-man with a chest blade; fast, leaps |
| **Almasty** | Wild man of the heights; strong, throws rocks |
| **Kaji** | Spirit smith; heavy hammer blows and thrown embers |
| **Uaig** | One-eyed giant; slow, hits very hard |
| **Devi** | Three-headed ogre; the strongest ordinary creature |
| **The Gveleshapi** | Dragon of the lake; boss of the Dragon Lake |
| **The Devi Lord** | Boss of Devi's Gorge |
| **Rokapi** | Queen of the witches; boss of the Witches' Hill |
| **The Giant** | A Uaig of a boss's size (8 m); the Eagle's Ascent's second boss, in its forest. Strikes the ground round it |

## Hero and combat

| Name | Meaning |
|---|---|
| **Moonfeather** | The silver sword (longer, 1.02 m); starts in the right hand |
| **Talonshade** | The dark-green sword (0.92 m); starts in the left hand |
| **Left Talon / Right Talon** | The two alternating sword slashes on left click |
| **Charged Slash** | The right-click heavy attack |
| **The slide, the thrown swords, the feather storm** | The three skills (F, V, C) (the finishing throw at a broken creature was taken out on 2026-11-19). Since 0.5.2 only F is in the game: V and C stay in the Character Lab (`NyxaraKit::SetSkillsVC`). Since 0.5.4 the slide is a dash, on foot only: 13 m in a second (the owner, 2026-11-25: three times as fast and three times as far as it was; 2026-11-26: 0.7 of that distance, and "F should not work while flying"), cutting every creature within a blade's length of her way, by her way and not by the clock (`SlidePace`, `SlideFar`, `SlideWide`, `SlideCutEvery` in `NyxaraKit.cpp`). The twisted dash, F in flight until 0.5.3, is kept in the kit and off (`NyxaraKit::SetFlightDash`) |
| **Long move in pieces** | 0.5.4, in the engine: a move on foot longer than the game's `LongMovePiece` (`GameSession()`, 0: off) is made in pieces of that length, her height set at once on the ground each reaches. Her slide sets it (0.15 m): made in one piece, half a metre a step, it stalled on a climb, the ground ahead a step too high for her feet. So made it goes up what her easy pace goes up, every slope that is no rock face (60°). Her going on foot at three times her pace (the shadow dash) sets it too (0.075 m): in one piece, a quarter of a metre a frame at her run, she would stall on slopes over about 18°. Code: `DoorwaysPhysicsSystem::MovePlayer`, `DoorwaysPhysicsMoveRequest::LongMovePiece`; her kit's `SlideStep`, `FootStep` |
| **Thread step**, **her soul (Q)** | 0.5.4: Q leaves her soul where she stands (she as she stood, in pale blue light) and throws her 6 m on; six threads of light run from it to her for as long as she likes; Q again pulls her back to it along them in a second at most, untouchable, through whatever stands between (the map's rules leave her alone on her way back); then Q rests 15 s. Q works out of a cut, a combo or her slide, and frees her as soon as she is thrown. Tried in the Character Lab with WING TETHER and WRAITH COIL, then as one of four ways of it (THREAD STEP, SLIP, VAULT, SHATTER); the owner chose it both times (`NyxaraSoul`, `NyxaraKit::LetSoulGo`, `EaglesAscent::PutHere`; the game's test `CAUCASIAN_EAGLE_DEMO=15`) |
| **Shadow dash**, her way of going | 0.5.4: how she goes on foot in the game, at three times her paces as made (W 9.6 m/s, Shift 16.2, Ctrl 5.0): crouched and bowed far forward, her arms and wings swept back, long low strides with a flight between each. Made of her own walk and run clips, timed anew as she goes: a foot down goes on as fast as the ground goes under her, so it does not slide. One of nine ways of going (AS MADE and eight) tried in the Character Lab, P her pace and J the way; the owner chose it, the fourth, and the pace: "i like 4th walk style - i like the speeded up also". Not F, her slide, a dash of its own (`FootPace`, `GaitStyle` in `GameConstants.h`; `NyxaraKit::SetFootPace`, `SetGaitStyle`, `GaitStyles`) |
| **Eagle**, her way of flying | 0.5.4: how she flies in the game: deep slow beats and long glides, her body long and level and her legs trailing, her wings half drawn in at speed; two great beats to rise; she lands braking, her wings cupped forward; her swords on her back unless she is armed. Posed by code from her skeleton every frame, not played from her three flight clips (Fly_Rise, Fly, Fly_Land). One of ten ways of flying (and AS MADE) tried in the Character Lab (O); the owner chose the first: "i like 1st most - so implement it ingame" (`FlightStyle` in `GameConstants.h`; `NyxaraKit::SetFlightStyle`; `NyxaraFlight.h`) |
| **Twin Talons, Rising Eagle, Tempest Triple, Whirlwind Talons, Soul Weave, Moonfall Dive** | The six combos |
| **Flair** | 0.5.4: a way her gear fights: the light behind her blades, where a blow bites and round a creature she kills, with its own combo (each button's string of moves) and pace. Nine were made, three for each gear (fast and sharp, heavy, flowing), to be tried in the Character Lab (G the gear, 1 2 3 the way); the owner chose the flowing one of each for the game. As she set out, before her first gear, she has none (the owner, 2026-11-26: "normal nyxara should not have effects"): her blows' blood and their weight only. Code: `NyxaraFlair`, `NyxaraKit::SetFlair`, `AscentGrowth::GearFlair` |
| **Sky Wheel, Fire Dance, Moon Wraith** | The three flowing flairs, the game's: THE HIGHLANDS' (the wind: wide blue trails that hang, golden rings, feathers), AMIRANI'S FIRE's (sheets of flame, embers turning round her) and MOONLIT's (teal trails and their pale echo, dark smoke, wisps). The other six stay in the Character Lab: Gale Edge, Stonebreaker, Cinder Razor, Forge Hammer, Silver Thread, Glacier. As she set out she has none of them (for a day she had Sky Wheel in her own blue: `CAUCASIAN_EAGLE_NYXARA_FLAIR=blue` shows that, to compare) |
| **Doomed** | A creature of the road brought down by what would have been her killing blow (since 0.5.4 one such blow in two, by chance: `NyxaraKit::SetFinishChance`; the others kill it): dazed, a violet ring under it, waiting (3 s) for her finishing kill |
| **Finishing kills: Reaper's Wheel, Mountain Breaker, Tempest Flurry, Headsman** | The four ways she kills a doomed creature (since 2026-11-20 every creature, the bosses too), one at random each time (never the same twice running), chosen by the owner of ten made (`NyxaraKit`: `Kept`). The other six (Talon Cross, Eagle's Plunge, Gale Slide, Heartpierce, Moonlit Flash, Rising Talon) are kept for the Character Lab |
| **Looks** | Three examples of how Nyxara grows (THE HIGHLANDS, MOONLIT, AMIRANI'S FIRE), each with tiers 1 to 3 of wings, swords and armour (tier 0 her own). Since 2026-11-21 her nine levels on the Eagle's Ascent wear them in the owner's order 1-3-2 (levels 1 to 3 THE HIGHLANDS, 4 to 6 AMIRANI'S FIRE, 7 to 9 MOONLIT; `levels 1 3 2` in `Looks.txt`); in the Character Lab keys 1 to 3 choose one. Since 0.5.3 her three gears wear THE HIGHLANDS tier 3, AMIRANI'S FIRE tier 2 and MOONLIT tier 3 (`AscentGrowth::GearLook`). Her game bundle `MixamoV1` carries them; `art/nyxara/looks` |
| **Cocoon**, **wing shield** | The E guard since 2026-11-20 (the Sword Barrier before it): her wings wrapped round her, blue lights along them. Since 0.5.2 the wing shield: one press, 3 seconds, blows and orbs from every side stopped while she goes on fighting, then 7 seconds' rest |
| **The mountain (M)**, **the detailed map (Tab)** | M: the whole mountain in 3D, gone over with the mouse (`MountainView::Explore`); Tab from it: the map from above, drawn from the map's data (`AscentMap`, `assets/ui/Map/AscentMap.jpg`) |
| **Wing boost** | Faster travel on Shift |
| **Soul bar** | The blue bar under health (the engine's energy value), on the road; not her soul (Q) |
| **Hit-stop** | The brief freeze when a blow lands |
| **Arena** | A round clearing on the road where creatures ambush her; the way on stays shut until they are beaten |
| **Open clearing** | A clearing with standing stones and a chest where nothing ambushes her (nine of them; `Road.txt` kind 3) |
| **Trap** | One of nine larger dangers on the road, each beaten once: the gate run, watchers, rune stones, wind teeth, cracked seal, Kaji press |
| **Place** | One of 26 named places beside the road; finding it gives crystals and a line in the journal |
| **Wave** | One group of creatures in an arena fight; two or three per arena |
| **Old gold chest** | A breakable chest; holds coins, a potion and a memory, a Moon Shard, an Eagle Feather or a Soul Ember |
| **Moon Shard** | Chest reward: 15 crystals (until November 2026 a Moon Tear, which healed) |
| **Eagle Feather** | Chest reward: each one raises her damage by 3% for the rest of the journey |
| **Soul Ember** | Chest reward: resets skill cooldowns and raises her damage by half for 20 seconds |
| **Windup** | The moment before a creature's blow lands, shown by a bright ring; the time to step away |

## Places and assets

| Name | Meaning |
|---|---|
| **Moonlit Sanctuary** | The walled garden of version 0.1. No longer part of the world: the journey begins on the road (its Blender source is still in `art/moonlit-sanctuary`) |
| **RoadToAmirani** | Scene id of the world: the whole road and the land round it |
| **Road.txt** | Data file written by the world builder: the path, arenas, chests, props and lights |
| **Mountain of Amirani** | The landmark at the end of the road: peak, cave ledge, Amirani, the eagle, the dog, the chains. Code: `AmiraniMountain` |
| **Ledge** | The flat shelf of the cave where Amirani stands, 78 m above where the road begins; reachable only after the Eagle falls |
| **The Eagle's Ascent** | Map A played on its own; since 0.5 what `CaucasianEagle.exe` starts (`--eagles-ascent` still asks for it; `--road` or `CAUCASIAN_EAGLE_ROAD=1` starts the road instead): the climb from the sea to Amiran's court at 4,200 m. Code: `EaglesAscent` (the map's rules), `EaglesAscentGame` (the mode) |
| **Eagle Nest** | One of the nine places on the Eagle's Ascent where she is saved: standing in it, H saves (0.5: a nest no longer heals or saves by itself); she wakes in the one she saved at after a fall, and all since that save is undone (`EaglesAscentGame::RollBack`). In 0.5.2 the only place she levelled up; since 0.5.3 nothing is bought there (her quests, Q, are taken anywhere). Its model: `Landmarks/EagleNest/EagleNest` (`art/map/eagle_nest`) |
| **Clay jar** | 0.5: one of the 3,710 Georgian clay vessels (qvevri, doki, kotso, kvibri) in groups of 3 to 5 every 30 to 50 m beside the Eagle's Ascent's roads; her blows break them; a potion (one in twelve), a crystal or two, or nothing. Since 0.5.2 the crystals in them are laid by the map's `level` lines: each stretch's share on the way and off it, all of it; since 0.5.3 they are of the stretch's kind (wind, fire, ice). Code: `Wayside` |
| **Hidden place** (on the mountain) | 0.5 to 0.5.1: one of 73 cairns with a gold light, in each long cave's heart and at the mountain's named places; standing at one found it: 8 crystals. Taken away on 2026-11-21 (the owner); a save's old `place` lines are skipped |
| **Placing model** | `art/map/ascent_creatures.py`: where the Eagle's Ascent's nests, bosses and creatures are, and (0.5.2) each level's price and what its stretch holds (`level` lines, the ledger it prints) and the groups around the way (`around`); writes `game/assets/Maps/EaglesAscent.creatures.txt` |
| **Level** (on the mountain) | 0.5.2 only (replaced by her gears and quests in 0.5.3; the map's `level` lines still set what each stretch holds): one of the nine levels of her wings, swords or armour, bought only in an Eagle Nest (8, 14, 21, 27, 33, 39, 46, 52, 58 crystals), each with its strength and its look. Code: `AscentGrowth` |
| **Stretch** | 0.5.2: the way between two Eagle Nests (the first from the shore to the first nest). What a stretch holds, on the way and off it, is three times the price of the next nest's level (the first one crystal more). Since 0.5.3 a stretch's crystals are of its kind: wind up to the third nest, fire up to the sixth, ice beyond (`AscentGrowth::CrystalKindOf`, `CrystalNests` in `GameConstants.h`) |
| **Group around the way** | 0.5.2: a group standing 9 to 18 m off the way beside it, where nothing stood before, about every 150 to 400 m (17 of them in 0.5.2; 16 since 0.5.3, around-0522 left out: its only place became a cliff edge once rock in steep cells is collision). The game finds its ground when the map is loaded (`EaglesAscent::PlaceAround`: walkable, level, reached on foot from the way, no trunk, not in a cave, 26 m across the ground from every other group and the mouths of caves with creatures, the nearest group from where its path leaves the way, no group further up the way within 26 m of there, and 100 m from a cave's group when the two are more than three: a fight of its own); it keeps to a short path of its own out from the way |
| **Gear** (on the mountain) | 0.5.3: what she wears and how strong she is: her own (AS SHE SET OUT), then THE HIGHLANDS, AMIRANI'S FIRE and MOONLIT, each earned at once when a set of her quests is done. Each is new wings, swords and armour, more damage (x 1.6, 2.3, 3.2), less taken (x 0.8, 0.6, 0.3) and longer flight (15, 22, 30 s; her own 10). Since 0.5.4 each also fights its own way (its flair); her own gear has none. Code: `AscentGrowth` (`Growth::State::Gear`, `GearFlair`), `GameConstants.h` (`GearDamage`, `GearTaken`, `GearFlightSeconds`) |
| **Quest** (on the mountain) | 0.5.3: one of her 18 tasks, shown with Tab anywhere (Q until 0.5.4), in three sets (3, 6, 9) offered one at a time; bought with the set's kind of crystals (wind, fire, ice); beat n creatures, break n clay jars, or reach an Eagle Nest; counts only once taken; a set done gives the next gear. Since 0.5.4 every quest taken and not yet done is listed at the left of the screen (five at the most before, and the done ones stayed); a done one turns gold, is struck through and leaves the list in 1.9 s (`AscentQuests::Leaving`, `Listed`, `DrawTaken`), and stays in the quests' window. The 18 are a first set, to be chosen again with the owner. Code: `AscentQuests` |
| **Crystal kinds** | 0.5.3: wind, fire and ice, by the stretch of the way where a creature or jar leaves them (wind to the third nest, fire to the sixth, ice beyond); her save keeps the three counts (`crystals w f i`) |
| **Steep walls** | 0.5.3, engine: faces of walkable collision too steep to stand on (normal.y under 0.5) are walls between her step height and the top of her body: she slides along a cliff or down a slope, and still walks off a ledge. Found when the map loads (`PrepareDoorwaysRuntimeSceneSteepGround`); `GameSession().SteepGroundBlocks`; `CAUCASIAN_EAGLE_STEEP_WALLS=0` turns them off in a test |
| **Drawn top** | 0.5.3: the map's `tops` record, the top of the rock as drawn, by ray cast, 1 m apart in rock and steep cells; the game holds her where the drawn rock ahead stands over her feet (`EaglesAscent::DrawnTop`; `CAUCASIAN_EAGLE_ROCK_RULES=0` turns the game's rock rules off in a test) |
| **Group** (on the mountain) | One to three creatures with one place on the Eagle's Ascent, standing or walking 20 m to and fro (a patrol); wakes as she comes near, never more than three alive at once |
| **Sky dome** | The painted night sky drawn around the camera by the game |
| **EagleV1** | Nyxara's compiled character bundle, `Characters/Nyxara/EagleV1` |
| **Bundle** | One folder holding a character's mesh, skeleton, materials, textures and clips |
| **Clip** | One authored animation, a `.danim` file |
| **Rig signature** | A hash identifying a skeleton; the pipeline refuses clips made for a different one |

## Project and engine

| Name | Meaning |
|---|---|
| **Doorways** | The engine (Direct3D 11), repository `kalk1t/Doorways-D3D11` |
| **Engine submodule** | The `engine/` folder: a pinned commit of the engine |
| **Game module** | `CaucasianEagle::Game`, the object the engine calls |
| **Game session** | `DoorwaysGameSession`: the options the game sets and the engine reads |
| **Kit** | `NyxaraKit`: the hero's combat code |
| **Stage folder** | `build/stage/CaucasianEagle`: the complete runnable game |
| **Showcase** | The scripted 30-second demo run of the road (`CAUCASIAN_EAGLE_DEMO=1` with `--road`; without it, since 0.5, the Ascent's own smoke run) |
| **Test pilot** | An unattended run in which the game plays the whole road by itself (`CAUCASIAN_EAGLE_PILOT=1`, which always starts the road); used to test the journey |
| **Grass fields** | 0.5: the Eagle's Ascent's grass, built round the camera in 12 m squares wherever the map's ground is painted meadow. Code: `AscentGrass` |
| **Trunks file** | `<map>.trees` (`Maps/EaglesAscent.trees`): every tree's foot, trunk radius and height, written by the Map Lab's builder; the game holds her off them (`EaglesAscent::HoldOffTrees`) |
| **Nature test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=5`): the grass seen at eight spots up the mountain, then runs at 24 trunks; `nature.tsv`, `NATURE_PASS` |
| **Jars' test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=4`; since 0.5.4 `tools\ways-test.ps1`): jars laid and counted, her cuts at ten groups, a save, more broken, a rollback and the save read back; `ways.tsv`, `WAYS_PASS`. With `-Dash` (`CAUCASIAN_EAGLE_WAYS_DASH=1`, since 0.5.4) she slides (F) through each group in place of her cuts, and slid at a cave's wall she must be in the cave still |
| **Creatures' tour** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=3`) that stands her before every group and boss, then in every nest, and checks they wake, see her, come at her, stay beaten and save her |
| **Pointer test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=7`) that works her window (I) and her menu (Esc) with a pointer of its own (since 0.5.3 her window shows her gear, with nothing to buy): the window's gears, CLOSE, Continue, Nyxara, Esc; it checks the pointer is free only while a window is open and that a click that closes one is not a sword blow; `pointer.tsv`, `POINTER_PASS` |
| **Crystals' ledger** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=8`): counts what each stretch holds (the way's creatures, bosses and jars; the caves' creatures, the groups around the way and the jars off it) against three times its level's price, checks 60 to 70 % of it on the way, every jar crystal laid, every group around the way placed, none closer than 140 m to the one before nor 26 m to any other group, each the nearest from where its path leaves the way, no group further up the way within 26 m of there, and none within 100 m of a cave's group when the two are more than three; `ledger.tsv`, `LEDGER_PASS` |
| **Quests' test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=11`, `tools\quest-test.ps1`): Tab opens the first three; quests bought, too few crystals refused, counted only once taken; a set done gives THE HIGHLANDS and is saved; the save read back; the next six; a quest taken after the save rolled back; since 0.5.4 also every taken quest listed (three, six and nine of them), a done one gone from the list and kept in the window; `quests.tsv`, `QUESTS_PASS` |
| **Boss film** | An unattended run (`CAUCASIAN_EAGLE_DEMO=12`, `tools\boss-film.ps1`): she is put before one boss (`CAUCASIAN_EAGLE_BOSS`, the Eagle by default), kept whole, and it fights her; `boss.tsv` has its place and ground every 0.1 s, pictures every 0.5 s; `BOSS_DONE`. Since 0.5.4 `-Walk` (she goes on foot up the way at a creature of it, `-Boss way-0200`, and cuts when she is there), `-Keys "4:F,6.5:L"` (her keys by the film's clock) and `-SlowMs` (a slow computer's frames) |
| **Camera sweep** | An unattended run (`CAUCASIAN_EAGLE_DEMO=13`, `tools\camera-test.ps1`): the camera's pitch stepped from lowest to highest at spots up the way (`camera.tsv`, pictures), or with `CAUCASIAN_EAGLE_CAMERA_WALK` her walk up and down stairs (`camera-walk.tsv`) |
| **Off-the-way test** | An unattended run (`CAUCASIAN_EAGLE_DEMO=14`, `tools\side-test.ps1`): about every 40 m of the way she runs straight off it to the left and to the right until 30 m off, stopped, or under the land; `side.tsv` (free, steep, steep_wall, tree, water, step, BLOCKED, FELL_THROUGH, timeout). Since 0.5.4 `-Dash`: she slides (F) off the way and runs on from where the slide leaves her; `-Ahead`: one run a place, up the way itself; `-Jog`: at her easy pace (W, 9.6 m/s; 3.2 before the shadow dash) in place of her run (Shift, 16.2; 5.4); `-Trace`: every frame of every run in `side-trace.tsv` (her place, the land and the walkable ground at it, the camera's place and the land at it), with `-Film` and `-FilmEvery` pictures too; `-SlowMs 33`: every frame waits that many thousandths of a second more, a slow computer's frames (`CAUCASIAN_EAGLE_SLOW_MS`, any unattended run: 16 about 31 frames a second, 33 about 21, 60 about 12, 100 and more 10, the engine's longest frame) |
| **Soul test** | 0.5.4: an unattended run (`CAUCASIAN_EAGLE_DEMO=15`, `tools\soul-test.ps1`): every 400 m of the way (but where she must fly) she cuts and leaves her soul (Q) in the cut, is free at once, runs on 3 s and is taken back to it (Q); how far her leaving carried her, how long her way back took, how far from it she ended, how much of it was drawn, and that she stands where it stood; at the first place also Q just after she is back (nothing: it rests), Q in flight (nothing), her window opened and closed with it away, and a nest's put letting it go; at the second Q again from the air; `soul.tsv`, `SOUL_PASS` |
| **Long step** | 0.5.4, in the game: a step on foot of over 0.35 m between two frames (her slide's is a third of a metre at 60 frames a second and 2 m at 10; her run's 0.27 m at 60 since the shadow dash, a tenth before) is gone over in steps of 0.2 m by the caves' walls, the trees' hold and the two rules at rock, and she is held at the first that would not be taken. Without it her slide (then half as fast again) went through a tree at 31 frames a second and fewer. A step of 3 m or more that 45 m/s would not make in its frame is no step: she was put there. A test's baseline: `CAUCASIAN_EAGLE_LONG_STEPS=0`. Code: `EaglesAscent::KeepOutOfRock`, `HoldOffTrees`, `LongStep`, `PutSpeed` |
| **Pivot's limit** | 0.5.4: the camera's pivot (the point at her chest it turns round, which follows her height smoothly) is never more than 1 m from her height. Her slide up a steep slope or stairs left it up to 2.9 m behind, under the ground at her feet, and the camera inside the land for a tenth of a second. `CameraPivotLagMost` (`GameConstants.h`; engine `GameSession().CameraPivotLagMost`, 0: no limit). A test's baseline: `CAUCASIAN_EAGLE_PIVOT_LAG=0` |
| **Frame limit** | 0.5.4: the most frames a second the game makes, chosen in Settings (*Frames a second, at most*: 30, 60, 120, no limit) and kept in `PlayerPreferences.ini` (`FrameRateLimit`, 0: none). The engine waits out the rest of each frame's time after it is presented (`App::HoldFrameRate`: a high-resolution timer, then the last millisecond spun). With vertical sync on the screen's own rate is the most as well. With it off a frame is presented at once and may tear (`DXGI_PRESENT_ALLOW_TEARING`, where Windows allows it): without that Windows held such frames to the screen's rate much of the time, and "no limit" or a limit over it was not kept. An unattended run has no limit unless `CAUCASIAN_EAGLE_FRAME_LIMIT=<n>` gives one (-1: the player's own); `CAUCASIAN_EAGLE_USER_FOLDER=<name>` gives a run a settings folder of its own |
| **Journey** | `Journey`: the code for the road, creatures, arenas, chests, story cards and the ending |
| **Sandbox** | The engine's own executable, `Doorways.exe`, running with no game |
| **NyxaraLooks** | `game/src/NyxaraLooks`: reads a bundle's `Looks.txt` and shows a look (parts shown, textures, tints, light, wing span) through `WorldRenderer::SetCharacterMaterialLook` |
| **Asset builder** | The engine tool that compiles glTF into engine formats |
| **Hyper3D Rodin** | The service used for the looks' swords, pauldrons and crowns (2026-11-21; `art/nyxara/looks/GENERATIONS.tsv`) |
| **Meshy** | The service used to generate the hero, the mountain figures, the creatures and some scenery |
