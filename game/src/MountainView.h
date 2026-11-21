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
        float Yaw = 0, Pitch = -.45f, Dist = 0;
        DirectX::XMFLOAT3 Focus{};

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
            camera.Yaw = Yaw; camera.Pitch = Pitch;
            camera.FarZ = (std::max)(mFarZ, Dist + mRadius * 4);
            mSetYaw = Yaw; mSetPitch = Pitch;
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
            ui.Centered(L"Mouse: turn the mountain   |   Wheel: closer, further   |   Tab: the map   |   M: close",
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
        float mRadius = 0, mSummit = 0, mMinPitch = 0, mMaxPitch = 0, mFarZ = 0, mFollow = 0, mYaw = 0, mPitch = 0, mSetYaw = 0, mSetPitch = 0, mWheel = 0;
    };
}
