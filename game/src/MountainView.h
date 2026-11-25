#pragma once

// The whole mountain, looked at from outside (owner, 2026-11-18: "M button to be able to open whole round mountain ...
// and able to rotate and explore whole map"). M opens and closes it; the game stands still while it is open.
//   mouse          turns it round and tilts it
//   mouse wheel    closer and further
// Nothing else for now (owner, 2026-11-18: "only be able to rotate the map with his mouse for now"; places she can
// teleport to through the map come later). The point looked at stays the mountain's middle.
// The engine's own camera goes on reading the mouse (yaw, pitch) and the wheel (follow distance); this takes what it
// added each frame and sets the camera itself, far out. Used by The Eagle's Ascent (EaglesAscentGame.cpp) and the Map
// Lab (testing/MapLab). A test: CAUCASIAN_EAGLE_MOUNTAIN_VIEW=1 opens it at the start and turns it by itself.
// The Eagle's Ascent goes over it with the pointer instead (Explore; owner, 2026-11-21: "3D mountain is good but can we
// keep the controls? to look over all map around"): the controls of the detailed map, in 3D.
//   left drag              the land goes with the pointer: anywhere over the whole mountain
//   right drag             turns round and tilts
//   wheel, + and -         closer (down to 40 m off the ground) and further (the whole mountain)
//   W A S D, arrows        go over it; Q and E turn round
//   Space                  to her

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <DirectXMath.h>
#include "Camera.h"
#include "DoorwaysGame.h"
#include "DoorwaysGamepad.h"
#include "DoorwaysUiFontMetrics.h"
#include "GameUi.h"
#include <string>
#include <vector>

namespace CaucasianEagle
{
    struct MountainView
    {
        bool Open = false;
        bool Explored = false;   // gone over with the pointer (Explore): its words say so
        float Yaw = 0, Pitch = -.45f, Dist = 0;
        DirectX::XMFLOAT3 Focus{};
        DirectX::XMFLOAT3 Eye{};   // where Explore last put the camera (the engine's own camera moves it back each frame before)

        static bool Tried()
        {
            static const bool on = [] { char v[8]{}; size_t n = 0; return getenv_s(&n, v, "CAUCASIAN_EAGLE_MOUNTAIN_VIEW") == 0 && n > 1 && v[0] == '1'; }();
            return on;
        }
        // centre: the mountain's middle on the ground; radius: how far its land reaches from there; summit: its height
        void Show(Doorways::DoorwaysGameSession& session, Camera& camera, const DirectX::XMFLOAT3& centre, float radius, float summit)
        {
            Open = true;
            mCentre = centre; mRadius = (std::max)(radius, 100.f); mSummit = (std::max)(summit, 100.f);
            mMinPitch = session.CameraMinPitch; mMaxPitch = session.CameraMaxPitch;
            mFarZ = camera.FarZ; mFollow = camera.FollowDistance; mYaw = camera.Yaw; mPitch = camera.Pitch;
            session.CameraMinPitch = -1.5f; session.CameraMaxPitch = .5f;
            Yaw = camera.Yaw; Pitch = -.45f;
            Focus = {centre.x, centre.y + mSummit * .42f, centre.z};
            Dist = Home();
            camera.Pitch = Pitch; mSetYaw = Yaw; mSetPitch = Pitch;
            // the wheel is read from what the engine does to its follow distance: set in the middle of its range, so
            // the wheel has room both ways
            mWheel = (camera.MinFollowDistance + camera.MaxFollowDistance) * .5f; camera.FollowDistance = mWheel;
        }
        void Hide(Doorways::DoorwaysGameSession& session, Camera& camera)
        {
            Open = false;
            session.CameraMinPitch = mMinPitch; session.CameraMaxPitch = mMaxPitch;
            camera.FarZ = mFarZ; camera.FollowDistance = mFollow; camera.Yaw = mYaw; camera.Pitch = mPitch;
        }
        // Every frame while it is open, after the engine's camera: 'her' is where she stands (H goes there).
        void Update(Camera& camera, const DirectX::XMFLOAT3& her, float realDt, bool keys)
        {
            using namespace Doorways;
            (void)keys; (void)her;
            // what the engine's camera added since it was set: the mouse
            Yaw += camera.Yaw - mSetYaw; Pitch += camera.Pitch - mSetPitch;
            if (mWheel > .1f && camera.FollowDistance > .1f) Dist *= std::pow(camera.FollowDistance / mWheel, 1.5f);   // the wheel
            camera.FollowDistance = mWheel;
            if (Tried()) Yaw += .35f * realDt;
            Pitch = std::clamp(Pitch, -1.5f, .5f);
            Dist = std::clamp(Dist, Home() * .25f, Home() * 2.5f);
            const float fx = std::sin(Yaw), fz = std::cos(Yaw);
            const float c = std::cos(Pitch), sn = std::sin(Pitch);
            camera.Position = {Focus.x - fx * c * Dist, Focus.y - sn * Dist, Focus.z - fz * c * Dist};
            camera.Yaw = Yaw; camera.Pitch = Pitch; camera.ViewPitchOffset = 0;
            camera.FarZ = (std::max)(mFarZ, Dist + mRadius * 4);
            mSetYaw = Yaw; mSetPitch = Pitch;
        }

        // Each frame while it is open, for the Eagle's Ascent: the pointer free (x < 0: none), its buttons held, the wheel
        // (notches), the keys held, Space pressed; 'land' the ground's height under a place. Sets the camera.
        struct Input { float X = -1, Y = -1; bool Left = false, Right = false; float Wheel = 0, KeyX = 0, KeyZ = 0, Turn = 0, Zoom = 0; bool Home = false; };
        template <class Land>
        void Explore(Camera& camera, const Input& in, const DirectX::XMFLOAT3& her, float realDt, float screenH, const Land& land)
        {
            const float dt = (std::min)(realDt, .1f), h = (std::max)(screenH, 100.f);
            const float perPixel = 2 * Dist * std::tan(camera.FieldOfViewY * .5f) / h;   // metres a pixel at the point looked at
            const float fx = std::sin(Yaw), fz = std::cos(Yaw);                        // forward over the ground; right is (fz, -fx)
            const bool over = in.X >= 0;
            if (over && (in.Left || in.Right) && mHeld)
            {
                const float dx = in.X - mLastX, dy = in.Y - mLastY;
                if (in.Left)
                {   // the land under the pointer goes with it (seen from above, a step up the screen is a longer way on the ground)
                    const float deep = perPixel / (std::max)(.35f, std::sin(-Pitch));
                    Focus.x += -fz * dx * perPixel + fx * dy * deep;
                    Focus.z += fx * dx * perPixel + fz * dy * deep;
                }
                else { Yaw += dx * 3.4f / h; Pitch = std::clamp(Pitch - dy * 2.2f / h, -1.5f, -.08f); }
            }
            mHeld = over && (in.Left || in.Right); mLastX = in.X; mLastY = in.Y;
            const float notches = in.Wheel + in.Zoom * dt * 4;
            if (notches != 0) Dist *= std::pow(.85f, notches);
            if (in.KeyX != 0 || in.KeyZ != 0)
            {
                const float speed = Dist * .9f * dt;
                Focus.x += (fz * in.KeyX + fx * in.KeyZ) * speed; Focus.z += (-fx * in.KeyX + fz * in.KeyZ) * speed;
            }
            Yaw += in.Turn * 1.4f * dt;
            if (in.Home) { Focus.x = her.x; Focus.z = her.z; Dist = (std::min)(Dist, 260.f); }
            Dist = std::clamp(Dist, 40.f, Home() * 2.5f);
            // (the point looked at stays over the mountain and its sea; its height: the land there when close, the
            // mountain's middle when the whole of it is in view)
            const float ox = Focus.x - mCentre.x, oz = Focus.z - mCentre.z, off = std::hypot(ox, oz), most = mRadius * 1.3f;
            if (off > most) { Focus.x = mCentre.x + ox * most / off; Focus.z = mCentre.z + oz * most / off; }
            const float close = std::clamp((Home() - Dist) / (Home() - 40.f), 0.f, 1.f);
            const float want = (std::max)(0.f, static_cast<float>(land(Focus.x, Focus.z))) * close + (mCentre.y + mSummit * .42f) * (1 - close);
            Focus.y += (want - Focus.y) * (std::min)(1.f, dt * 6);
            const float c = std::cos(Pitch), sn = std::sin(Pitch);
            DirectX::XMFLOAT3 at{Focus.x - std::sin(Yaw) * c * Dist, Focus.y - sn * Dist, Focus.z - std::cos(Yaw) * c * Dist};
            const float ground = static_cast<float>(land(at.x, at.z)) + 6;
            float pitch = Pitch;
            if (at.y < ground)
            {   // never in the rock: up over it, still looking at the point
                at.y = ground;
                pitch = -std::atan2(at.y - Focus.y, std::hypot(Focus.x - at.x, Focus.z - at.z));
            }
            camera.Position = at; camera.Yaw = Yaw; camera.Pitch = pitch; camera.ViewPitchOffset = 0; Eye = at;
            camera.FarZ = (std::max)(mFarZ, Dist + mRadius * 4);
            mSetYaw = Yaw; mSetPitch = pitch;
        }

        // Opens it over a trail (the mountain's middle and reach are the trail's), or closes it.
        void Toggle(Doorways::DoorwaysGameSession& session, Camera& camera, const std::vector<DirectX::XMFLOAT3>& trail, float summit)
        {
            if (Open) { Hide(session, camera); return; }
            if (trail.empty()) return;
            float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f, y0 = 1e9f;
            for (const auto& t : trail) { x0 = (std::min)(x0, t.x); x1 = (std::max)(x1, t.x); z0 = (std::min)(z0, t.z); z1 = (std::max)(z1, t.z); y0 = (std::min)(y0, t.y); }
            Show(session, camera, {(x0 + x1) * .5f, y0, (z0 + z1) * .5f}, (std::max)(x1 - x0, z1 - z0) * .5f * 1.25f, summit);
        }
        // The words over it, and where she is.
        // A place marked on the mountain (the Eagle's Ascent: its Eagle Nests, bosses and creatures); no label: a dot.
        struct Mark { DirectX::XMFLOAT3 P{}; std::wstring Label; Color Tint; float Size = 6; };
        void DrawHud(Ui& ui, const Camera& camera, const DirectX::XMFLOAT3& her, float summit, const std::vector<Mark>& marks = {}) const
        {
            using namespace DirectX;
            const float s = std::clamp(ui.S, .8f, 1.5f), big = 20 * s / Doorways::DoorwaysUiFontHeight, body = 13 * s / Doorways::DoorwaysUiFontHeight;
            const float labelSize = 11 * s / Doorways::DoorwaysUiFontHeight;
            ui.Rect(0, 0, ui.W, 64 * s, {.015f, .025f, .04f, .7f});
            ui.Centered(L"THE MOUNTAIN", ui.W * .5f, 8 * s, big, Palette::Gold);
            ui.Centered(Explored ? L"Drag: go over it   |   Right drag: turn   |   Wheel: closer, further   |   WASD: move, Q E: turn   |   Space: to her   |   M: close"
                                 : L"Mouse: turn the mountain   |   Wheel: closer, further   |   M: close",
                        ui.W * .5f, 38 * s, body, Palette::Ink);
            const XMMATRIX view = camera.BuildViewProjectionMatrix(ui.W / ui.H);
            for (const auto& m : marks)
            {
                XMFLOAT4 at; XMStoreFloat4(&at, XMVector4Transform(XMVectorSet(m.P.x, m.P.y + 2, m.P.z, 1), view));
                if (at.w <= 0 || std::abs(at.x / at.w) > 1 || std::abs(at.y / at.w) > 1) continue;
                const float x = (at.x / at.w + 1) * ui.W * .5f, y = (1 - at.y / at.w) * ui.H * .5f, r = m.Size * .5f * s;
                ui.Rect(x - r - 1 * s, y - r - 1 * s, 2 * r + 2 * s, 2 * r + 2 * s, {0, 0, 0, .55f});
                ui.Rect(x - r, y - r, 2 * r, 2 * r, m.Tint);
                if (!m.Label.empty()) ui.Centered(m.Label, x, y - r - 16 * s, labelSize, m.Tint);
            }
            XMFLOAT4 c; XMStoreFloat4(&c, XMVector4Transform(XMVectorSet(her.x, her.y + 2, her.z, 1), view));
            if (c.w > 0 && std::abs(c.x / c.w) <= 1 && std::abs(c.y / c.w) <= 1)
            {
                const float x = (c.x / c.w + 1) * ui.W * .5f, y = (1 - c.y / c.w) * ui.H * .5f;
                ui.Rect(x - 4 * s, y - 4 * s, 8 * s, 8 * s, Palette::Soul);
                ui.Centered(L"NYXARA  " + std::to_wstring(static_cast<int>((std::max)(0.f, her.y))) + L" m of " + std::to_wstring(static_cast<int>(summit)) + L" m", x, y - 24 * s, body, Palette::Soul);
            }
        }

    private:
        float Home() const { return (std::max)(mRadius * 2.5f, mSummit * 2.2f); }   // all of it in view
        DirectX::XMFLOAT3 mCentre{};
        bool mHeld = false; float mLastX = 0, mLastY = 0;
        float mRadius = 0, mSummit = 0, mMinPitch = 0, mMaxPitch = 0, mFarZ = 0, mFollow = 0, mYaw = 0, mPitch = 0, mSetYaw = 0, mSetPitch = 0, mWheel = 0;
    };
}
