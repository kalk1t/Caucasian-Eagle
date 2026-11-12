#pragma once

// What grows beside the road, and the clouds over it.
//
//   Grass   tufts of blades on the land beside the road, thicker or thinner by region, none on the road,
//           in water, on steep rock or inside a city. Built in 20 m squares around the camera as she moves.
//   Trees   broadleaf and conifer trees made here (a trunk with branches, sprays of leaves), standing
//           5 to 140 m from the road's edge where the region has woods.
//   Clouds  a layer of cloud that drifts across the night sky, in front of the moon.
//
// Where things stand is decided by a rule (a hash of the place), not stored: the same tuft and the same
// tree are in the same place every run. The height of the land comes from
// assets/Compiled/World/Terrain.bin, written by art/world/build_nature.py together with the textures.
// Nothing here can be touched or collided with: Nyxara stays on the road.
// The numbers are in GameConstants.h ("nature") and in the region table at the top of Nature.cpp.

#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>
#include <vector>

class Renderer;

namespace CaucasianEagle
{
    class Nature
    {
    public:
        struct Clearing { float X = 0, Z = 0, Radius = 0; };   // nothing grows here (a city)

        bool Initialize(Renderer& renderer, std::string& error);
        void SetClearings(std::vector<Clearing> clearings) { mClearings = std::move(clearings); }
        void Update(float realDeltaTime) { mTime += realDeltaTime; }

        // The land beside the road. Outside the map the nearest edge is returned.
        float Height(float x, float z) const;
        float Beyond(float x, float z) const;   // metres beyond the road's edge (0 on the road, at most 255)
        int Region(float x, float z) const;     // 0..11
        // Moves (x, z) out of the trunk of any tree it stands in. 'radius' is the walker's own.
        void PushFromTrees(float& x, float& z, float radius) const;

        void Draw(Renderer& renderer, DirectX::XMFLOAT3 eye, const DirectX::XMMATRIX& viewProjection);
        void DrawClouds(Renderer& renderer, DirectX::XMFLOAT3 eye, const DirectX::XMMATRIX& viewProjection) const;

    private:
        struct Cell { Doorways::DoorwaysGpuMesh Mesh; bool Empty = true; int Region = 0; unsigned Used = 0; };
        struct Tree { Doorways::DoorwaysGpuMesh Trunk, Leaves; };

        bool BuildCell(Renderer& renderer, int cx, int cz, Cell& cell);
        bool BuildTrees(Renderer& renderer, std::string& error);
        bool BuildClouds(Renderer& renderer, std::string& error);
        bool Grows(float x, float z, int region) const;   // not road, water, chasm, cliff or city
        // The tree of one TreeCell square, if one stands there: where, how tall (not yet faded by distance) and how wide.
        bool TreeAt(int cx, int cz, float& x, float& z, int& region, float& tall, float& wide) const;
        float Sample(const std::vector<float>& field, float x, float z) const;

        bool mReady = false;
        float mTime = 0;
        unsigned mFrame = 0;

        int mNx = 0, mNz = 0;
        float mX0 = 0, mZ0 = 0, mGrid = 4;
        std::vector<float> mHeight, mBeyond;
        std::vector<unsigned char> mRegion;
        std::vector<Clearing> mClearings;

        std::unordered_map<long long, Cell> mCells;
        std::vector<Tree> mBroadleaf, mConifer;
        std::vector<Doorways::DoorwaysGpuMesh> mCloudBands;   // rings of the cloud dome, from the horizon up
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mGrassTexture, mLeafTexture, mBarkTexture, mCloudTexture;
    };

    Nature& GetNature();
}
