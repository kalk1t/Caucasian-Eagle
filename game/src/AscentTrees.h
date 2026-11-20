#pragma once

// The Eagle's Ascent's trees as real 3D models (0.5.2, 2026-11-20; the owner: "improve trees, create its real 3D models,
// refine them, their branches and leaves, their roots").
//
// The map builder lists the trees near the path in <map>.plants (testing/MapLab/build_mountain.py, GAME_TREES: about
// 48 000, one line each: "model variant x y z height yaw leanx leanz snow"); the far forest stays baked into the map.
// The models are art/trees/build_trees.py's (game/assets/Trees: trees.txt, Models/<model>_<variant>_<lod>_{bark,leaves}
// .dmesh, Textures/): grown branch by branch with roots, bark and sprays of leaves, at four levels of detail.
//
// Drawn by distance from the camera: level 0 (every twig and spray) close, 1 and 2 further, and beyond Detail[2] a
// picture of the tree (an impostor: three crossed quads, art/trees/impostors.py). Every model, variant and level is one
// instanced draw; the far trees of a square are merged into one mesh, built once, one draw. The trees within
// ShadowReach cast the sun's shadow (Renderer::DrawDoorwaysGpuSubmeshShadowInstanced, through the game's shadow hook).
// Their trunks hold her as before (<map>.trees, EaglesAscent::HoldOffTrees).
// CAUCASIAN_EAGLE_GAME_TREES=0: none drawn (tests, before/after pictures).

#include "Rendering/DoorwaysGpuMesh.h"
#include "RenderTypes.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class Renderer;

namespace CaucasianEagle
{
    class AscentTrees
    {
    public:
        static constexpr float Cell = 192.f;             // m, side of a square of trees
        static constexpr float Detail[3] = {30.f, 85.f, 230.f};   // m: level 0 within the first, 1 within the second, 2 within the third
        static constexpr float FarReach = 3200.f;        // m: the impostors this far
        static constexpr float ShadowReach = 150.f;      // m: trees this near cast shadows
        static constexpr int BuildsPerFrame = 2;          // far squares merged per frame

        // dataFile: the map's data file (<map>.txt); its .plants beside it. No .plants or no library: no trees, and true.
        // False only when the library is there but broken.
        bool Initialize(Renderer& renderer, const std::filesystem::path& dataFile, std::string& error);
        void Draw(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection);
        void DrawShadows(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& lightViewProjection);
        bool On() const { return mReady && !mTrees.empty(); }
        size_t Count() const { return mTrees.size(); }
        // last frame: trees drawn at each level (3: impostors, near and merged), instanced draws, far squares drawn
        int Drawn[4] = {}, Draws = 0, FarSquares = 0, Shadowed = 0;
        std::string Report() const;

    private:
        struct Part { Doorways::DoorwaysGpuMesh Mesh; Doorways::DoorwaysDMeshSubmesh Sub{}; bool Valid = false; };
        struct Impostor { float U0 = 0, V0 = 0, U1 = 1, V1 = 1, W = 1, H = 1; Doorways::DoorwaysGpuMesh Mesh; bool Valid = false; };
        struct Variant
        {
            float Height = 1;
            std::array<std::array<Part, 2>, 3> Lod;      // [level 0..2][bark, leaves]
            Impostor Far[2];                             // [no snow, snow]
        };
        struct Model { std::string Name; int Kind = 0; bool Conifer = false, Snow = false; std::vector<Variant> Variants; };
        struct Kind
        {
            std::string Name;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Bark, BarkNormal, Leaves, LeavesSnow;
        };
        struct Tree
        {
            DirectX::XMFLOAT4X4 World, WorldInvTranspose;
            DirectX::XMFLOAT3 Centre;   // of its crown, for culling and distance
            float Radius = 1, Scale = 1;
            unsigned short Model = 0; unsigned char Variant = 0, Snow = 0;
        };
        struct Square
        {
            std::vector<unsigned> Trees;
            DirectX::XMFLOAT3 Min{1e9f, 1e9f, 1e9f}, Max{-1e9f, -1e9f, -1e9f};
            Doorways::DoorwaysGpuMesh FarMesh; int FarTrees = 0; bool FarBuilt = false;
        };
        bool LoadLibrary(Renderer& renderer, const std::filesystem::path& folder, std::string& error);
        void LoadPlants(const std::filesystem::path& file);
        bool BuildImpostor(Renderer& renderer, Impostor& imp);
        void BuildFar(Renderer& renderer, Square& square);
        // which bucket: model, variant, level (0..2 a model's level, 3 its impostor), snow
        size_t Bucket(int model, int variant, int lod, int snow) const { return ((static_cast<size_t>(model) * 4 + variant) * 4 + lod) * 2 + snow; }
        void Gather(const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection, float reach, bool shadow);

        bool mReady = false;
        std::vector<Kind> mKinds;
        std::vector<Model> mModels;
        std::vector<Tree> mTrees;
        std::unordered_map<long long, Square> mSquares;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mImpostors;
        std::vector<std::vector<unsigned>> mBuckets;                // tree indices by bucket, this frame
        std::vector<DoorwaysDMeshInstanceGpuData> mInstances;
        std::vector<long long> mFarSquares;                         // squares drawn merged this frame
    };
}
