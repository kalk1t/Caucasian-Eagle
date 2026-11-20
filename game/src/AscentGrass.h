#pragma once

// The Eagle's Ascent's grass fields (2026-11-20, the owner: "i want grass fields, i mean connection of the little grass
// assets you have", at least a thousand times the grass beside the path).
//
// Tufts of blades (three upright cards through one point, the road's Grass.png) over the whole near land wherever the
// ground is painted meadow, thinner on the forest floor and the moss, never on the path, rock, scree, sand or snow:
// read from the map's own splat (<map>.splat_a.dds and .splat_b.dds, testing/MapLab/terrain_splat.py), so the grass
// is where the ground looks green. Beyond the splat's rectangle (the rings round the mountain) on gentle land under the
// snow line. Not on steep ground, not within a built model's reach, not on a ring's path.
//
// Built in 12 m squares round the camera as she moves: every square out to FarDistance holds a light layer (one tuft in
// three); the squares within NearDistance add the rest, so the field round her is thick and closes up as she walks.
// Where a tuft stands is a rule (a hash of the place), not stored: the same field every run. Nothing here stops her.
// CAUCASIAN_EAGLE_GRASS=0: no grass (tests, pictures).

#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class Renderer;

namespace CaucasianEagle
{
    class EaglesAscent;

    class AscentGrass
    {
    public:
        static constexpr float Cell = 12.f;            // m, side of one square (one or two meshes)
        static constexpr float NearDistance = 34.f;    // m, the thick field drawn this far from the camera
        static constexpr float FarDistance = 78.f;     // m, the light layer this far
        static constexpr float Density = 13.f;         // places tried per square metre (where the ground is all meadow, most hold a tuft)
        static constexpr int BuildsPerFrame = 3;

        // After the map's models are loaded (they keep the grass away). False only on a broken texture; with no splat the
        // grass grows by height and slope alone.
        bool Initialize(Renderer& renderer, const EaglesAscent& map, const std::filesystem::path& dataFile, std::string& error);
        void Draw(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection);
        bool On() const { return mReady; }
        // last frame: tufts drawn, squares drawn
        int TuftsDrawn = 0, CellsDrawn = 0;
        // 0..1: how thick grass grows at (x, z) (0: none); y is the land's height there
        float Grows(float x, float z, float& y) const;

    private:
        struct Square
        {
            Doorways::DoorwaysGpuMesh Light, Thick;
            int LightTufts = 0, ThickTufts = 0;
            bool HasThick = false;
            unsigned Used = 0;
            DirectX::XMFLOAT3 Tint{1, 1, 1};
        };
        void Build(Renderer& renderer, int cx, int cz, bool thick, Square& square);
        float Splat(const std::vector<unsigned char>& picture, int channel, float x, float z) const;

        const EaglesAscent* mMap = nullptr;
        bool mReady = false;
        unsigned mFrame = 0;
        // the splat: RGBA8, mW x mH texels of Res m from (mX0, mZ0); a: meadow forest scree sand, b: trail moss snow rock
        std::vector<unsigned char> mA, mB;
        int mW = 0, mH = 0;
        float mX0 = 0, mZ0 = 0, mRes = 1.5f, mSnow = 3500.f, mSea = 0;
        struct Built { float X, Z, R; };
        std::vector<Built> mBuilt;
        std::unordered_map<long long, std::vector<unsigned>> mBuiltCells;   // the models round each 48 m square
        std::unordered_map<long long, Square> mSquares;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTexture;
    };
}
