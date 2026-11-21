#pragma once

// The Eagle's Ascent's detailed map: the M key (0.5.2; the owner 2026-11-21: "make Detailed Map for M and let user explore
// whole map while M is open with mouse").
//
// The whole round mountain from above, north up: a picture of it made from the map's own data (art/map/build_ascent_map.py:
// its heights, the ground's paint, every tree, the sea, relief and contour lines), and over it, drawn sharp at any zoom,
// the way up, the rings round the mountain, the caves, the Eagle Nests, the bosses, the creatures still waiting, and
// Nyxara with the way she faces. The game stands still while it is open.
//   drag (left button)      moves the map; let go, it runs on and slows
//   mouse wheel, + and -    closer and further, about the pointer
//   W A S D, arrows         move the map
//   Space                   back to her
//   pointer over a mark     what it is
// Its picture: assets/ui/Map/AscentMap.jpg, and AscentMap.txt (the picture's extent in metres).

#include "GameUi.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <functional>
#include <string>
#include <vector>
#include <wrl/client.h>

class Renderer;

namespace CaucasianEagle
{
    class AscentMap
    {
    public:
        // Loads the picture once. False (with 'error') when it is missing.
        bool Load(Renderer& renderer, std::string& error);
        bool Loaded() const { return mPicture != nullptr; }

        bool Open = false;
        void Show(const DirectX::XMFLOAT3& her);
        void Hide() { Open = false; mDragging = false; }

        // A place on the map. Kind picks its sign; Title and Line are what the pointer over it shows.
        enum Kind { Nest, NestSaved, Boss, BossBeaten, Group, CaveGroup, Summit, Start };
        struct Mark { DirectX::XMFLOAT3 P{}; Kind What = Group; std::wstring Title, Line; int Count = 0; };
        struct Data
        {
            const std::vector<DirectX::XMFLOAT3>* Trail = nullptr;                // the way up
            const std::vector<std::vector<DirectX::XMFLOAT3>>* Rings = nullptr;   // the trails round the mountain
            std::vector<std::vector<DirectX::XMFLOAT3>> Caves;                    // each cave's middle line
            std::vector<Mark> Marks;
            DirectX::XMFLOAT3 Her{}; float HerYaw = 0, LookYaw = 0; bool Flying = false;
            float Summit = 0;
            std::function<float(float, float)> Land;                             // the land's height (for the pointer)
            std::function<std::wstring(float)> ZoneAt;                           // the zone's name at a height
        };

        // Each frame while it is open: the pointer (x < 0: none), its button, a click, the wheel (notches), the keys
        // held (dx, dz: -1..1 each), Space pressed; the screen's size.
        void Update(float x, float y, bool down, bool click, float wheel, float keyX, float keyZ, float zoomKeys, bool home,
                    const DirectX::XMFLOAT3& her, float realDt, float screenW, float screenH, float S);
        void Draw(Ui& ui, const Data& data);
        // (for its test: the view, and where a world point is on the screen)
        float CentreX() const { return mCX; }
        float CentreZ() const { return mCZ; }
        float Scale() const { return mScale; }
        DirectX::XMFLOAT2 ScreenOf(float x, float z) const { return ToScreen(x, z); }

    private:
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mPicture;
        float mW = 0, mH = 0, mX0 = 0, mX1 = 0, mZ0 = 0, mZ1 = 0;   // its size in texels; its extent in metres
        // the view: the world point at the middle of the map's area and screen pixels a metre (shown, and wanted)
        float mCX = 0, mCZ = 0, mScale = .2f, mWantX = 0, mWantZ = 0, mWantScale = .2f;
        float mGoX = 0, mGoZ = 0;            // m a second it still moves after a drag
        bool mDragging = false; float mLastX = 0, mLastY = 0, mPointerX = -1, mPointerY = -1;
        float mAreaX = 0, mAreaY = 0, mAreaW = 1, mAreaH = 1;      // where the map is on the screen (beside its panel)
        float FitScale() const;
        float MostScale() const { return 3.5f; }
        void Clamp();
        DirectX::XMFLOAT2 ToScreen(float x, float z) const;
        DirectX::XMFLOAT2 ToWorld(float sx, float sy) const;
    };
}
