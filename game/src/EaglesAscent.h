#pragma once

#include "Rendering/DoorwaysGpuMesh.h"
#include "WorldState.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <vector>

class Renderer;
struct EnvironmentSettings;
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
        struct Station { std::string Name, Id; DirectX::XMFLOAT3 P{}; float Yaw = 0, Along = 0; bool Checkpoint = true; };
        struct Flight { std::string Id, Kind; DirectX::XMFLOAT3 From{}, To{}; };   // a marked flight line: launch apron, landing apron
        struct Updraft { std::string Id; DirectX::XMFLOAT3 P{}; float Radius = 0, Top = 0, Lift = 0; };   // rising air: a column from P.y to Top
        struct Wind { std::string Id; DirectX::XMFLOAT3 P{}; float Radius = 0, X = 0, Z = 0, Speed = 0; }; // a crosswind: a ball of moving air
        struct Zone { float From = 0; std::string Name; };                         // a zone begins at a height
        struct Fly { std::string Kind; float From = 0, To = 0; bool Done = false; }; // a stretch only wings cross (metres along the trail)
        struct Moment { std::string Name; DirectX::XMFLOAT3 P{}; };                  // a named place
        static constexpr float TrailStep = 4.f;   // m between the data file's trail samples

        // <file>.txt and <file>.heights beside it.
        bool Load(const std::filesystem::path& file, std::string& error);
        // The game models named by the map's 'prop' lines (assets/Compiled/<name>.dmesh + <name>BaseColor.png).
        bool LoadModels(Renderer& renderer, std::string& error);

        // At the start of the game's update (the engine has just moved her): in flight she is kept out of the rock, judged
        // from where she was at the last call.
        void KeepOutOfRock(Player& player, bool flying);
        // Where she is along the trail (Along, OffTrail, Slope).
        void Locate(const DirectX::XMFLOAT3& position);
        // The air, the checkpoints and the safety nets. Returns the event when she was returned to the last checkpoint
        // ("rescued", "rescued_from_inside_the_mountain"), else nullptr. rescue = false: no safety net (tests).
        const char* Air(Doorways::DoorwaysGameHost& host, float dt, bool flying, bool rescue);
        // Puts her standing at a station, healed, on foot; a checkpoint station becomes the one to return to.
        void Visit(Doorways::DoorwaysGameHost& host, int station);

        void DrawModels(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection, int light);
        // light: 0 day, 1 dawn, 2 night. fog: 0 clear air, 1 haze (the map's own, Gate 4), 2 thick fog.
        static void StyleEnvironment(EnvironmentSettings& environment, int light, int fog);

        float Land(float x, float z) const;      // the land's height there (bilinear on the grid); -1e9 off the grid
        float LandLow(float x, float z) const;   // the lowest corner of the grid cell: a cleft's floor, not its rims averaged in
                                                 // (on the floor grid when the map has one: the carved rock's ledges included)
        const Zone* ZoneAt(float y) const;

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
        const wchar_t* AirNow = L"";   // what the air is doing to her this step
        int ModelsDrawn = 0, ModelCount = 0;

    private:
        struct PropModel { Doorways::DoorwaysGpuMesh Mesh; Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture; DirectX::XMFLOAT3 Min{}, Max{}; };
        struct Prop { int Model = 0; DirectX::XMFLOAT3 P{}; float Yaw = 0, Height = 1; };
        std::vector<std::string> mPropNames;
        std::vector<PropModel> mPropModels;
        std::vector<Prop> mProps;
        std::vector<float> mLand; float mLandX = 0, mLandZ = 0, mLandStep = 1; int mLandNx = 0, mLandNz = 0;
        // Optional second grid (record "heights ... 2"): at each point the lowest walkable ground in the cells round it.
        // The carved rock lies up to 20 m under the land's grid on a steep face, and its ledges are ground (Gate 5).
        std::vector<float> mFloor;
        DirectX::XMFLOAT3 mFlown{}; bool mHasFlown = false;
        float mInRock = 0;             // seconds she has been falling inside the mountain
    };
}
