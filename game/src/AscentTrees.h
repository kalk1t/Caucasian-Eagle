#pragma once

// The Eagle's Ascent's trees as real 3D models (0.5.2, 2026-11-20; the owner: "improve trees, create its real 3D models,
// refine them, their branches and leaves, their roots"; 2026-11-21: "i want all trees to be 3D models ... i dont need
// trees which are not 3D").
//
// The map builder lists every tree of the map in <map>.plants (testing/MapLab/build_mountain.py, GAME_TREES: about
// 300 000, one line each: "model variant x y z height yaw leanx leanz snow"). The models are art/trees/build_trees.py's
// (game/assets/Trees: trees.txt, Models/<model>_<variant>_<lod>_{bark,leaves}.dmesh, Textures/): grown branch by branch
// with roots, bark and sprays of leaves, at four levels of detail, all four 3D.
//
// Drawn by how large a tree looks ('seen': its distance over its radius): level 0 (every twig and spray) close, 1, 2 and
// 3 (the trunk, its limbs, a few clusters) further, every one a 3D model, until the tree is about 25 pixels tall
// (Picture); only beyond that is it its picture (an impostor: crossed quads, art/trees/impostors.py). Every model,
// variant and level is one instanced draw. The trees of a square are in two groups by size, and those of a block
// (4 x 4 squares) in one: a group whose every tree is a picture is merged into one mesh, built once (nearest first, a
// few thousand trees a frame), one draw; a block of pictures the same. So the whole mountain's forest is drawn from
// anywhere (the far forest used to be baked into the map). The trees within ShadowReach cast the sun's shadow
// (Renderer::DrawDoorwaysGpuSubmeshShadowInstanced, through the game's shadow hook). Their trunks hold her
// (<map>.trees, EaglesAscent::HoldOffTrees).
// The ground's plants are models of the same library (2026-11-21; the owner: "Fallen logs, tomatoes, cucumbers, ferns and
// flowers are still built into the map - i want also them to be 3D"; art/trees/plants.py): fallen logs, tomatoes on their
// stakes, cucumbers, ferns, clumps of meadow flowers. They have no picture: trees.txt gives each variant's reach from its
// foot and its top ("extent"; a log lies, its height there its length), they are 3D until they are a few pixels and
// then not drawn, and the small ones cast shadows only within PlantShadowReach. The logs and the stakes hold her too.
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
        static constexpr int BlockSquares = 4;            // a block: 4 x 4 squares, merged whole when all its trees are pictures
        static constexpr float Detail[3] = {30.f, 85.f, 230.f};   // 'seen': level 0 within the first, 1 within the second, 2 within the third
        static constexpr float Picture = 900.f;          // 'seen': level 3 (3D) within it; beyond, the picture (about 25 px tall at 1080 lines)
        static constexpr float BigReach = 14.f;          // a square's trees of a radius over this (clamped, 8..30 m) are its second group
        static constexpr float ShadowReach = 150.f;      // m: trees this near cast shadows
        static constexpr float PlantShadowReach = 60.f;  // m: a small ground plant (radius under 3 m) casts one this near (further, a texel or two)
        static constexpr int BuildTreesPerFrame = 12000; // trees merged into far meshes a frame (one group at least)
        static constexpr unsigned KeepFrames = 1800;     // a merged mesh not drawn for this many frames is let go

        // dataFile: the map's data file (<map>.txt); its .plants beside it. No .plants or no library: no trees, and true.
        // False only when the library is there but broken.
        bool Initialize(Renderer& renderer, const std::filesystem::path& dataFile, std::string& error);
        void Draw(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection);
        // Her chest, this frame (on: the camera follows her). A tree whose trunk or crown stands between the camera and
        // her, or that the camera is in, is drawn see-through (a screen door, SeeThroughDoor of its pixels left out), so
        // that she is never lost behind one (owner 2026-11-22: "examine camera with and around trees").
        void SetFocus(const DirectX::XMFLOAT3& chest, bool on) { mFocus = chest; mFocusOn = on; }
        static constexpr float SeeThroughDoor = .7f;
        int SeeThrough = 0;   // last frame: trees between the camera and her (drawn see-through unless SeeThroughOff)
        bool SeeThroughOff = false;   // (tests: CAUCASIAN_EAGLE_CAMERA_OLD=1, the trees as they were, to compare)
        // would this tree hide what is at 'to' from 'eye' (its trunk or crown on the line, short of 'to' by 'shortOf' m)
        bool Hides(unsigned tree, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& to, float shortOf) const;
        void DrawShadows(Renderer& renderer, const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& lightViewProjection);
        bool On() const { return mReady && !mTrees.empty(); }
        size_t Count() const { return mTrees.size(); }
        // last frame: trees drawn at each level (0..3: 3D; 4: pictures, single and merged), instanced draws, merged
        // groups drawn, trees in merged meshes held now
        int Drawn[5] = {}, Draws = 0, FarGroups = 0, Shadowed = 0;
        long long Merged = 0;
        double GatherMs = 0, NearMs = 0, FarMs = 0; int Built = 0;   // last frame: where the CPU's time went, trees merged
        std::string Report() const;

    private:
        struct Part { Doorways::DoorwaysGpuMesh Mesh; Doorways::DoorwaysDMeshSubmesh Sub{}; bool Valid = false; };
        struct Impostor { float U0 = 0, V0 = 0, U1 = 1, V1 = 1, W = 1, H = 1; Doorways::DoorwaysGpuMesh Mesh; bool Valid = false; };
        struct Variant
        {
            float Height = 1;
            float Spread = 0, Top = 0;                   // a ground plant's (no picture): its reach from its foot and its top, model metres
            std::array<std::array<Part, 2>, 4> Lod;      // [level 0..3][bark, leaves]
            Impostor Far[2];                             // [no snow, snow]
        };
        struct Model { std::string Name; int Kind = 0; bool Conifer = false, Snow = false, Ground = false; std::vector<Variant> Variants; };   // Ground: a plant (its extent listed)
        struct Kind
        {
            std::string Name;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Bark, BarkNormal, Leaves, LeavesSnow;
        };
        struct Tree
        {
            DirectX::XMFLOAT4X4 World, WorldInvTranspose;
            DirectX::XMFLOAT3 Centre;   // of its crown, for culling and distance
            float Radius = 1, Scale = 1, Reach = 8;   // Reach: the radius clamped to 8..30 m, by which it is 'seen'
            DirectX::XMFLOAT3 Foot{}; float Tall = 0, Wide = 0, LeanX = 0, LeanZ = 0;   // its trunk's line and crown's width (Hides)
            unsigned short Model = 0; unsigned char Variant = 0, Snow = 0;
        };
        // trees drawn or merged together: one group of a square, or all of a block
        struct Group
        {
            std::vector<unsigned> Trees;
            DirectX::XMFLOAT3 Min{1e9f, 1e9f, 1e9f}, Max{-1e9f, -1e9f, -1e9f};
            float Reach = 8;                           // the largest Reach among its trees
            bool Pictures = false;                     // any of its trees has a picture (a group of plants only is never merged)
            Doorways::DoorwaysGpuMesh FarMesh; int FarTrees = 0; bool FarBuilt = false; unsigned LastDrawn = 0;
        };
        struct Block { int All = -1; std::vector<int> Parts; };   // Parts: the groups of its squares
        bool LoadLibrary(Renderer& renderer, const std::filesystem::path& folder, std::string& error);
        void LoadPlants(const std::filesystem::path& file);
        bool BuildImpostor(Renderer& renderer, Impostor& imp);
        void BuildFar(Renderer& renderer, Group& group);
        // which bucket: model, variant, level (0..3 a model's level, 4 its picture), snow
        size_t Bucket(int model, int variant, int lod, int snow) const { return ((static_cast<size_t>(model) * 4 + variant) * 5 + lod) * 2 + snow; }
        void Gather(const DirectX::XMFLOAT3& eye, const DirectX::XMMATRIX& viewProjection, bool shadow);

        bool mReady = false;
        std::vector<Kind> mKinds;
        std::vector<Model> mModels;
        std::vector<Tree> mTrees;
        std::vector<Group> mGroups;
        std::unordered_map<long long, Block> mBlocks;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mImpostors;
        std::vector<std::vector<unsigned>> mBuckets;                // tree indices by bucket, this frame
        std::vector<std::vector<unsigned>> mDoorBuckets;            // ...and those drawn see-through (between the camera and her)
        DirectX::XMFLOAT3 mFocus{}; bool mFocusOn = false;
        std::vector<DoorwaysDMeshInstanceGpuData> mInstances;
        std::vector<std::pair<float, int>> mFar;                   // groups drawn merged this frame, by distance
        unsigned mFrame = 0;
    };
}
