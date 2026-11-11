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
| **The Road to Amirani** | The journey: one road of twelve regions from Moonlit Sanctuary to the mountain |
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

## Hero and combat

| Name | Meaning |
|---|---|
| **Moonfeather** | The silver sword (longer, 1.02 m); starts in the right hand |
| **Talonshade** | The dark-green sword (0.92 m); starts in the left hand |
| **Left Talon / Right Talon** | The two alternating sword slashes on left click |
| **Charged Slash** | The right-click heavy attack |
| **Talon Dash, Feather Cyclone, Moon Leap** | The three skills (F, V, C) |
| **Twin Talons, Rising Eagle, Tempest Triple, Whirlwind Talons, Soul Weave, Moonfall Dive** | The six combos |
| **Sword Barrier** | The E guard: two spectral swords that block orbs |
| **Sword Exchange** | The Q swap of the swords between hands |
| **Wing boost** | Faster travel on Shift |
| **Soul bar** | The blue bar under health (the engine's energy value) |
| **Hit-stop** | The brief freeze when a blow lands |
| **Arena** | A round clearing on the road where creatures ambush her; the way on stays shut until they are beaten |
| **Wave** | One group of creatures in an arena fight; two or three per arena |
| **Old gold chest** | A breakable chest; holds coins and a memory, a Moon Tear, an Eagle Feather or a Soul Ember |
| **Moon Tear** | Chest reward: restores health |
| **Eagle Feather** | Chest reward: each one raises her damage by 3% for the rest of the journey |
| **Soul Ember** | Chest reward: resets skill cooldowns and raises her damage by half for 20 seconds |
| **Windup** | The moment before a creature's blow lands, shown by a bright ring; the time to step away |

## Places and assets

| Name | Meaning |
|---|---|
| **Moonlit Sanctuary** | The walled garden where the road begins; the v0.1 map |
| **RoadToAmirani** | Scene id of the world: the sanctuary plus the whole road |
| **Road.txt** | Data file written by the world builder: the path, arenas, chests, props and lights |
| **Mountain of Amirani** | The landmark at the end of the road: peak, cave ledge, Amirani, the eagle, the dog, the chains. Code: `AmiraniMountain` |
| **Ledge** | The flat shelf of the cave where Amirani stands, 78 m above the sanctuary floor; reachable only after the Eagle falls |
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
| **Showcase** | The scripted 30-second demo run (`CAUCASIAN_EAGLE_DEMO=1`) |
| **Test pilot** | An unattended run in which the game plays the whole road by itself (`CAUCASIAN_EAGLE_PILOT=1`); used to test the journey |
| **Journey** | `Journey`: the code for the road, creatures, arenas, chests, story cards and the ending |
| **Sandbox** | The engine's own executable, `Doorways.exe`, running with no game |
| **Asset builder** | The engine tool that compiles glTF into engine formats |
| **Meshy** | The service used to generate the hero, the mountain figures, the creatures and some scenery |
