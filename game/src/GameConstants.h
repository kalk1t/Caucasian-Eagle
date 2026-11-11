#pragma once

// Caucasian Eagle: game-wide names and tuning constants.
// Every value here is listed in techplan/11-constants.md; keep the two in step.

namespace CaucasianEagle
{
    // ---- identity
    constexpr const wchar_t* GameTitle = L"Caucasian Eagle";
    constexpr const char* GameVersion = "0.1.0";
    constexpr const wchar_t* UserDataFolder = L"CaucasianEagle"; // %LOCALAPPDATA%\CaucasianEagle: settings and saves

    // ---- content
    constexpr const char* StartSceneId = "MoonlitSanctuary";                // assets/Compiled/Scenes/<id>.dscene
    constexpr const char* HeroBundle = "Characters/Nyxara/EagleV1";         // under assets/Compiled
    constexpr const char* DemoEnvironmentVariable = "CAUCASIAN_EAGLE_DEMO"; // =1: scripted showcase run
    constexpr const char* VistaEnvironmentVariable = "CAUCASIAN_EAGLE_VISTA"; // =1 with the showcase: camera held on the mountain

    // ---- movement
    constexpr float WalkSpeed = 1.38f;          // m/s, Ctrl walk (authored Walk treadmill speed)
    constexpr float RunClipSpeed = 4.6f;        // m/s, authored Run / Strafe treadmill speed
    constexpr float WalkBackClipSpeed = 1.33f;  // m/s, authored WalkBackward treadmill speed
    constexpr float WingBoostMultiplier = 2.2f; // Shift travel speed multiplier

    // ---- camera (orbit rig looking down on Nyxara)
    constexpr float CameraPitch = -0.60f;      // rad, about 34 degrees down
    constexpr float CameraDistance = 6.5f;     // m from the chest pivot
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
    constexpr float MountainX = 0.f, MountainY = 0.f, MountainZ = 330.f; // m, peak centre at play-floor height
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
