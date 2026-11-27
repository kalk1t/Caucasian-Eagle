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
    constexpr int MoonRuneHeal = 4;             // per moon rune: health for every wave and every arena won

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
    constexpr const char* GameVersion = "0.5.4";
    constexpr const wchar_t* UserDataFolder = L"CaucasianEagle"; // %LOCALAPPDATA%\CaucasianEagle: settings and saves

    // ---- content
    constexpr const char* StartSceneId = "RoadToAmirani";                   // assets/Compiled/Scenes/<id>.dscene
    // The Eagle's Ascent (Map A, from Map Lab): another way into the game, on its own (EaglesAscentGame.cpp).
    // CaucasianEagle.exe --eagles-ascent, or CAUCASIAN_EAGLE_ASCENT=1.
    constexpr const char* AscentSceneId = "MapA_EaglesAscent";              // assets/Compiled/Scenes/<id>.dscene
    constexpr const char* AscentDataFile = "assets/Maps/EaglesAscent.txt";   // + .heights: tools/map-to-game.ps1 copies both from Map Lab's build
    constexpr const wchar_t* AscentSwitch = L"--eagles-ascent";
    constexpr const char* AscentEnvironmentVariable = "CAUCASIAN_EAGLE_ASCENT";   // =1: the same as the switch; =0: the road
    // Since 0.5 (the owner, 2026-11-20: "update CaucasianEagle's main exe to latest version") the Eagle's Ascent is what
    // CaucasianEagle.exe starts; the road of 0.4 is started with --road (or CAUCASIAN_EAGLE_ROAD=1, or the road's test
    // pilot, CAUCASIAN_EAGLE_PILOT=1).
    constexpr const wchar_t* RoadSwitch = L"--road";
    constexpr const char* RoadEnvironmentVariable = "CAUCASIAN_EAGLE_ROAD";
    constexpr const char* AscentLogEnvironmentVariable = "CAUCASIAN_EAGLE_MAP_LOG";   // testing: the Ascent's evidence file (CAUCASIAN_EAGLE_DEMO=1: its smoke test)
    // its Eagle Nests, bosses and creatures (2026-11-20; art/map/ascent_creatures.py writes it), and her save: the last
    // nest she rested at and the creatures she has beaten (in the user data folder; the road's journey.sav is apart)
    constexpr const char* AscentCreaturesFile = "assets/Maps/EaglesAscent.creatures.txt";
    constexpr const char* AscentSaveFileName = "eagles-ascent.sav";
    constexpr const char* AscentCreaturesEnvironmentVariable = "CAUCASIAN_EAGLE_CREATURES";   // =0: the Ascent without its creatures
    constexpr const char* AscentSaveEnvironmentVariable = "CAUCASIAN_EAGLE_ASCENT_SAVE";       // testing: this file is the Ascent's save
    constexpr const char* HeroBundle = "Characters/Nyxara/MixamoV1";        // under assets/Compiled: her body built from Mixamo clips (Character Lab bundle v31): two swords, wings, no bow
    constexpr const char* HeroBundleBefore = "Characters/Nyxara/EagleV1";   // the body before it, weapons part of the mesh: kept whole, the way back
    constexpr const char* HeroBundleFolder = "Characters/Nyxara";           // CAUCASIAN_EAGLE_HERO names another bundle in this folder
    constexpr const char* HeroEnvironmentVariable = "CAUCASIAN_EAGLE_HERO"; // =EagleV1: the body before, weapons in the mesh. =RodinV1: the generated body (candidate; less detail)
    constexpr const char* BowAimJoint = "Spine02";    // with the bow up, her body from this joint up pitches to where the arrow goes
    constexpr float BowAimPitchMax = 1.0f;            // rad, the most it pitches up or down
    constexpr float BowAimPitchRate = 14.f;           // 1/s, how fast it follows
    constexpr const char* HeroEquipment = "Characters/Nyxara/Equipment";    // under assets/Compiled: the weapons of the new body
    constexpr const char* DemoEnvironmentVariable = "CAUCASIAN_EAGLE_DEMO"; // =1: scripted showcase run
    constexpr const char* VistaEnvironmentVariable = "CAUCASIAN_EAGLE_VISTA"; // =1 with the showcase: camera held on the mountain

    // ---- journey: the road to Amirani (Journey.cpp; world built by art/world/build_world.py)
    constexpr const char* RoadFile = "assets/Compiled/World/Road.txt";   // path, arenas, chests, props, lights
    constexpr const char* SaveFileName = "journey.sav";                   // in the user data folder
    constexpr const char* SaveEnvironmentVariable = "CAUCASIAN_EAGLE_SAVE";   // testing: this file is the save, read and written in unattended runs too
    constexpr const char* SaveKeyEnvironmentVariable = "CAUCASIAN_EAGLE_SAVEKEY"; // testing, unattended: F8 is pressed after this many seconds
    constexpr float DeathSeconds = 3.5f;         // s she lies where she fell before she wakes at the last saved point
    constexpr const char* PilotEnvironmentVariable = "CAUCASIAN_EAGLE_PILOT"; // =1 with the showcase: a test pilot plays the whole road
    constexpr const char* SpeedEnvironmentVariable = "CAUCASIAN_EAGLE_SPEED"; // test pilot only: simulation speed 1..12
    constexpr const char* MortalEnvironmentVariable = "CAUCASIAN_EAGLE_MORTAL"; // =1: the test pilot can die and asks for a respawn (tests death)
    constexpr const char* JournalEnvironmentVariable = "CAUCASIAN_EAGLE_JOURNAL"; // testing: hold the story journal open on page 1..5 (11..15: everything unlocked)
    constexpr const char* StartEnvironmentVariable = "CAUCASIAN_EAGLE_START"; // start after this many arenas (testing; ignores the save)
    constexpr const char* AtEnvironmentVariable = "CAUCASIAN_EAGLE_AT";       // testing, with the pilot: =N stands her N metres along the road (CAUCASIAN_EAGLE_GO=1: and lets her go on from there)
    // ---- the timing of her attacks (NyxaraKit). "Clip seconds" are seconds of the animation, before its rate.
    constexpr float ComboWindowSeconds = 1.2f;   // s after a skill ends in which its combo can still be started
    constexpr float ComboBufferSeconds = .6f;    // s a mouse press during a skill is remembered; the combo starts when the skill's last hit has landed
    constexpr float SkillBufferSeconds = .45f;   // s a skill key pressed too early is remembered
    constexpr float RecoveryCancel = .22f;       // clip s after the last hit of a skill or combo: from here a slash, the heavy blow, the barrier or the swap cuts the recovery
    constexpr float SlashChainAfterHit = .12f;   // clip s after a slash lands: the next buffered slash or heavy blow starts
    constexpr float HeavyChainAfterHit = .30f;   // clip s after the heavy blow lands: a buffered slash starts
    constexpr float SlashChainSeconds = 1.5f;    // s after a slash in which the next click swings the other sword
    constexpr float SlashStringSeconds = 1.6f;   // s after a slash starts in which the next one counts on in the string (the third is the flurry)
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

    // ---- traps (Journey::BuildTraps). They are made for a traveller on the wing: each reaches from the ground to
    //      past her ceiling, each is beaten once (the save keeps it) and then stands quiet.
    constexpr const char* TrapEnvironmentVariable = "CAUCASIAN_EAGLE_TRAP";   // testing, with the pilot: =N stands her before trap N, =-N in its middle (CAUCASIAN_EAGLE_GO=1: and she goes on; =2: and fights nothing on the way)
    constexpr float TrapHeight = 70.f;           // m over the road up to which a trap reaches (her ceiling is 45)
    constexpr float TrapSide = 26.f;             // m beyond the way through it within which it still reaches her
    constexpr float TrapHalf = 7.f;              // m, half width of the way through one (a clearing's road is wider than that)
    constexpr float TrapWarn = 30.f;             // m before one at which the line under the compass names it
    constexpr int TrapCrystals = 30;             // crystals for beating one
    // the gate run: a light over the road opens a gate far ahead, for as long as a fast flight there takes
    constexpr float GateRunLength = 195.f;       // m from the switch to the gate
    constexpr float GateSwitchHeight = 4.5f;     // m over the road
    constexpr float GateSwitchTouch = 3.6f;      // m
    constexpr float GateRunPace = 1.45f;         // the time allows for this multiple of her cruising speed (the wing boost is 2.2)...
    constexpr float GateRunGrace = 3.f;          // ...and this many seconds more
    constexpr float GateRetry = 3.f;             // s the switch sleeps after a run that failed
    constexpr float GateArchTall = 22.f;         // m, the arch that holds the gate
    constexpr float GateHalf = 8.f;              // m, half width of the door in it
    constexpr float GateTall = 14.f;             // m, its height
    constexpr float GateDoor = 1.2f;             // m before and behind the shut door at which she is stopped
    constexpr int GateBars = 5;                  // bars of fire across the course while a run lasts
    constexpr float GateBarLow = 2.f;            // m over the road between which each rises and falls
    constexpr float GateBarHigh = 15.f;
    constexpr float GateBarOut = 9.f;            // m beyond the road's edge that a bar reaches
    constexpr float GateBarDamage = 12.f;
    constexpr int GateGuards = 4;                // creatures that wake on the course
    // wind teeth: gusts throw her across a way between two walls of ice wind
    constexpr float WindLength = 56.f;
    constexpr float WindEdge = 3.f;              // m beyond the way's edge where the walls stand
    constexpr float WindCalm = 2.4f;             // s the next gust gathers (its streaks show which way)
    constexpr float WindBlow = 1.8f;             // s it blows
    constexpr float WindPush = 1.1f;             // its speed, times her cruising speed (on foot: half)
    constexpr float WindWall = 1.0f;             // m, half thickness of a wall
    constexpr float WindDamage = 11.f;
    // rune stones: three stones throw fire at her in turn
    constexpr float LauncherLength = 60.f;
    constexpr float LauncherTall = 5.5f;
    constexpr float LauncherEvery = 2.6f;        // s between a stone's volleys
    constexpr float LauncherSpeed = 13.f;        // m/s
    constexpr float LauncherDamage = 9.f;
    // watchers: two statues turn a sheet of light each, from the ground to the sky
    constexpr float WatchLength = 50.f;
    constexpr float WatchTall = 9.f;
    constexpr float WatchTurn = 0.75f;           // rad/s
    constexpr float WatchReach = 36.f;           // m, length of a sheet
    constexpr float WatchWidth = 1.3f;           // m, half its thickness
    constexpr float WatchDamage = 14.f;
    constexpr float WatchGuardEvery = 9.f;       // s at least between two times they wake guards
    // the cracked seal: stones fall on her; one that lands on the seal cracks it
    constexpr float SealLength = 48.f;
    constexpr float SealRadius = 4.5f;
    constexpr int SealCracks = 3;
    constexpr float SealEvery = 1.3f;            // s between stones
    constexpr float SealWarn = 1.25f;            // s from the ring to the stone
    // the Kaji press: a hammer across the road; a link over it can be broken only just after each blow
    constexpr float PressLength = 44.f;
    constexpr float PressTall = 11.f;            // m, its frame
    constexpr float PressHead = 2.6f;            // m, height of the hammer
    constexpr float PressDepth = 3.4f;           // m, along the road
    constexpr float PressPillarOut = 1.6f;       // m beyond the way's edge where its pillars stand
    constexpr float PressEvery = 3.6f;           // s from blow to blow
    constexpr float PressWarn = 1.0f;            // s the hammer shakes before it falls
    constexpr float PressFall = 0.25f;           // s of the fall
    constexpr float PressOpen = 1.4f;            // s after the blow the link can be broken
    constexpr float PressDamage = 28.f;          // under the hammer
    constexpr float PressOrbDamage = 8.f;        // the fire the blow throws out, also upward
    constexpr float PressLinkUp = 3.4f;          // m over the frame where the link hangs
    constexpr float RoofStep = .5f;              // m she steps up onto a building's surface; lower than this under it, its wall stops her
    constexpr int RoofGrid = 96;                 // cells per side of a building's top-height map
    constexpr const char* RoofEnvironmentVariable = "CAUCASIAN_EAGLE_ROOF"; // unattended test: she is dropped over building N-1
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
    constexpr float MonsterSprint = 1.6f;        // ...at this multiple of its speed
    constexpr float MonsterTurnRate = 9.f;       // rad/s a creature turns to face her
    constexpr float MonsterLeadSeconds = 0.45f;  // creatures run to where she will be this far ahead, not to where she is
    constexpr float MonsterFlank = 0.65f;        // how strongly melee creatures swing out to the sides as they close in (0 = straight at her)
    constexpr float MonsterFlankDistance = 7.f;  // m beyond striking distance where the swing begins
    constexpr float MonsterCircleSpeed = 0.55f;  // share of its speed a creature circles her with between its blows
    constexpr int LateWaveStage = 3;             // from this region on an arena has 4 waves instead of 3
    constexpr float PatrolBudget = 0.8f;         // a roaming pack costs this share of an arena's first wave
    constexpr float PatrolEvery = 55.f;          // m of open road between two arenas for every roaming pack in it (at most six)
    constexpr float PatrolAhead = 26.f;          // m, a roaming pack appears this far ahead of her
    constexpr float PatrolGiveUp = 70.f;         // m, a pack left this far behind gives up
    constexpr float PackLost = 130.f;            // m, a pack this far from her in any direction gives up
    constexpr float UnderLand = .8f;            // m, deeper than this under the land (and the road) and not on ground: she is lifted back: there is no ground down there
    constexpr float CorridorRim = 8.f;           // m beyond the road's edge where the ground may still lie level with the road (she is lifted to the higher of the two)
    constexpr float PackRoam = 40.f;             // m beyond the road's edge that a roaming pack follows her over the land
    constexpr float PackDrop = 3.f;              // m below the road: lower land (water, a chasm) a pack does not go down to...
    constexpr float PackClimb = 7.f;             // ...nor up land this far above it
    constexpr float PackFlank = 11.f;            // m beyond the road's edge where part of a pack comes out of the land
    // ---- creatures against a traveller in the air (UpdateMonsters)
    constexpr float MeleeReachUp = 1.2f;         // m over its own height that a creature's blow still reaches
    constexpr float FlyerCeiling = 22.f;         // m over the ground that spirits and witches follow her up
    constexpr float FlyerClimb = 5.5f;           // m/s
    constexpr float LeapReach = 15.f;            // m over its feet that a small creature can leap to
    constexpr float LeapRange = 11.f;            // m along the ground from which it leaps
    constexpr float LeapSeconds = 0.95f;         // up and down again
    constexpr float LeapEvery = 4.5f;            // s between leaps, about
    constexpr float LeapTouch = 1.5f;            // m beyond its radius within which the leap strikes her
    constexpr float LeapDamage = 1.2f;           // times its blow
    constexpr float ThrowReach = 34.f;           // m over its feet up to which a giant throws stones at her
    constexpr float ThrowRange = 28.f;           // m along the ground
    constexpr float ThrowSpeed = 15.f;           // m/s
    constexpr float ThrowEvery = 3.2f;           // s, about
    constexpr float ThrowDamage = 0.8f;          // times its blow
    constexpr float StageHealthGrowth = 0.20f;   // + per stage (12 stages): creatures of the last stage have 3.2 times the health
    constexpr float StageDamageGrowth = 0.07f;   // + per stage: creatures of the last stage hit 1.77 times as hard
    constexpr float FeatherDamageBonus = 0.03f;  // Nyxara's damage per Eagle Feather
    constexpr float EmberSeconds = 20.f;         // Soul Ember duration
    constexpr float EmberDamage = 1.5f;          // Nyxara's damage multiplier while it burns
    constexpr float MercySeconds = 1.0f;         // after a hit she cannot be hurt again for this long (a crowd cannot kill her in one moment)
    constexpr int MaxAttackers = 3;              // ordinary creatures that may wind up a blow at the same time

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
    // (the creatures' dodges, each kind its own: Journey.cpp's Styles and BeginEvade, 2026-11-20)
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
    // The Eagle's Ascent (0.5, owner 2026-11-20): "when nyxara starts first time she is only able to fly for 10 seconds,
    // first upgrade increase the flying time to 15,second upgrade to 20 and third upgrade to 30"
    constexpr float AscentFlightSeconds[MaxItemTier + 1] = {10, 15, 20, 30};
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
    // The Eagle's Ascent's nine nests (0.5.2 sold a level of her growth at each; 0.5.3 has her gears instead, below): the
    // crystals' ledger (EaglesAscentGame::Ledger, DEMO=8) still counts the crystals between them.
    constexpr int AscentMaxLevel = 9;
    // Her gears (0.5.3, the owner 2026-11-22: three gears, each earned with a set of quests, the nine levels gone; docs/
    // plan-three-gears-masters.md): her own, THE HIGHLANDS, AMIRANI'S FIRE, MOONLIT. The last is as strong as level 9 was.
    constexpr float GearFlightSeconds[4] = {10, 15, 22, 30};    // how long she flies
    constexpr float GearDamage[4] = {1.f, 1.6f, 2.3f, 3.2f};     // the damage of her blows
    constexpr float GearTaken[4] = {1.f, .8f, .6f, .3f};         // the damage she takes
    // Her crystals' kinds by the stretch of the way (0.5.3): wind on the way up to the third Eagle Nest (22 % of the way),
    // fire from there up to the sixth (60 %), ice beyond (AscentGrowth::CrystalKindOf). Her first set of quests is bought
    // with wind, the second with fire, the third with ice (AscentQuests). (First chosen as the spirits' places, which the
    // owner took out the same day: "we dont need masters anymore".)
    constexpr int CrystalNests[2] = {3, 6};
    constexpr float GearAttackPace = .5f;                         // her skills play at half speed, and her attacks where her gear's flair is off (the owner: "effects and half speed"); her flair's strings have their own pace (NyxaraFlair::Info), her slide its own (NyxaraKit.cpp: SlidePace)
    constexpr int AscentBossCrystals = 25;       // crystals from a boss of the Ascent (art/map/ascent_creatures.py BOSS_CRYSTALS)
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
    // Soaring (travel on the ground) and free flight (hold Space). Presentation, except the ceiling.
    constexpr float GlideLift = 0.75f;          // m her feet are off the ground while she soars
    constexpr float GlideLiftBoost = 1.0f;      // m, the same with the wing boost
    constexpr float GlideLiftRate = 2.6f;       // 1/s, how fast she rises to that height and settles from it (lower = slower)
    constexpr float SoarSway = 0.06f;           // m she rises and falls on the air while soaring
    constexpr float SoarSwayRate = 1.3f;        // rad/s of that sway (one rise and fall in about 5 s)
    constexpr float SoarBank = 0.12f;           // rad of bank per rad/s of turning
    constexpr float SoarClimbPitch = 0.30f;     // rad the nose lifts in a full climb and drops in a full dive (free flight)
    constexpr float SoarSpring = 30.f;          // stiffness of the lean and bank spring (lower = slower, smoother)
    constexpr float SoarDamping = 10.f;         // its damping (about 11 = no overshoot at this stiffness)
    constexpr float SoarBoostRate = 1.35f;      // wing-beat clips play this much faster with the wing boost
    constexpr float RageCrySeconds = 2.5f;      // s at least between two of her cries of fury on kills
    // R changes between two ways of going: on the wing (the camera aims her) and on foot (she walks and runs;
    // Space jumps).
    constexpr float FlightTakeoffHeight = 6.f;  // m she rises by herself when R takes her up
    constexpr float FlightTakeoffSeconds = .8f; // s that rise takes, sprung (FreeFlightTakeoffSprung: fastest off the ground);
                                                // 3 s at one speed until 2026-11-26 (the owner: "can she start flying up right
                                                // away after 1sec"); her Fly_Rise's spring and beat take 0.8 s from the lift
    constexpr float FlightLandReach = 6.f;      // m over the ground within which R lands her; higher, R only says "too high"
    constexpr float FlightLandSeconds = 3.f;    // s the landing takes
    constexpr float FlightLandMinSpeed = .05f;  // m/s, the slowest she comes down (the landing always takes FlightLandSeconds)
    constexpr float FootSpeed = 3.2f;           // m/s on foot: an easy jog (the Run clip at 0.7 of its pace, so her legs match the ground)
    constexpr float FootRunSpeed = 5.4f;        // m/s on foot with Shift (the Run clip at 1.17). Ctrl walks at WalkSpeed, the Walk clip's own pace
    // In the game she goes three times as fast as her paces above were made for (FootPace: Ctrl 5.0 m/s, W 9.6, Shift
    // 16.2, her stride stepped to them: NyxaraKit::SetFootPace), in the way of going SHADOW DASH (GaitStyle 4,
    // NyxaraKit::SetGaitStyle: low and fast, crouched, bowed far forward, her arms swept straight back, her wings flat
    // to her back). The owner, 2026-11-26, having tried them in the Character Lab: "i like 4th walk style - i like the
    // speeded up also - implement it ingame". (1 and 0 until then: her walk and run as made.)
    constexpr float FootPace = 3.f;
    constexpr int GaitStyle = 4;
    // She flies in the way EAGLE (FlightStyle 1, NyxaraKit::SetFlightStyle: her whole flight posed by NyxaraFlight.h in
    // place of her three flight clips; deep slow beats and long soaring glides, her body long and level, her legs
    // trailing; two great beats to rise; she lands braking, her wings cupped forward; her swords on her back unless she
    // is armed). The owner, 2026-11-27, having tried ten ways in the Character Lab: "i like 1st most - so implement it
    // ingame". (0 until then: her Fly_Rise, Fly and Fly_Land as made.)
    constexpr int FlightStyle = 1;
    constexpr float FlightSway = .11f;          // m she rides up and down in the air
    constexpr float FlightSwayRate = 1.9f;      // rad/s of that
    constexpr float FlightRock = .035f;         // rad her body rocks in the air (pitch; the roll is 0.8 of it, slower)
    constexpr float FlightGroundLift = .8f;     // m she hovers over the ground when she is low in flight
    constexpr float FlightSteerGain = 1.f;      // how strongly looking up or down with the camera climbs or dives
    constexpr float FlightCeiling = 45.f;       // m above the ground under her that free flight can climb

    // ---- camera (orbit rig looking down on Nyxara)
    constexpr float CameraLookScale = 0.6f;    // how fast the mouse and the right stick turn the camera, against the engine's
                                               // own speed (1). The player's "Mouse speed" setting multiplies this.
    constexpr const char* PointerFile = "assets/ui/Cursor/Pointer.cur";   // the mouse pointer; drawn by art/ui/build_cursor.py
    constexpr float CameraPitch = -0.50f;      // rad, about 29 degrees down: the camera sits high behind her (more negative = higher)
    constexpr float CameraDistance = 15.0f;    // m from the chest pivot
    constexpr float CameraMaxDistance = 18.0f; // m, how far the mouse wheel can pull back (the engine's own limit is 8)
    constexpr float CameraPivotHeight = 1.35f; // m above her feet

    // The bow (Q changes between it and the axes). Right mouse button held: she aims, and the camera comes in over
    // her shoulder with a mark at the middle of the screen. Left mouse button, held: arrows, one after another.
    constexpr float BowReleaseSeconds = 0.08f; // s from loosing an arrow until her hand is back on the string
    constexpr float BowDrawSeconds = 0.2533f;  // s her hand takes to pull the string back; the arrow goes when it is drawn
    constexpr float BowInterval = BowReleaseSeconds + BowDrawSeconds;   // s between arrows while the button is held (a third of a second: three arrows a second)
    constexpr int BowDamage = 25;              // an arrow loosed without aiming (it finds the nearest enemy ahead by itself)
    constexpr int BowAimDamage = 25;           // an aimed arrow
    constexpr float BowArrowSpeed = 75.f;      // m/s
    constexpr float BowRange = 60.f;           // m an arrow flies
    constexpr float BowHipRange = 32.f;        // m within which an arrow loosed without aiming finds an enemy
    constexpr float BowAimAssist = 0.5f;       // m beside an enemy's body an aimed arrow still counts as on it
    constexpr float BowAimAssistFar = 0.012f;  // and this much more for each metre it is away (far targets are small)
    constexpr float BowAimArrowSpeed = 75.f;   // m/s, an aimed arrow (the same: set by the owner, 2026-11-13)
    constexpr float BowHoming = 10.f;          // how hard an arrow bends to the enemy it was loosed at (it never turns round)
    constexpr float BowSnapDegrees = 22.f;     // as she raises the bow the view turns to the enemy nearest the middle, within this angle
    constexpr float BowSnapRate = 12.f;        // how fast it turns there (per second)
    constexpr float BowAimLevelPitch = -0.10f; // rad: with no enemy to turn to, the view comes up to this, nearly level
    constexpr float BowAimZoom = 0.20f;        // the view narrows by this part while she aims
    constexpr float BowAimFriction = 0.55f;    // with the mark on an enemy the mouse turns the view this much as fast again
    constexpr float BowAimDistance = 4.5f;     // m, the camera's distance while she aims
    constexpr float BowAimSide = 0.85f;        // m the camera stands to her right while she aims
    constexpr float BowAimRaise = 0.35f;       // m the camera's pivot rises while she aims
    constexpr float BowAimLook = 0.6f;         // the mouse turns the view this much as fast while she aims
    constexpr float BowPoseSeconds = 0.45f;    // s she keeps the bow up after an arrow
    constexpr float BowClipEnd = 0.2f;         // s, the BowShot clip: loosed at 0, drawn again at the end
    constexpr float CameraMinPitch = -1.2f;    // rad, steepest look-down
    constexpr float CameraMaxPitch = 1.05f;    // rad, highest look-up: the view's top edge near straight up (owner 2026-11-22: "look up all up")
    // Looking up, the camera swings down behind her and in close (owner 2026-11-22: "it should bring camera to nyxara
    // closer"): her chest stays CameraLookUpKeep under the view's middle until the camera is CameraLookUpClear over her
    // feet and CameraLookUpNear from her chest; looking further up she sinks to the view's bottom, head and shoulders last.
    constexpr float CameraLookUpNear = 1.7f, CameraLookUpKeep = 0.2f, CameraLookUpClear = 0.35f;
    constexpr float CameraLookUpSide = 0.6f;   // m, close in, the camera is over her right shoulder: her wings off the view's middle
    constexpr float CameraPivotSmoothing = 0.12f; // s, the camera follows her height smoothly: on stairs it glides, not jumps
    // m, and never further from her height than this (2026-11-26). Her slide (F) goes up a steep slope or stairs so
    // fast, as much as 19.6 m in its second, that the pivot was left up to 2.9 m behind: under the ground at her feet
    // (it is 1.35 m over them). The camera, stopped by that ground at once, was inside the land for a tenth of a second
    // or two: in 40 of 162 slides up the way and 52 of 324 off it, over the whole way (the off-the-way test with -Dash
    // and -Trace; CAUCASIAN_EAGLE_PIVOT_LAG=0 is its baseline). With the limit: in none up the way, and off it in two
    // where it is so without her slide too. Running up, the pivot is 0.76 m behind her at the most, and the stairs'
    // glide is as it was. Without her slide the limit is met where she is set on a cave's floor 1.7 m up at once
    // (1,700 m right of the way, 3,780 m on it, 7,620 m left), and running down the steepest slopes off the way, where
    // the pivot would be 1.13 m over her at the most.
    constexpr float CameraPivotLagMost = 1.0f;
    constexpr float CameraLiftUnder = 3.5f;       // m, a slope or wall behind her bringing the camera nearer than this: it rises over it
    constexpr float VistaPitch = 0.24f;        // rad, capture mode (CAUCASIAN_EAGLE_VISTA=1): look up at the mountain

    // ---- night sky (art/sky/make_sky_textures.py; drawn by AmiraniMountain, paths relative to the game folder)
    constexpr const char* SkyTexture = "assets/textures/caucasus/NightSky.jpg";
    constexpr float SkyRadius = 3000.f;     // m, dome radius around the camera (engine far plane is 5400)
    constexpr float SkyTextureWraps = 2.f;  // times the sky texture repeats around the horizon
    constexpr float SkyBrightness = 0.46f;  // dome tint (the engine's ambient light multiplies it)
    constexpr float SkyEmission = 1.0f;
    constexpr float MoonlightX = 0.72f, MoonlightY = -0.56f, MoonlightZ = 0.40f; // direction the moonlight travels (no moon is drawn)
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
    constexpr const char* FlyEnvironmentVariable = "CAUCASIAN_EAGLE_FLY";       // testing, with the pilot: =1 flies: climbs from 4 s, hovers from 8 s, sinks from 16 s
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
    constexpr const char* PlacesFile = "assets/Compiled/World/Places.txt";  // the places beside the road and what stands there (art/places/build_places.py)
    constexpr int PlaceCrystals = 8;             // crystals for finding a place beside the road
    constexpr float PlaceFindHeight = 60.f;      // m above or below a place within which she still finds it (she may be on the wing)
    constexpr const char* MapTexture = "assets/ui/Map/WorldMap.png";        // the painted world map (art/map/build_map.py)
    constexpr const char* MapEnvironmentVariable = "CAUCASIAN_EAGLE_MAP";   // testing: =1 holds the world map open
    constexpr float CityDrawDistance = 450.f;    // m, buildings are drawn this far
    constexpr float CityTint = 0.78f;            // darkens the buildings under the engine's strong moonlight
    constexpr float CityEmission = 0.16f;        // self-light, so lamp-lit windows read at night
    constexpr float FarModelHeights = 12.0f;     // a place model with a far version (art/places/prepare_places.py) draws it beyond 12 times its height
    constexpr float MinimapRange = 110.f;        // m from Nyxara to the rim of the minimap
    constexpr int WinePotionHeal = 10;           // Telavi: more health from every potion
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
