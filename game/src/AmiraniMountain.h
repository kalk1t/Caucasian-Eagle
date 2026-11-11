#pragma once

#include "Rendering/DoorwaysGpuMesh.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <string>

class Renderer;
struct EnvironmentSettings;

// The landmark the whole game is played in sight of: the mountain where Amirani is chained,
// with the eagle at his liver and his dog at the chain, under the Caucasus night sky.
//
// Scenery: its only walkable part is the ledge (collision comes with the RoadToAmirani scene). Numbers are in GameConstants.h
// ("night sky", "mountain of Amirani"). Assets: assets/Compiled/Landmarks/AmiraniMountain
// (built by art/mountain) and assets/textures/caucasus (built by art/sky).
//
// The sky dome and the moon are drawn here, not by the engine: the engine lights its sky
// panels like scenery, which washes out a painted sky. The engine's own sky and moon are
// switched off in StyleEnvironment.
namespace CaucasianEagle
{
    class AmiraniMountain
    {
    public:
        bool Initialize(Renderer& renderer, std::string& error);
        void Update(float realDeltaTime);
        void Draw(Renderer& renderer, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewProjection) const;

        void SetEagleAway(bool away) { mEagleAway = away; }   // he has left the wound to fight his daughter
        void SetFreed(bool freed) { mFreed = freed; }          // the chain is open: no eagle, no chains

        // Moonlight and haze of the Caucasus night (IDoorwaysGame::AdjustEnvironment).
        static void StyleEnvironment(EnvironmentSettings& environment);

    private:
        struct Model
        {
            Doorways::DoorwaysGpuMesh Mesh;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture;
            DirectX::XMFLOAT3 Min{}, Max{}; // bounds of the compiled mesh
        };
        bool Load(Renderer& renderer, const char* name, Model& out, std::string& error);
        bool BuildSky(Renderer& renderer, std::string& error);
        void DrawSky(Renderer& renderer, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewProjection) const;
        // Mesh placed with its feet centre at 'feet', scaled so its height is 'height', turned by 'yaw'.
        static DirectX::XMMATRIX Place(const Model& model, float height, float yaw, DirectX::XMFLOAT3 feet,
                                       float pitch = 0);

        Model mMountain, mAmirani, mEagle, mDog;
        Doorways::DoorwaysGpuMesh mDome, mMoon, mQuad;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSkyTexture, mMoonTexture, mGlowTexture;
        float mMountainYaw = 0; // set at load so the cave ledge faces the play area
        float mTime = 0;
        bool mReady = false;
        bool mEagleAway = false, mFreed = false;
    };

    AmiraniMountain& GetAmiraniMountain();
}
