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
| **The slide, the thrown swords, the feather storm** | The three skills (F, V, C); in flight F is the twisted dash (the finishing throw at a broken creature was taken out on 2026-11-19). Since 0.5.2 only F is in the game: V and C stay in the Character Lab (`NyxaraKit::SetSkillsVC`) |
| **Twin Talons, Rising Eagle, Tempest Triple, Whirlwind Talons, Soul Weave, Moonfall Dive** | The six combos |
| **Doomed** | A creature of the road brought down by what would have been her killing blow: dazed, a violet ring under it, waiting (3 s) for her finishing kill |
| **Finishing kills: Reaper's Wheel, Mountain Breaker, Tempest Flurry, Headsman** | The four ways she kills a doomed creature (since 2026-11-20 every creature, the bosses too), one at random each time (never the same twice running), chosen by the owner of ten made (`NyxaraKit`: `Kept`). The other six (Talon Cross, Eagle's Plunge, Gale Slide, Heartpierce, Moonlit Flash, Rising Talon) are kept for the Character Lab |
| **Looks** | Three examples of how Nyxara grows (THE HIGHLANDS, MOONLIT, AMIRANI'S FIRE), each with tiers 1 to 3 of wings, swords and armour (tier 0 her own). Since 2026-11-21 her nine levels on the Eagle's Ascent wear them in the owner's order 1-3-2 (levels 1 to 3 THE HIGHLANDS, 4 to 6 AMIRANI'S FIRE, 7 to 9 MOONLIT; `levels 1 3 2` in `Looks.txt`); in the Character Lab keys 1 to 3 choose one. Her game bundle `MixamoV1` carries them; `art/nyxara/looks` |
| **Cocoon**, **wing shield** | The E guard since 2026-11-20 (the Sword Barrier before it): her wings wrapped round her, blue lights along them. Since 0.5.2 the wing shield: one press, 3 seconds, blows and orbs from every side stopped while she goes on fighting, then 7 seconds' rest |
| **The mountain (M)**, **the detailed map (Tab)** | M: the whole mountain in 3D, gone over with the mouse (`MountainView::Explore`); Tab from it: the map from above, drawn from the map's data (`AscentMap`, `assets/ui/Map/AscentMap.jpg`) |
| **Wing boost** | Faster travel on Shift |
| **Soul bar** | The blue bar under health (the engine's energy value) |
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
| **Eagle Nest** | One of the nine places on the Eagle's Ascent where she is saved: standing in it, H saves (0.5: a nest no longer heals or saves by itself); she wakes in the one she saved at after a fall, and all since that save is undone (`EaglesAscentGame::RollBack`). Since 0.5.2 the only place she levels up and changes her looks (I). Its model: `Landmarks/EagleNest/EagleNest` (`art/map/eagle_nest`) |
| **Clay jar** | 0.5: one of the 3,710 Georgian clay vessels (qvevri, doki, kotso, kvibri) in groups of 3 to 5 every 30 to 50 m beside the Eagle's Ascent's roads; her blows break them; a potion (one in twelve), a crystal or two, or nothing. Since 0.5.2 the crystals in them are laid by the map's `level` lines: each stretch's share on the way and off it, all of it. Code: `Wayside` |
| **Hidden place** (on the mountain) | 0.5 to 0.5.1: one of 73 cairns with a gold light, in each long cave's heart and at the mountain's named places; standing at one found it: 8 crystals. Taken away on 2026-11-21 (the owner); a save's old `place` lines are skipped |
| **Placing model** | `art/map/ascent_creatures.py`: where the Eagle's Ascent's nests, bosses and creatures are, and (0.5.2) each level's price and what its stretch holds (`level` lines, the ledger it prints) and the groups around the way (`around`); writes `game/assets/Maps/EaglesAscent.creatures.txt` |
| **Level** (on the mountain) | 0.5.2: one of the nine levels of her wings, swords or armour, bought only in an Eagle Nest (8, 14, 21, 27, 33, 39, 46, 52, 58 crystals), each with its strength and its look. Code: `AscentGrowth` |
| **Stretch** | 0.5.2: the way between two Eagle Nests (the first from the shore to the first nest). What a stretch holds, on the way and off it, is three times the price of the next nest's level (the first one crystal more) |
| **Group around the way** | 0.5.2: a group standing 9 to 18 m off the way beside it, where nothing stood before, about every 150 to 400 m (17 of them). The game finds its ground when the map is loaded (`EaglesAscent::PlaceAround`: walkable, level, reached on foot from the way, no trunk, not in a cave, 26 m across the ground from every other group and the mouths of caves with creatures, the nearest group from where its path leaves the way, no group further up the way within 26 m of there, and 100 m from a cave's group when the two are more than three: a fight of its own); it keeps to a short path of its own out from the way |
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
| **Jars' test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=4`): jars laid and counted, her cuts at ten groups, a save, more broken, a rollback and the save read back; `ways.tsv`, `WAYS_PASS` |
| **Creatures' tour** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=3`) that stands her before every group and boss, then in every nest, and checks they wake, see her, come at her, stay beaten and save her |
| **Pointer test** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=7`) that works her window (I) and her menu (Esc) with a pointer of its own: away from a nest LEVEL UP buys nothing; in the first nest each card's LEVEL UP buys one level, worn and saved, and its marks put looks on; CLOSE, Continue, Nyxara, Esc; it checks the pointer is free only while a window is open and that a click that closes one is not a sword blow; `pointer.tsv`, `POINTER_PASS` |
| **Crystals' ledger** | An unattended run of the Eagle's Ascent (`CAUCASIAN_EAGLE_DEMO=8`): counts what each stretch holds (the way's creatures, bosses and jars; the caves' creatures, the groups around the way and the jars off it) against three times its level's price, checks 60 to 70 % of it on the way, every jar crystal laid, every group around the way placed, none closer than 140 m to the one before nor 26 m to any other group, each the nearest from where its path leaves the way, no group further up the way within 26 m of there, and none within 100 m of a cave's group when the two are more than three; `ledger.tsv`, `LEDGER_PASS` |
| **Journey** | `Journey`: the code for the road, creatures, arenas, chests, story cards and the ending |
| **Sandbox** | The engine's own executable, `Doorways.exe`, running with no game |
| **NyxaraLooks** | `game/src/NyxaraLooks`: reads a bundle's `Looks.txt` and shows a look (parts shown, textures, tints, light, wing span) through `WorldRenderer::SetCharacterMaterialLook` |
| **Asset builder** | The engine tool that compiles glTF into engine formats |
| **Hyper3D Rodin** | The service used for the looks' swords, pauldrons and crowns (2026-11-21; `art/nyxara/looks/GENERATIONS.tsv`) |
| **Meshy** | The service used to generate the hero, the mountain figures, the creatures and some scenery |
