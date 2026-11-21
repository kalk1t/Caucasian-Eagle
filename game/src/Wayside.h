#pragma once

// What lies beside the Eagle's Ascent's roads (game 0.5, the owner, 2026-11-20): clay jars.
//
// Clay jars: Georgia's old craft of clay (the qvevri, the wine vessel kept since the eighth millennium BC, the doki jug,
// the kotso pot, the kvibri). In every road (the way, the caves and the rings round the mountain) every 30 to 50 m a
// group of 3 to 5 of them stands beside it. Her blades break them (NyxaraKit's blow listener: Strike); what is inside
// shows then: crystals, a healing potion, or nothing.
// (The hidden places, 73 cairns with a gold light that paid 8 crystals each, were taken away on 2026-11-21: the owner,
// "i want those hidden places to be removed fully".)
// Every jar has an id that is the same every time the map is laid out (its road and its place along it): the Eagle's
// Ascent's save keeps the broken ones.

#include "Rendering/DoorwaysGpuMesh.h"

#include <d3d11.h>

#include <DirectXMath.h>
#include <string>
#include <vector>

struct DoorwaysRuntimeScene;
struct WorldState;
class Renderer;

namespace CaucasianEagle
{
    class EaglesAscent;

    class Wayside
    {
    public:
        struct Jar
        {
            std::string Id;
            DirectX::XMFLOAT3 P{};
            float Yaw = 0, Size = 1, Tint = 0;
            int Shape = 0;            // 0 qvevri, 1 doki, 2 kotso, 3 kvibri
            int Crystals = 0;         // what is in it
            bool Potion = false;
            bool Broken = false;
        };
        struct Gain
        {
            int Crystals = 0, Potions = 0, Jars = 0;
            DirectX::XMFLOAT3 At{};
        };

        bool Load(Renderer& renderer, std::string& error);      // the jars' meshes
        bool Laid() const { return mLaid; }
        // the jars along every road (needs the map's ground: once its scene is up)
        void Lay(const DoorwaysRuntimeScene& scene, const EaglesAscent& map);
        // her blow from 'from', facing yaw, reaching reach m within arc degrees of her facing: the jars it breaks
        Gain Strike(const DirectX::XMFLOAT3& from, float yaw, float reach, float arcDegrees);
        void Update(float dt);
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection, ID3D11ShaderResourceView* glowTexture);
        // a light for this frame (her growth's lights on her wings, blades and armour), drawn with the rest
        void Glow(const DirectX::XMFLOAT3& at, float size, const DirectX::XMFLOAT4& color) { mGlows.push_back({at, size, color}); }

        std::vector<std::string> BrokenIds() const;
        void Restore(const std::vector<std::string>& broken);   // as the save has them
        const std::vector<Jar>& Jars() const { return mJars; }

    private:
        struct Shard { DirectX::XMFLOAT3 P{}, V{}; float Ground = 0, Spin = 0, Size = .1f, Life = 0, Tint = 0; bool Resting = false; };
        struct GlowLight { DirectX::XMFLOAT3 At{}; float Size = 1; DirectX::XMFLOAT4 Color{}; };
        std::vector<Jar> mJars;
        std::vector<Shard> mShards;
        std::vector<GlowLight> mGlows;
        Doorways::DoorwaysGpuMesh mShapes[4], mBands[4], mQuad;
        bool mMeshes = false, mLaid = false;
        float mTime = 0;
    };
}
