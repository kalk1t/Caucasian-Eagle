#pragma once

// Caucasian Eagle: game-wide names and tuning constants.
// Every value here is listed in techplan/11-constants.md; keep the two in step.

namespace CaucasianEagle
{
    // ---- identity
    constexpr const wchar_t* GameTitle = L"Caucasian Eagle";
    constexpr const char* GameVersion = "0.2.0";
    constexpr const wchar_t* UserDataFolder = L"CaucasianEagle"; // %LOCALAPPDATA%\CaucasianEagle: settings and saves

    // ---- content
    constexpr const char* StartSceneId = "RoadToAmirani";                   // assets/Compiled/Scenes/<id>.dscene
    constexpr const char* HeroBundle = "Characters/Nyxara/EagleV1";         // under assets/Compiled
    constexpr const char* DemoEnvironmentVariable = "CAUCASIAN_EAGLE_DEMO"; // =1: scripted showcase run
    constexpr const char* VistaEnvironmentVariable = "CAUCASIAN_EAGLE_VISTA"; // =1 with the showcase: camera held on the mountain

    // ---- journey: the road to Amirani (Journey.cpp; world built by art/world/build_world.py)
    constexpr const char* RoadFile = "assets/Compiled/World/Road.txt";   // path, arenas, chests, props, lights
    constexpr const char* SaveFileName = "journey.sav";                   // in the user data folder
    constexpr const char* PilotEnvironmentVariable = "CAUCASIAN_EAGLE_PILOT"; // =1 with the showcase: a test pilot plays the whole road
    constexpr const char* SpeedEnvironmentVariable = "CAUCASIAN_EAGLE_SPEED"; // test pilot only: simulation speed 1..12
    constexpr const char* MortalEnvironmentVariable = "CAUCASIAN_EAGLE_MORTAL"; // =1: the test pilot can die and asks for a respawn (tests death)
    constexpr const char* StartEnvironmentVariable = "CAUCASIAN_EAGLE_START"; // start after this many arenas (testing; ignores the save)
    constexpr float SanctuaryRadius = 97.f;      // m, inside it the road rules do not apply
    constexpr float LedgeWalkFrontZ = 1872.f;    // m, front edge of the walkable ledge under Amirani (MountainZ - 28)
    constexpr float LedgeWalkBackZ = 1889.f;     // m, back edge of the walkable ledge
    constexpr float LedgeWalkHalfWidth = 38.f;   // m
    constexpr float MonsterHealthScale = 1.8f;   // health of every ordinary creature
    constexpr float BossHealthScale = 1.0f;      // health of the three region bosses and the Eagle
    constexpr float MonsterDamageScale = 0.6f;   // every monster's damage
    constexpr int LateWaveStage = 3;             // from this region on an arena has 4 waves instead of 3
    constexpr float PatrolBudget = 0.55f;        // a roaming pack costs this share of an arena's first wave
    constexpr float PatrolAhead = 26.f;          // m, a roaming pack appears this far ahead of her
    constexpr float PatrolGiveUp = 70.f;         // m, a pack left this far behind gives up
    constexpr float StageHealthGrowth = 0.12f;   // + per stage (12 stages)
    constexpr float StageDamageGrowth = 0.03f;   // + per stage
    constexpr float FeatherDamageBonus = 0.03f;  // Nyxara's damage per Eagle Feather
    constexpr float EmberSeconds = 20.f;         // Soul Ember duration
    constexpr float EmberDamage = 1.5f;          // Nyxara's damage multiplier while it burns
    constexpr float MercySeconds = 1.0f;         // after a hit she cannot be hurt again for this long (a crowd cannot kill her in one moment)
    constexpr int MaxAttackers = 2;              // ordinary creatures that may wind up a blow at the same time
    constexpr float RegenPerSecond = 3.f;        // health per second outside fights
    constexpr int KillHealBase = 4;              // health she takes back from every creature she fells...
    constexpr int KillHealPerCost = 3;           // ...plus this for each point of its cost (a Devi gives 34)
    constexpr int ArenaHeal = 30;                // health for winning an arena
    constexpr int WaveHeal = 20;                 // health when a wave is beaten and another comes
    constexpr int MoonTearHeal = 40;             // health from a Moon Tear
    constexpr float WaveDelay = 1.4f;            // s between waves
    constexpr float SpawnInterval = 0.3f;        // s between monsters of one wave
    constexpr float ChestWakeDistance = 30.f;    // m, a chest becomes breakable inside this distance
    constexpr float PropDrawDistance = 260.f;    // m, trees and rocks
    constexpr float TowerDrawDistance = 600.f;   // m, towers
    constexpr float LightDrawDistance = 140.f;   // m, lantern and crystal glows
    constexpr float PropTint = 0.55f;            // night tint of scenery props
    constexpr float MonsterTint = 0.85f;         // monsters read against the night
    constexpr float MonsterEmission = 0.10f;

    // ---- movement
    constexpr float WalkSpeed = 1.38f;          // m/s, Ctrl walk (authored Walk treadmill speed)
    constexpr float RunClipSpeed = 4.6f;        // m/s, authored Run / Strafe treadmill speed
    constexpr float WalkBackClipSpeed = 1.33f;  // m/s, authored WalkBackward treadmill speed
    constexpr float WingBoostMultiplier = 2.2f; // Shift travel speed multiplier

    // ---- camera (orbit rig looking down on Nyxara)
    constexpr float CameraPitch = -0.34f;      // rad, about 19 degrees down: the road and the mountain stay in view
    constexpr float CameraDistance = 7.4f;     // m from the chest pivot
    constexpr float CameraPivotHeight = 1.35f; // m above her feet
    constexpr float CameraMinPitch = -1.2f;    // rad, steepest look-down
    constexpr float CameraMaxPitch = 0.45f;    // rad, highest look-up
    constexpr float VistaPitch = 0.24f;        // rad, capture mode (CAUCASIAN_EAGLE_VISTA=1): look up at the mountain

    // ---- night sky (art/sky/make_sky_textures.py; drawn by AmiraniMountain, paths relative to the game folder)
    constexpr const char* SkyTexture = "assets/textures/caucasus/NightSky.jpg";
    constexpr const char* MoonTexture = "assets/textures/caucasus/Moon.jpg";
    constexpr const char* MoonGlowTexture = "assets/Compiled/Effects/Nyxara/Glow.png";
    constexpr float SkyRadius = 3000.f;     // m, dome radius around the camera (engine far plane is 5400)
    constexpr float SkyTextureWraps = 2.f;  // times the sky texture repeats around the horizon
    constexpr float SkyBrightness = 0.46f;  // dome tint (the engine's ambient light multiplies it)
    constexpr float SkyEmission = 1.0f;
    constexpr float MoonAzimuth = -0.46f;   // rad from +Z toward +X: left of the mountain
    constexpr float MoonElevation = 0.47f;  // rad above the horizon
    constexpr float MoonAngularSize = 0.125f; // rad, about 7 degrees: far larger than the real moon
    constexpr float MoonFaceYaw = 2.2f;     // rad, which side of the moon texture faces the viewer
    constexpr float MoonBrightness = 1.05f; // moon tint (unlit like the dome)
    constexpr float MoonEmission = 1.0f;
    constexpr float MoonHaloSize = 4.2f;    // halo width in moon diameters
    constexpr float MoonHaloR = 0.50f, MoonHaloG = 0.95f, MoonHaloB = 0.85f, MoonHaloAlpha = 0.42f;
    constexpr float MoonlightX = 0.72f, MoonlightY = -0.56f, MoonlightZ = 0.40f; // direction the moonlight travels
    constexpr float HazeStart = 40.f;       // m, distance haze begins
    constexpr float HazeEnd = 900.f;        // m, distance haze is complete
    constexpr float HazeStrength = 0.62f;

    // ---- mountain of Amirani (art/mountain; scenery only, no collision)
    constexpr const char* LandmarkFolder = "assets/Compiled/Landmarks/AmiraniMountain";
    constexpr float MountainX = 0.f, MountainY = 0.f, MountainZ = 1900.f; // m, peak centre at play-floor height
    constexpr float MountainFarDim = 0.45f;     // how much the mountain darkens when far away (it takes no fog)
    constexpr float MountainDimStart = 150.f;   // m, distance where the dimming begins
    constexpr float MountainDimEnd = 1700.f;    // m, distance where it is complete
    constexpr float LedgeHeight = 78.f;     // m, cave ledge above the play floor (Mountain.json ledgeY)
    constexpr float LedgeHalfWidth = 40.f;  // m (Mountain.json ledgeHalfWidth)
    constexpr float LedgeBackZ = 8.f;       // m, cave back wall relative to the peak centre (Mountain.json ledgeBackZ)
    constexpr float MountainTint = 0.42f;   // darkens the rock under the engine's strong moonlight
    constexpr float MountainEmission = 0.04f;
    constexpr float FigureEmission = 0.22f; // self-light so the figures read at night from the play area
    constexpr float EagleEmission = 0.40f;  // the eagle and the dog are dark; they need more
    constexpr float AmiraniHeight = 50.f;   // m: a titan, visible from the whole play area
    constexpr float AmiraniFromWall = 10.f; // m in front of the cave back wall
    constexpr float AmiraniYaw = 3.14159265f; // rad, faces the play area
    constexpr float WristOut = 0.42f, WristUp = 0.90f; // wrist position as fractions of his height
    constexpr float AnkleOut = 0.13f;
    constexpr float EagleHeight = 30.f;     // m
    constexpr float EagleYaw = 1.0f;       // rad, turned toward the wound
    constexpr float EaglePitch = 0.10f;     // rad
    constexpr float EagleSide = -21.f;      // m, at his right side (viewer's left)
    constexpr float EagleFromBody = 12.f;   // m in front of him
    constexpr float EagleHeightOnBody = 0.40f; // eagle feet height as a fraction of his height
    constexpr float EaglePeckPeriod = 3.6f; // s between lunges at the wound
    constexpr float DogHeight = 13.f;       // m at the shoulder
    constexpr float DogYaw = 1.2f;          // rad
    constexpr float DogSide = 25.f;         // m, at his left foot
    constexpr float DogFromBody = 14.f;      // m in front of him
    constexpr float ChainLinkLength = 2.6f; // m
    constexpr float ChainLinkWidth = 1.5f;  // m
}
