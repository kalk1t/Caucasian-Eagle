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
| **Shade** | The dark wraith enemy of v0.1 |

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

## Places and assets

| Name | Meaning |
|---|---|
| **Moonlit Sanctuary** | The v0.1 map; scene id `MoonlitSanctuary` |
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
| **Sandbox** | The engine's own executable, `Doorways.exe`, running with no game |
| **Asset builder** | The engine tool that compiles glTF into engine formats |
| **Meshy** | The service used to generate the hero's meshes, rig and base animations |
