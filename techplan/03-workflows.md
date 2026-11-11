# 6. Core workflows

[← Index](TECHNICAL_DOCUMENTATION.md)

Commands are run from the repository root in PowerShell unless stated otherwise.

## 6.1 Get the project on a new machine

```powershell
git clone --recurse-submodules https://github.com/kalk1t/Caucasian-Eagle.git
cd Caucasian-Eagle
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Run
```

Needs Visual Studio 2026 with "Desktop development with C++" and the Windows SDK 10.0.26100. `build.ps1` restores the engine's NuGet package, builds, stages and (with `-Run`) starts the game.

## 6.2 Change game code

1. Edit files under `game/src/`.
2. `tools\build.ps1 -Run`.
3. Commit in this repository.

Only the game executable recompiles; the engine library is reused.

## 6.3 Change the engine

The engine is never edited through `engine/` here.

```mermaid
sequenceDiagram
    participant E as Doorways-D3D11 repo
    participant G as Caucasian-Eagle repo
    E->>E: edit engine, build Doorways.slnx, run sandbox
    E->>E: commit + push
    G->>G: git -C engine fetch && git -C engine checkout <commit>
    G->>G: tools\build.ps1 (engine library rebuilds)
    G->>G: test the game
    G->>G: git add engine && git commit ("engine: bump to <commit>")
```

If the game needs something the engine does not offer, add a general option or callback to `engine/src/DOORWAYS/DoorwaysGame.h` in the engine repository. Do not add game names or game-specific branches to engine code ([06-engine-interface.md](06-engine-interface.md)).

## 6.4 Rebuild the hero (Nyxara)

Source files live under `art/nyxara/` on the workstation; the heavy ones are not in git ([art/README.md](../art/README.md)).

```powershell
# 1. Assemble body + wings + swords, author the 31 clips, export the glTF
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P art\nyxara\blender\assemble.py

# 2. Compile into the engine formats (delete the old bundle first)
Remove-Item -Recurse -Force game\assets\Compiled\Characters\Nyxara\EagleV1
engine\Tools\DoorwaysAssetBuilder\x64\Debug\DoorwaysAssetBuilder.exe character-pipeline `
    art\nyxara\blender\Nyxara_game.glb game\assets\Compiled\Characters\Nyxara\EagleV1 `
    Idle BasicAttack Walk Run StrafeLeft StrafeRight WalkBackward Jump JumpAirborne JumpLanding `
    SlashLeft HeavyAttack Dodge HitReact Death Respawn Skill1 Skill2 Skill3 `
    Combo1L Combo1R Combo2L Combo2R Combo3L Combo3R Guard SwordSwap `
    FlightHover FlightTravel FlightTakeoff FlightLanding

# 3. Stage and run
tools\build.ps1 -Run
```

The pipeline must print `[PASS] Native character pipeline; signature=DoorwaysRig_v1_74bb2d2053cd36e2`. A different signature means the skeleton changed and the clip list or the kit may need updating. The asset builder is an engine tool and is built from the engine repository's solution [confirmed for the engine repository checkout; building it inside the submodule has not been tried].

## 6.5 Rebuild the map (Moonlit Sanctuary alone; superseded by 6.10)

```powershell
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b art\moonlit-sanctuary\Moonlit_Sanctuary_v01.blend `
    -P art\moonlit-sanctuary\Export\export_game_glb.py -- art\moonlit-sanctuary\Export\MoonlitSanctuary_Game_v01.glb

engine\Tools\DoorwaysAssetBuilder\x64\Debug\DoorwaysAssetBuilder.exe scene-pipeline `
    art\moonlit-sanctuary\Export\MoonlitSanctuary_Game_v01.glb game\assets\Compiled MoonlitSanctuary
```

This writes the scene, static mesh, materials and collision files under `game/assets/Compiled/` [inferred from the engine-repository workflow; this exact command has not been run from the game repository yet].

## 6.6 Capture the scripted showcase

```powershell
$env:CAUCASIAN_EAGLE_DEMO = '1'
build\stage\CaucasianEagle\CaucasianEagle.exe
```

The game plays a fixed sequence of skills, combos and movement for about 30 seconds. It is the quickest visual check after a change.

To capture the mountain instead, also set `$env:CAUCASIAN_EAGLE_VISTA = '1'`: the camera is held on it and the combat effects and HUD are hidden. The run only advances while the game window is in front.

## 6.8 Rebuild the night sky

```powershell
python art\sky\make_sky_textures.py game\assets\textures\caucasus
```

Writes `NightSky.jpg` and `Moon.jpg`. Needs Python with numpy and Pillow. The result is deterministic.

## 6.9 Rebuild the mountain of Amirani

```powershell
# 1. Mountain mesh, colour map and Mountain.json
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P art\mountain\build_mountain.py -- art\mountain\export

# 2. Figures: rename mesh nodes for the importer, write 1024 px colour maps (needs art\mountain\meshy\out)
python art\mountain\prepare_models.py art\mountain\meshy\out art\mountain\export game\assets\Compiled\Landmarks\AmiraniMountain

# 3. Compile each mesh into a fresh folder, then copy the .dmesh next to the colour maps
foreach ($n in 'Mountain','Amirani','Eagle','Dog') {
    engine\Tools\DoorwaysAssetBuilder\x64\Debug\DoorwaysAssetBuilder.exe static-pipeline art\mountain\export\$n.glb build\landmark\$n
    Copy-Item build\landmark\$n\$n.dmesh game\assets\Compiled\Landmarks\AmiraniMountain\
}
Copy-Item art\mountain\export\MountainBaseColor.png game\assets\Compiled\Landmarks\AmiraniMountain\
```

`static-pipeline` refuses an output folder that already exists; delete `build\landmark` first. After step 1, copy `ledgeY`, `ledgeHalfWidth` and `ledgeBackZ` from `Mountain.json` into `GameConstants.h` if they changed. The importer only accepts render nodes whose names start with `Doorways_`.

## 6.10 Rebuild the world (sanctuary plus the road), creatures and props

```powershell
# 1. World: geometry, collision, textures, Road.txt  (about 2 minutes)
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b art\moonlit-sanctuary\Moonlit_Sanctuary_v01.blend `
    -P art\world\build_world.py -- art\world\export\RoadToAmirani.glb art\world\export\Road.txt art\world\export\textures

# 2. Delete the old compiled world, then compile
foreach ($d in 'StaticMeshes','Materials','Collision','Textures') { Remove-Item -Recurse -Force game\assets\Compiled\$d\RoadToAmirani -ErrorAction SilentlyContinue }
Remove-Item game\assets\Compiled\Scenes\RoadToAmirani.dscene -ErrorAction SilentlyContinue
<asset builder> scene-pipeline art\world\export\RoadToAmirani.glb game\assets\Compiled RoadToAmirani
Copy-Item art\world\export\Road.txt game\assets\Compiled\World\Road.txt

# 3. Creatures and props (only when a Meshy model changes; needs the meshy\out folders)
python art\monsters\prepare_models.py art\monsters\meshy\out game\assets\Compiled\Monsters <asset builder> Chinka=chinka Ali=ali ...
```

`<asset builder>` is `DoorwaysAssetBuilder.exe` from the engine repository checkout (`Tools\DoorwaysAssetBuilder\x64\Debug`). The world script is deterministic (fixed random seeds). Everything that shapes the road is in the tables at the top of `build_world.py`: the twelve regions (`STAGES`), the waypoints (`W`), the arenas (`ARENAS`) and the sizes. The game reads positions from `Road.txt`, so the code does not change when the road does, except `MountainZ` and the `LedgeWalk*` constants if the mountain moves.

## 6.11 Test the journey without playing it (test pilot)

```powershell
$env:CAUCASIAN_EAGLE_DEMO = '1'; $env:CAUCASIAN_EAGLE_PILOT = '1'
$env:CAUCASIAN_EAGLE_SPEED = '8'     # optional: simulation speed, 1 to 12
$env:CAUCASIAN_EAGLE_START = '24'    # optional: start as if this many arenas were already won (34 or more: start at the chain, after the Eagle)
$env:CAUCASIAN_EAGLE_MORTAL = '1'    # optional: the pilot can die; it then asks the engine for a respawn (tests death and the return to the last arena)
build\stage\CaucasianEagle\CaucasianEagle.exe
```

The game plays itself: it leaves the sanctuary, follows the road, fights every wave, breaks every chest in reach, fights the bosses and the Eagle, breaks the chain. Unless `CAUCASIAN_EAGLE_MORTAL` is set the pilot cannot die (its health is refilled every frame), so it tests the journey's logic and gives a lower bound for its length; it says nothing about difficulty. It never reads or writes `journey.sav` and never sends input to the desktop. Progress goes to the debugger output as `[JOURNEY]` and `[PILOT]` lines (view with Sysinternals DebugView or a debugger); the run is finished when `[JOURNEY] COMPLETE` appears.

## 6.7 Release a version

See [09-build-package-release.md](09-build-package-release.md).
