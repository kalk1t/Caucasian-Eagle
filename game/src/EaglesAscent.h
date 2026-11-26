#pragma once

#include "Rendering/DoorwaysGpuMesh.h"
#include "WorldState.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class Renderer;
struct EnvironmentSettings;
struct DoorwaysRuntimeScene;
namespace Doorways { struct DoorwaysGameHost; }

// A mountain map played on foot and in flight: Map A, "The Eagle's Ascent" (sea level to Amiran's court at 4,200 m), and
// the other Map Lab maps. One copy of the map's rules, used by the game's Eagle's Ascent mode (CaucasianEagleGame.cpp)
// and by Map Lab (testing/MapLab), whose tests (-Smoke, -Cross, -Walk, -Miss) so test the code the game runs.
//
// The map is a compiled scene (its land, rock, paving, pieces and collision: the engine draws it and walks her on it)
// plus a data file written by testing/MapLab/build_mountain.py: <map>.txt (stations, trail, flight lines, stretches only
// wings cross, updrafts, wind, zones, game models standing on it) and <map>.heights (the land's heights on a grid).
// This class reads the data file and keeps the map's rules:
// - checkpoints: standing on a checkpoint station's terrace makes it the place to return to;
// - the air: updrafts lift her in flight (not while she lands), a crosswind pushes her;
// - the rock: the engine's flight has no collision with the land, so in flight she is kept over it, and rock steeper than
//   she could walk stops her;
// - the safety nets: under the sea, falling inside the mountain (away from the route the land has no walkable collision),
//   or far from the trail, she is returned to the last checkpoint;
// - the game's own models standing on the map (no collision), and the map's light (day, dawn, night; clear, haze, fog).
namespace CaucasianEagle
{
    class EaglesAscent
    {
    public:
        // Nest: the station's Eagle Nest (LoadCreatures): she rests in it (within NestReach of it) and wakes in it
        struct Station { std::string Name, Id; DirectX::XMFLOAT3 P{}; float Yaw = 0, Along = 0; bool Checkpoint = true; bool HasNest = false; DirectX::XMFLOAT3 Nest{}; };
        static constexpr float StationReach = 22.f, NestReach = 2.3f;   // m from a station, from a nest, that makes it hers
        const DirectX::XMFLOAT3& Rest(int station) const;                 // where she wakes at it: its nest, or the station
        struct Flight { std::string Id, Kind; DirectX::XMFLOAT3 From{}, To{}; };   // a marked flight line: launch apron, landing apron
        struct Updraft { std::string Id; DirectX::XMFLOAT3 P{}; float Radius = 0, Top = 0, Lift = 0; };   // rising air: a column from P.y to Top
        struct Wind { std::string Id; DirectX::XMFLOAT3 P{}; float Radius = 0, X = 0, Z = 0, Speed = 0; }; // a crosswind: a ball of moving air
        struct Zone { float From = 0; std::string Name; };                         // a zone begins at a height
        struct Fly { std::string Kind; float From = 0, To = 0; bool Done = false; }; // a stretch only wings cross (metres along the trail)
        struct Moment { std::string Name; DirectX::XMFLOAT3 P{}; };                  // a named place
        static constexpr float TrailStep = 4.f;   // m between the data file's trail samples
        static constexpr float FarPlane = 14000.f;   // the camera's far plane on this map, m: the summit must be in view from the shore
        // Her jump on the mountain (2026-11-18, the owner: "the jump is so weak"): 9.5 m/s against the engine's 18 m/s2 is
        // 2.5 m up and about a second in the air (the engine's own 6.5 m/s was 1.2 m: it did not clear a parapet). With the
        // session's WalkOffLedges she can run or jump off any edge and fall, or take wing on the way down.
        static constexpr float JumpSpeed = 9.5f;

        // <file>.txt and <file>.heights beside it.
        bool Load(const std::filesystem::path& file, std::string& error);
        // The game models named by the map's 'prop' lines (assets/Compiled/<name>.dmesh + <name>BaseColor.png).
        bool LoadModels(Renderer& renderer, std::string& error);

        // At the start of the game's update (the engine has just moved her): in flight she is kept out of the rock, judged
        // from where she was at the last call. On foot in the air (a jump or a fall off an edge) she is kept on the rock's
        // face where it is too steep to stand on, and slides down it to ground she can stand on (2026-11-18).
        void KeepOutOfRock(Player& player, bool flying, float dt = 1.f / 60.f, const DoorwaysRuntimeScene* scene = nullptr);
        bool Sliding = false;          // this step she slid down rock too steep to stand on
        // On foot a rock face or a bank as drawn is a wall (2026-11-22): KeepOutOfRock holds her where the drawn top
        // (DrawnTop) WallAhead m ahead of her would be more than IntoRock m over her feet. Not near the way (NearTheWay:
        // within WayReach m of its line, on a stair's treads, or by a cave's open end).
        // A long step (2026-11-25: her slide goes half a metre a step of the engine, a metre and more a frame at few frames
        // a second) is gone over in short ones by these rules, by the caves' walls and by the trees' hold, and she is held
        // at the first that would not be taken, as at more frames a second: a step of over LongStep m, in steps of
        // ShortStep at the most. One of PutStep m or more, and more than PutSpeed m/s would carry her in that frame, is
        // no step: she was put there. (Her slide goes 30 m/s at the most and the engine's frame is a tenth of a second
        // at the longest, App.cpp: a step of 3 m.) The rules were made and tested at a run's step, a tenth of a metre.
        // Measured (the off-the-way test with her slide; its baseline, CAUCASIAN_EAGLE_LONG_STEPS=0, is without this):
        // without it her slide went through the tree 9.7 m left of the way at 660 m in nine runs of nine at 31, 21 and
        // 10 frames a second, on to 23.7 m, and was held by it only at 200; with it the tree holds her in sixteen runs
        // of sixteen at 31, 21, 12 and 10.
        // The caves' walls held her in the jars' test at 12 frames a second without it too, a metre deeper in the wall.
        static constexpr float LongStep = .35f, ShortStep = .2f, PutStep = 3.f, PutSpeed = 45.f;
        bool LongSteps = true;         // false: CAUCASIAN_EAGLE_LONG_STEPS=0 (a test's baseline: a long step asked only where it ends)
        static constexpr float IntoRock = .9f, WallAhead = .3f, WayReach = 1.5f;   // (3.5 m: beside a stair's switchback the next leg's line was that near, and she walked into the bank and fell into the mountain at 380 m)
        // The drawn top round rock and steep land (the map data's "tops", build_mountain.py): the highest drawn land, rough
        // ground or rock there, from points 1 m apart; NaN where none was found (open land, off the grid).
        float DrawnTop(float x, float z) const;
        bool NearTheWay(const DirectX::XMFLOAT3& p) const;
        int WallHolds = 0;             // steps she was held at such a wall (the tests count them)
        // The two rules on foot below (at a face as drawn, and from walking into the mountain) are on; a test turns them
        // off (CAUCASIAN_EAGLE_ROCK_RULES=0) to see the engine's steep walls (GameSession().SteepGroundBlocks) alone.
        bool RockRules = true;
        int IntoHolds = 0;             // steps she was held from walking into the mountain (below, KeepOutOfRock)
        // Where she is along the trail (Along, OffTrail, Slope).
        void Locate(const DirectX::XMFLOAT3& position);
        // The air, the checkpoints and the safety nets. Returns the event when she was returned to the last checkpoint
        // ("rescued", "rescued_from_inside_the_mountain"), else nullptr. rescue = false: no safety net (tests).
        const char* Air(Doorways::DoorwaysGameHost& host, float dt, bool flying, bool rescue);
        // Puts her standing at a station, healed, on foot; a checkpoint station becomes the one to return to.
        void Visit(Doorways::DoorwaysGameHost& host, int station);
        bool HealOnVisit = true;       // Visit heals her (the Eagle's Ascent 0.5: no; only potions heal)

        void DrawModels(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection, int light);
        // Hides the game models of that name (a 'prop' line's) within radius m of p, or shows them again; returns how many.
        // The Eagle at Amiran's court is not drawn while he is there as a creature (EaglesAscentGame).
        int HideModels(const std::string& name, const DirectX::XMFLOAT3& p, float radius, bool hidden);
        // the nearest placed model of that name within radius of p: its place and height (false: none)
        bool FindModel(const std::string& name, const DirectX::XMFLOAT3& p, float radius, DirectX::XMFLOAT3& at, float& height) const;
        // light: 0 day, 1 dawn, 2 night. fog: 0 clear air, 1 haze (the map's own, Gate 4), 2 thick fog.
        static void StyleEnvironment(EnvironmentSettings& environment, int light, int fog);

        float Land(float x, float z) const;      // the land's height there (bilinear on the grid); -1e9 off the grid
        // What flight is kept over: in a fine patch (records "patch", round each stretch only wings cross) the highest
        // drawn rock and land on a 1 m grid; elsewhere Land. 2026-11-18: the 6 m grid let her into a cleft's drawn walls.
        float FlightLand(float x, float z) const;
        float LandLow(float x, float z) const;
        // The ground the heights cover (the near grid and the outer land round it): for the ground probe.
        void LandExtent(float& x0, float& z0, float& x1, float& z1) const;   // the lowest corner of the grid cell: a cleft's floor, not its rims averaged in
        // Caves (records "cave <kind> <n> x y z halfwidth height ...", 2026-11-18): is p inside a tunnel? Then the
        // floor under her, the roof over her, and how far she is to the right of its middle (rightX, rightZ the way).
        struct CaveSpot { float Floor = 0, Roof = 0, Across = 0, Half = 0, RightX = 0, RightZ = 0; };
        bool InCave(const DirectX::XMFLOAT3& p, CaveSpot* spot = nullptr) const;
        struct CavePoint { float X = 0, Y = 0, Z = 0, Half = 0, Tall = 0; };
        // Open: each end (first, last) opens to the air or into another cave; else it ends in the rock (a chamber's back)
        struct Cave { std::string Kind; std::vector<CavePoint> Line; float MinX = 0, MaxX = 0, MinZ = 0, MaxZ = 0, MinY = 0, MaxY = 0; bool Open[2] = {true, true}; };
        const std::vector<Cave>& Caves() const { return mCaves; }
        // The rings round the mountain (records "ring <n> x y z ...", 2026-11-18): trails joining the main one at its bends.
        const std::vector<std::vector<DirectX::XMFLOAT3>>& Rings() const { return mRings; }
        float RingDistance(const DirectX::XMFLOAT3& p) const;   // m to the nearest point of a ring; 1e30 with none
                                                 // (on the floor grid when the map has one: the carved rock's ledges included)
        const Zone* ZoneAt(float y) const;
        // The top of the stair tread drawn at (x, z) about 'nearY' (within a step and a half of it; of two there, the
        // higher: the one seen), NaN where there is none.
        // She walks the ramp under the treads; her feet are set on them (NyxaraKit::SetFootGround).
        float TreadTop(float x, float z, float nearY) const;
        // the middle of the tread of this top nearest (x, z) along the stair, and the stair's way (false: none within 2 m)
        bool TreadCenter(float x, float z, float top, float& cx, float& cz, float& ax, float& az) const;

        // Its Eagle Nests, bosses and creatures (2026-11-20): <file>.creatures.txt beside the map's data, written by its
        // placing model, art/map/ascent_creatures.py. The nests become the only checkpoints (she begins on the shore, but
        // it is no longer one); EaglesAscentGame saves her at them and wakes the creatures (Journey::UpdateMountain).
        struct Nest { int Station = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Along = 0; };
        struct Placed
        {
            std::string Id;                    // "way-1200", "cave-07", "boss-giant"
            bool Boss = false, Patrol = false;
            std::vector<std::string> Kinds;    // who is there ("chinka" ..; a boss: "gveleshapi", "giant", "devi_lord", "eagle")
            DirectX::XMFLOAT3 P{}; float Yaw = 0;
            int Cave = -1;                     // -1 on the way; else its cave (Caves())
            float At = 0, Lo = 0, Hi = 0, Roam = 0;   // m along the way (or the cave's line): its place and the stretch it keeps to
            // A group around the way (record "around", 2026-11-21: the owner, "around path didnt have any monsters before
            // and you could add there also (every 150-400 meter)"): off the way at m At along it, to its Side (1 its right
            // going up, -1 its left). PlaceAround finds its place on ground she can walk to from the way, and Spur: its line
            // from the way's middle out to the place and 6 m past it. No spur: no place was found; it stands at the way's edge.
            bool Around = false; int Side = 1;
            std::vector<DirectX::XMFLOAT3> Spur;
            float SpurAt = 0;                  // m along its spur to its place
        };
        std::vector<Nest> Nests;
        std::vector<Placed> Placements;
        // Her growth's crystals (records "level", 2026-11-21): the price of each level of her wings, swords or armour (bought
        // at an Eagle Nest), and the crystals the clay jars hold on the way to each nest, beside the way and off it (in the
        // caves and on the rings: Wayside::Lay shares them out). What lies between two nests pays for one level of all three.
        struct Level { int Price = 0, WayJars = 0, SideJars = 0; };
        std::vector<Level> Levels;             // [0]: level 1, on the way to the first nest
        int StretchAlong(float along) const;   // 1 on the way to the first nest .. Nests.size() + 1 past the last
        int StretchHeight(float y) const;      // the same off the way (a cave, a ring): by its height against the nests'
        // Every group around the way given its place, once the map's ground is up; 'report' gets a line each
        void PlaceAround(const DoorwaysRuntimeScene& scene, std::vector<std::string>* report = nullptr);
        // Before LoadModels: the nests are game models too (NestModel, when it has been compiled; NestModels then).
        bool LoadCreatures(const std::filesystem::path& file, std::string& error);
        static constexpr const char* NestModel = "Landmarks/EagleNest/EagleNest";
        bool NestModels = false;

        // the map (read by the lab's tests and HUD)
        std::vector<Station> Stations;
        std::vector<Zone> Zones;
        std::vector<DirectX::XMFLOAT3> Trail;
        std::vector<Flight> Flights;
        std::vector<Fly> FlyStretches;
        std::vector<Moment> Moments;
        std::vector<Updraft> Updrafts;
        std::vector<Wind> Winds;
        float Summit = 0, TrailLength = 0;
        // her state on it
        int Checkpoint = 0;            // the last checkpoint station she stood at
        float Along = 0, OffTrail = 0, Slope = 0;
        float Rescued = 0;             // seconds left of the "returned to checkpoint" notice
        // The forest's trunks (<map>.trees, written by the Map Lab's build_mountain.py: every tree of the map, ~260 000).
        struct Trunk { float X = 0, Y = 0, Z = 0, R = 0, H = 0; };
        // how high above a trunk's foot it still holds her: a tree's to well up its trunk; a low one's (under 2 m: a
        // fallen log's piece, a tomato's stake, a small shrub) to just over its top, so she can jump over it
        static float TrunkTop(const Trunk& t) { return t.Y + (t.H < 2.f ? t.H + .3f : (std::max)(2.5f, t.H * .75f)); }   // ((std::max): windows.h's max macro)
        size_t TreeCount() const { return mTrunks.size(); }
        bool TreesHold = true;         // false: CAUCASIAN_EAGLE_TREES=0 (a test's baseline)
        // The ground's look record ("terrain x0 z0 width depth snowline sea ..."): false without one.
        bool TerrainInfo(float out[7]) const { if (!mHasTerrain) return false; for (int i = 0; i < 7; ++i) out[i] = mTerrain[i]; return true; }
        // Where the game models stand: x, z and the radius they cover (AscentGrass keeps out of it).
        std::vector<DirectX::XMFLOAT3> BuiltCircles() const;
        // The trunk nearest (x, z) whose foot is within 'band' m of y, if one is within 'reach' m: true and where.
        bool NearestTrunk(float x, float y, float z, float reach, Trunk& out) const;
        const wchar_t* AirNow = L"";   // what the air is doing to her this step
        int ModelsDrawn = 0, ModelCount = 0;

    private:
        struct PropModel { Doorways::DoorwaysGpuMesh Mesh; Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture; DirectX::XMFLOAT3 Min{}, Max{}; };
        struct Prop { int Model = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Height = 1; bool Hidden = false; };
        std::vector<std::string> mPropNames;
        std::vector<PropModel> mPropModels;
        std::vector<Prop> mProps;
        std::vector<float> mLand; float mLandX = 0, mLandZ = 0, mLandStep = 1; int mLandNx = 0, mLandNz = 0;
        // Optional second grid (record "heights ... 2"): at each point the lowest walkable ground in the cells round it.
        // The carved rock lies up to 20 m under the land's grid on a steep face, and its ledges are ground (Gate 5).
        std::vector<float> mFloor;
        struct Patch { float X = 0, Z = 0, Step = 1; int Nx = 0, Nz = 0; std::vector<float> H; };
        std::vector<Patch> mPatches;
        std::vector<int> mTopCell; std::vector<float> mTops; int mTopsPer = 0;   // ("tops": a cell's block, -1 for none; the blocks)
        Patch mOuter;                            // the rest of the round mountain, coarse (the map data's "outer" record): Land() off the near grid
        float OuterLand(float x, float z, bool lowest) const;
        // Stair treads (records "tread"): centre, the path's way, m along it and across, top; found through 4 m cells
        struct Tread { float X = 0, Z = 0, Ax = 0, Az = 1, Along = 0, Across = 0, Top = 0; };
        std::vector<Tread> mTreads;
        std::vector<Cave> mCaves;
        std::vector<std::vector<DirectX::XMFLOAT3>> mRings;
        std::unordered_map<long long, std::vector<int>> mTreadCells;
        // On foot in a cave (2026-11-20, the owner: "nyxara falls out of walls from caves"): the tube's collision is
        // walkable only (the engine stops no one at it), so she is held inside its walls and under its roof here.
        void HoldInCave(Player& p) const;
        // 2026-11-20, the owner: "add collisions to trees". She is held off every trunk, on foot and on wings, from its
        // foot to three quarters of its height (the crowns stay soft: leaves). A box each in the scene would be some
        // 260 000 blockers the engine tries at every step; here only the trunks of her own 8 m squares are looked at.
        // A long step (from: where she was a frame ago; her slide goes half a metre a step of the engine, a metre a frame
        // at 30 frames a second) is gone over in short ones and held at each, as she would be at more frames a second:
        // held only where it ended, a step ending past a trunk's middle is pushed on, out of its far side (2026-11-25).
        void HoldOffTrees(Player& p, bool flying, const DirectX::XMFLOAT3* from = nullptr) const;
        void LoadTrees(const std::filesystem::path& path);
        std::vector<Trunk> mTrunks;
        std::unordered_map<long long, std::vector<unsigned>> mTrunkCells;
        static constexpr float TrunkCell = 8.f;
        static long long TrunkKey(int cx, int cz) { return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cz); }
        void CaveEnds();                 // which ends of each cave are open (Load)
        float mPut = PutStep;          // this frame's longest step that is a step (KeepOutOfRock: PutStep, PutSpeed)
        DirectX::XMFLOAT3 mFlown{}, mStood{}; bool mHasFlown = false, mHasStood = false, mJumped = false; float mAir = 0; float mSlideStill = 0; DirectX::XMFLOAT3 mSlideAnchor{}; float mSlidAgo = 1;
        float mInRock = 0;             // seconds she has been falling inside the mountain
        // The ground's look (record "terrain x0 z0 width depth snowline sea", 2026-11-18): the splat <map>.splat_a.dds
        // and .splat_b.dds covers that rectangle; the engine's terrain shader draws the ML_Terrain* materials with it.
        bool mHasTerrain = false; float mTerrain[7] = {};
        // The far ranges (record "ranges x z radius", 2026-11-18): the axis they stand round and their inner radius.
        // With them the sea runs out past the far plane, and far land and water go over into the sky's colour.
        float mRanges[3] = {};
        std::filesystem::path mDataPath;
    };
}
