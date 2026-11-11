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
}
