#pragma once

// Caucasian Eagle: game-wide names and tuning constants.
// Every value here is listed in techplan/11-constants.md; keep the two in step.

namespace CaucasianEagle
{
    // ---- identity
    constexpr const wchar_t* GameTitle = L"Caucasian Eagle";
    constexpr const char* StartScreenPicture = "assets/ui/Menu/StartScreen.png";   // behind the start screen; painted by art/ui/build_start_screen.py
    // ---- what she collects (Collection.cpp)
    constexpr int CharmSet = 3;                 // charms of one creature kind that make a set
    constexpr float CharmChance = .3f;          // that a felled creature drops its charm (until the set is complete)
    constexpr float CharmSetDamage = .15f;      // a set: her blows against that kind, more by this share
    constexpr float CharmSetTaken = .15f;       // a set: that kind's blows against her, less by this share
    constexpr float KeepsakeDamage = .02f;      // each city keepsake: all her blows, more by this share
    constexpr float KeepsakeTaken = .02f;       // each city keepsake: all blows against her, less by this share
    constexpr float FatherFeatherBoost = .025f; // each feather of her father: wing-boost speed, more by this share
    constexpr float FatherFeatherReach = 2.2f;  // m, she picks a feather up this close
    constexpr float FatherFeatherSight = 110.f; // m, a feather's light is drawn this far
    constexpr float SongCrystals = 1.25f;       // seven verses: crystals from creatures, times this
    constexpr int MaxRunes = 3;                 // of one kind
    constexpr float RuneChance = .35f;          // that an old gold chest holds a rune (a king's hoard always does)
    constexpr float FireRuneDamage = .06f;      // per fire rune set in her blades: all her blows, more by this share
    constexpr float FrostRunePace = .08f;       // per frost rune: creatures wait this share longer between attacks
    constexpr int MoonRuneHeal = 2;             // per moon rune: health for every creature felled

    // ---- sound (Sound.cpp; the files are in game/assets/sounds)
    constexpr const char* SoundFolder = "assets/sounds";
    constexpr float MusicVolume = .5f;        // of the Music setting
    constexpr float MusicFadeSeconds = 2.5f;  // one piece into the next
    constexpr float WindVolume = .45f;
    constexpr float PlaceVolume = .6f;        // city voices, or the fire of the walls
    constexpr float FlightVolume = .5f;
    constexpr float NarrationVolume = 1.f;
    constexpr float NarrationDuck = .4f;      // everything else, while the storyteller speaks
    constexpr float StepLength = 1.7f;        // metres on the ground between two footsteps
    constexpr float StepVolume = .4f;
    constexpr float SoundRepeatSeconds = .07f; // the same sound is not started again sooner than this
    constexpr const char* GameVersion = "0.3.0";
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
    constexpr const char* JournalEnvironmentVariable = "CAUCASIAN_EAGLE_JOURNAL"; // testing: hold the story journal open on page 1..4 (11..14: everything unlocked)
    constexpr const char* StartEnvironmentVariable = "CAUCASIAN_EAGLE_START"; // start after this many arenas (testing; ignores the save)
    // ---- the timing of her attacks (NyxaraKit). "Clip seconds" are seconds of the animation, before its rate.
    constexpr float ComboWindowSeconds = 1.2f;   // s after a skill ends in which its combo can still be started
    constexpr float ComboBufferSeconds = .6f;    // s a mouse press during a skill is remembered; the combo starts when the skill's last hit has landed
    constexpr float SkillBufferSeconds = .45f;   // s a skill key pressed too early is remembered
    constexpr float RecoveryCancel = .22f;       // clip s after the last hit of a skill or combo: from here a slash, the heavy blow, the barrier or the swap cuts the recovery
    constexpr float SlashChainAfterHit = .12f;   // clip s after a slash lands: the next buffered slash or heavy blow starts
    constexpr float HeavyChainAfterHit = .30f;   // clip s after the heavy blow lands: a buffered slash starts
    constexpr float SlashChainSeconds = 1.5f;    // s after a slash in which the next click swings the other sword
    constexpr float GuardCooldownSeconds = .6f;  // s after the barrier drops before it can rise again
    constexpr float WallReach = 12.f;            // m beyond the road's edge: within it the wall of an unwon arena stops her; farther out she walks round it
    constexpr float CityReach = 25.f;            // m beyond the road's edge: within it she is "in" a city (its name, healing, gift)
    constexpr float WayOffRoad = 2.f;            // m beyond the road's edge from which the way back is shown
    constexpr float WayStep = 30.f;              // m along the road ahead of her nearest point: where the way marker leads
    constexpr float WayTrail = 30.f;             // m, length of the trail of lights on the ground
    constexpr float WayFadeSeconds = .4f;

    // ---- trouble on the road between the fights (Journey::BuildTroubles): cursed ground to walk round, falling
    //      stones to keep moving under, wisps to slip between. Their harm goes through the same scale as a creature's.
    constexpr const char* TroubleEnvironmentVariable = "CAUCASIAN_EAGLE_TROUBLE";   // testing, with the pilot: =N stands her before trouble N; =-N beside it, 30 m off the road
    constexpr float TroubleFirst = 150.f;        // m along the road: none before it
    constexpr float TroubleLastBefore = 190.f;   // m before the end of the road: none after it
    constexpr float TroubleEvery = 85.f;         // m between two, give or take a quarter
    constexpr float TroubleClear = 20.f;         // m kept free of them round every arena and city
    constexpr float TroubleWarn = 14.f;          // m before one at which the line under the compass names it
    constexpr float MireDamage = 7.f;            // each time it bites (her mercy time sets how often)
    constexpr float RockfallReach = 16.f;        // m round its middle in which stones fall on her
    constexpr float RockfallEvery = 1.5f;        // s between stones
    constexpr float RockfallWarn = 1.1f;         // s the ring shows before the stone lands
    constexpr float RockfallSpread = 2.2f;       // m round her at which a stone is aimed
    constexpr float RockfallRadius = 2.3f;       // m round the mark that it hurts
    constexpr float RockfallDamage = 12.f;
    constexpr int WispCount = 3;
    constexpr float WispGap = 4.5f;              // m between two along the road
    constexpr float WispSpeed = 1.15f;           // radians a second of their swing across the road
    constexpr float WispTouch = 1.15f;           // m
    constexpr float WispDamage = 10.f;
    constexpr float RoamBody = .45f;             // m, her radius against trees, boulders and gate piers
    constexpr float TrunkRadius = .045f;         // of a tree's width: its trunk, which she walks round
    constexpr float StartS = 2.f;                // m along the road where a new journey begins (and where she returns before the first arena is won)
    constexpr float LedgeWalkFrontZ = 1872.f;    // m, front edge of the walkable ledge under Amirani (MountainZ - 28)
    constexpr float LedgeWalkBackZ = 1889.f;     // m, back edge of the walkable ledge
    constexpr float LedgeWalkHalfWidth = 38.f;   // m
    constexpr float MonsterHealthScale = 1.8f;   // health of every ordinary creature
    constexpr float BossHealthScale = 1.0f;      // health of the three region bosses and the Eagle
    constexpr float MonsterDamageScale = 0.85f;  // every monster's damage
    constexpr float MonsterAttackPace = 0.8f;    // every ordinary creature's time between blows, times this (lower = they strike more often)
    constexpr float MonsterSpeedScale = 1.2f;    // every ordinary creature's speed
    constexpr float MonsterSprintDistance = 14.f; // m, farther from her than this a creature sprints to catch up
    constexpr float MonsterSprint = 1.45f;       // ...at this multiple of its speed
    constexpr float MonsterTurnRate = 9.f;       // rad/s a creature turns to face her
    constexpr float MonsterLeadSeconds = 0.45f;  // creatures run to where she will be this far ahead, not to where she is
    constexpr float MonsterFlank = 0.65f;        // how strongly melee creatures swing out to the sides as they close in (0 = straight at her)
    constexpr float MonsterFlankDistance = 7.f;  // m beyond striking distance where the swing begins
    constexpr float MonsterCircleSpeed = 0.55f;  // share of its speed a creature circles her with between its blows
    constexpr int LateWaveStage = 3;             // from this region on an arena has 4 waves instead of 3
    constexpr float PatrolBudget = 0.55f;        // a roaming pack costs this share of an arena's first wave
    constexpr float PatrolAhead = 26.f;          // m, a roaming pack appears this far ahead of her
    constexpr float PatrolGiveUp = 70.f;         // m, a pack left this far behind gives up
    constexpr float StageHealthGrowth = 0.20f;   // + per stage (12 stages): creatures of the last stage have 3.2 times the health
    constexpr float StageDamageGrowth = 0.07f;   // + per stage: creatures of the last stage hit 1.77 times as hard
    constexpr float FeatherDamageBonus = 0.03f;  // Nyxara's damage per Eagle Feather
    constexpr float EmberSeconds = 20.f;         // Soul Ember duration
    constexpr float EmberDamage = 1.5f;          // Nyxara's damage multiplier while it burns
    constexpr float MercySeconds = 1.0f;         // after a hit she cannot be hurt again for this long (a crowd cannot kill her in one moment)
    constexpr int MaxAttackers = 3;              // ordinary creatures that may wind up a blow at the same time
    constexpr int KillHeal = 10;                 // health she takes back from every creature she fells
    constexpr float RegenSeconds = 2.f;          // while she lives her health comes back by itself: RegenHealth every this many seconds
    constexpr int RegenHealth = 1;

    // ---- how ordinary creatures fight (Journey.cpp UpdateMonsters)
    constexpr float HeavyBlowChance = 0.28f;     // share of a creature's blows that are heavy: slower, wider, harder
    constexpr float HeavyBlowWindup = 1.7f;      // a heavy blow's wind-up, times the light one
    constexpr float HeavyBlowDamage = 1.6f;      // its damage, times the light one
    constexpr float HeavyBlowReach = 1.6f;       // m beyond the creature's reach that a heavy blow lands (a light one: 0.9)
    constexpr float FollowUpChance = 0.35f;      // quick creatures: a second, faster blow straight after a light one
    constexpr float FollowUpWindup = 0.55f;      // its wind-up, times the light one
    constexpr float QuickWindup = 0.56f;         // s; creatures whose wind-up is shorter than this are 'quick'
    constexpr float SlamRange = 12.f;            // m, giants throw the ground at her from 5 m to this far
    constexpr float SlamSeconds = 1.1f;          // from the mark on the ground to the blow
    constexpr float SlamRadius = 3.2f;           // m, the marked circle
    constexpr float SlamDamage = 1.3f;           // times the giant's blow
    constexpr float SlamEvery = 7.f;             // s between a giant's slams, about
    constexpr float DodgeDistance = 4.5f;        // m, small creatures this near may jump aside when she attacks facing them
    constexpr float DodgeChance = 0.5f;
    constexpr float DodgeEvery = 2.5f;           // s before the same creature may try again, about
    constexpr float HopSeconds = 0.28f;          // length of a jump aside or back
    constexpr float HopSpeed = 3.f;              // times the creature's speed
    constexpr float RoundGuardSeconds = 2.5f;    // a creature whose blow she blocked runs round to her back for this long
    constexpr float WoundedShare = 0.3f;         // below this share of health a small creature fights hit-and-run
    constexpr float WoundedKeep = 6.5f;          // m it keeps away between its blows
    constexpr float RangedLead = 0.7f;           // 0 = shots aimed at where she is, 1 = fully at where she will be
    constexpr int FanEvery = 3;                  // spirits and witches: every this many volleys is a fan of three
    constexpr int KajiNovaOrbs = 8;              // Kaji: every third shot is a ring of fire round itself
    // ---- growth (Growth.h): crystals, evolutions, training, potions
    constexpr int BossCrystals = 60;             // crystals from a guardian; an ordinary creature leaves as many as its cost (1..10)
    constexpr int ChestCrystals = 5;             // crystals in every chest
    constexpr int SaveCrystalsPerArena = 20;     // a save from before crystals existed is given this many per arena already won
    constexpr int MaxItemTier = 3;               // wings, swords and armour evolve this many times
    constexpr int ItemTierCost[MaxItemTier] = {40, 140, 320};   // crystals for the first, second and third evolution of one item
    constexpr int MaxTrainingLevel = 8;          // power and defence
    constexpr int TrainingCostBase = 10;         // crystals for the first level...
    constexpr int TrainingCostStep = 10;         // ...and this many more for each level after it
    constexpr float PowerStep = 0.07f;           // more damage per level of power
    constexpr float DefenceStep = 0.05f;         // less damage taken per level of defence
    constexpr float SwordTierDamage = 0.20f;     // more damage per evolution of the swords
    constexpr float ArmourTierTaken = 0.12f;     // less damage taken per evolution of the armour
    constexpr float WingTierCooldown = 0.12f;    // skills return this much sooner per evolution of the wings
    constexpr float WingTierBoost = 0.08f;       // wing boost this much faster per evolution of the wings
    constexpr float FormDamage = 0.10f;          // each form of Nyxara herself: more damage...
    constexpr float FormTaken = 0.08f;           // ...less damage taken...
    constexpr float FormScale = 0.03f;           // ...and she stands this much taller
    constexpr int MaxPotions = 5;                // potions she can carry
    constexpr int PotionHeal = 35;               // health from one potion
    constexpr int PotionKey = 'Z';               // drinks one
    constexpr float ArenaCageMargin = 20.f;      // m, the walls of a fight stand this far beyond the arena at each end
    constexpr float WallStandOff = 1.4f;         // m between where she is stopped and the middle of the wall
    constexpr float WallSightNear = 1.3f;        // m, a wall block this close to the line from the camera to her sinks out of the way...
    constexpr float WallCameraNear = 3.2f;       // m, a wall block this close to the camera itself is sunk...
    constexpr float WallCameraFar = 6.5f;        // ...and one this far stands at full height
    constexpr float WallSightFar = 3.2f;         // ...and one this far stands at full height
    constexpr const char* GrowthEnvironmentVariable = "CAUCASIAN_EAGLE_GROWTH";   // testing: =1 holds Nyxara's window open with crystals to spend; =2 its second page
    constexpr int ArenaHeal = 30;                // health for winning an arena
    constexpr int WaveHeal = 20;                 // health when a wave is beaten and another comes
    constexpr int MoonShardCrystals = 15;        // crystals from a Moon Shard (chests no longer heal)
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
    constexpr float CameraLookScale = 0.7f;    // how fast the mouse and the right stick turn the camera, against the engine's
                                               // own speed (1). The player's "Mouse speed" setting multiplies this.
    constexpr const char* PointerFile = "assets/ui/Cursor/Pointer.cur";   // the mouse pointer; drawn by art/ui/build_cursor.py
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

    // ---- story journal and notices (Journey.cpp, Story.cpp)
    constexpr const char* RestartEnvironmentVariable = "CAUCASIAN_EAGLE_RESTART"; // testing: an unattended run holds F9 from this second
    constexpr float RestartHoldSeconds = 1.5f;   // F9 held this long begins the journey again
    constexpr float JournalTimeScale = .001f;    // simulation speed while the journal is open (the road stands still)
    constexpr float JournalFadeSeconds = .18f;   // journal fade in / out
    constexpr float JournalScrollSpeed = 520;    // layout units per second while W / S is held
    constexpr int PauseRows = 8;                 // her own menu (Esc): continue, Nyxara, story, map, settings, guide, start screen, leave
    constexpr const char* RoamEnvironmentVariable = "CAUCASIAN_EAGLE_ROAM";     // testing: =N makes the pilot weave up to N m off the road, over the land
    constexpr const char* PauseEnvironmentVariable = "CAUCASIAN_EAGLE_PAUSE";   // testing: =1 holds her menu open
    constexpr const char* GuideEnvironmentVariable = "CAUCASIAN_EAGLE_GUIDE";   // testing: =N holds the guide open on page N
    constexpr const char* SettingsEnvironmentVariable = "CAUCASIAN_EAGLE_SETTINGS";          // testing: =1 holds the settings window open
    constexpr const char* SettingsKeysEnvironmentVariable = "CAUCASIAN_EAGLE_SETTINGS_KEYS"; // testing: keys played into it (W S A D, E Enter, X Esc)
    constexpr int JournalKeyCount = 15;          // F1, A, Left, D, Right, W, Up, S, Down, PageUp, PageDown, M, I, Enter, Space
    constexpr int MaxToasts = 4;                 // chest notices shown at once at the right edge

    // ---- arena walls (Journey.cpp): stone that rises across the road at both ends of an arena while its fight lasts
    constexpr float WallHeight = 5.2f;           // m, tallest blocks
    constexpr float WallBlockWidth = 1.7f;       // m along the wall
    constexpr float WallBlockDepth = 1.5f;       // m along the road
    constexpr float WallRiseSeconds = 0.9f;      // from the ground to full height
    constexpr float WallSinkSeconds = 1.6f;      // back into the ground when the fight is won

    // ---- the seven cities (art/cities/build_cities.py; words in Story.cpp; Journey.cpp UpdateCities) and the maps (WorldMap.cpp)
    constexpr const char* CitiesFile = "assets/Compiled/World/Cities.txt";  // cities, their models, buildings and lights
    constexpr const char* MapFile = "assets/Compiled/World/Map.txt";        // the part of the world the map picture shows
    constexpr const char* MapTexture = "assets/ui/Map/WorldMap.png";        // the painted world map (art/map/build_map.py)
    constexpr const char* MapEnvironmentVariable = "CAUCASIAN_EAGLE_MAP";   // testing: =1 holds the world map open
    constexpr float CityDrawDistance = 450.f;    // m, buildings are drawn this far
    constexpr float CityTint = 0.78f;            // darkens the buildings under the engine's strong moonlight
    constexpr float CityEmission = 0.16f;        // self-light, so lamp-lit windows read at night
    constexpr float MinimapRange = 110.f;        // m from Nyxara to the rim of the minimap
    constexpr int WineKillHeal = 5;              // Telavi: more health from every creature she fells
    constexpr float SpringsHealScale = 2.f;      // Tbilisi: healing for a wave or an arena won, times this
    constexpr int BlessingPotionHeal = 15;       // Mtskheta: more health from every potion
    constexpr float StoneDamageScale = 0.8f;     // Gori: damage she takes, times this
    constexpr int FleeceGoldScale = 2;           // Kutaisi: gold in a chest, times this
    constexpr int SeaWindFeathers = 3;           // Batumi: eagle feathers given
    constexpr float SvanSteelDamage = 1.25f;     // Mestia: her damage, times this

    // ---- nature (Nature.cpp): grass, trees and clouds. What grows in each region is the table at the top of Nature.cpp.
    constexpr const char* NatureTextureFolder = "assets/textures/nature";   // Grass, Leaves, Bark, Clouds (art/world/build_nature.py)
    constexpr float NatureTint = 0.50f;          // darkens grass and trees under the engine's strong moonlight
    constexpr float NatureMaxSlope = 0.85f;      // rise over run; steeper land is bare rock
    constexpr float MountainClearZ = 1790.f;     // beyond this, near the middle line, Amirani's mountain stands
    constexpr float GrassCell = 20.f;            // m, side of one square of grass (one mesh)
    constexpr float GrassDistance = 80.f;        // m, grass is drawn this far from the camera
    constexpr float GrassFromRoad = 0.3f;        // m beyond the road's edge where grass begins
    constexpr int GrassCellsPerFrame = 2;        // new squares built in one frame
    constexpr int GrassCellLimit = 160;          // squares kept before old ones are let go
    constexpr float TreeCell = 15.f;             // m, one tree at most in each square of this side
    constexpr float TreeDistance = 240.f;        // m, trees are drawn this far
    constexpr float TreeFromRoad = 6.f;          // m beyond the road's edge: nearest trees
    constexpr float TreeToRoad = 150.f;          // m beyond the road's edge: farthest trees
    constexpr float BroadleafHeight = 11.f;      // m, a middling broadleaf tree
    constexpr float ConiferHeight = 15.f;        // m, a middling fir
    constexpr float CloudRadius = 2600.f;        // m, the cloud dome (nearer than the moon at 2790)
    constexpr float CloudTextureScale = 0.16f;   // how many times the cloud picture repeats overhead
    constexpr float CloudFlatness = 0.14f;       // smaller = clouds crowd more toward the horizon
    constexpr float CloudCover = 0.62f;          // opacity of the thickest cloud
    constexpr float CloudBrightness = 0.50f;     // moonlit grey-blue
    constexpr float CloudEmission = 1.0f;
    constexpr float CloudDriftU = 0.0035f;       // picture widths per second: the wind
    constexpr float CloudDriftV = 0.0012f;
}
