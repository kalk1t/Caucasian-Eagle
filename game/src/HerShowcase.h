#pragma once

// Nyxara shown in her window (the Eagle's Ascent's I, 0.5.2; the owner 2026-11-21: "let player rotate and look at nyxara
// while Inventory is on, so we need nyxara appears with front view and user is able to choose her look and check her
// around with mouse click"). While the window is open the camera stands before her, facing her, between its two panels:
//   drag (left button, away from the cards)   turns the view round her and up and down; let go, it runs on and slows
//   mouse wheel                                closer and further
// The engine's camera is left as it was and given back when the window closes (its follow camera then takes her up again).
// The game stands still meanwhile; her pose goes on in real time (NyxaraKit::SetShowcase), so she stands and breathes.

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <functional>
#include "Camera.h"

namespace CaucasianEagle
{
    struct HerShowcase
    {
        bool On = false;
        float Turn = 0, Tilt = -.10f, Dist = 3.f;         // radians round her from her front, up and down; metres
        float TurnGo = 0;                                  // radians a second it still turns after a drag
        bool Dragging = false;

        static constexpr float NearDist = 1.6f, FarDist = 7.f, HomeDist = 3.9f;

        void Begin(Camera& camera)
        {
            On = true; Dragging = false;
            mYaw = camera.Yaw; mPitch = camera.Pitch; mFollow = camera.FollowDistance; mNearZ = camera.NearZ;
            Turn = 0; Tilt = -.08f;   /* (straight before her: the owner, "front view") */ Dist = HomeDist; TurnGo = 0; mShownTurn = Turn; mShownTilt = Tilt; mShownDist = Dist;
            mFresh = true; mRoom = -1;
        }
        void End(Camera& camera)
        {
            if (!On) return;
            On = false; Dragging = false;
            camera.Yaw = mYaw; camera.Pitch = mPitch; camera.FollowDistance = mFollow; camera.NearZ = mNearZ;
        }
        // the pointer: pressed where nothing else takes it (grab), moved while held (screen pixels), let go
        void Grab(float x, float y) { Dragging = true; mLastX = x; mLastY = y; TurnGo = 0; }
        void Drag(float x, float y, float realDt, float screenHeight)
        {
            if (!Dragging) return;
            const float per = 3.4f / (std::max)(400.f, screenHeight);   // radians a pixel: a screen's height is over half a turn
            const float dx = x - mLastX, dy = y - mLastY;
            Turn -= dx * per; Tilt = std::clamp(Tilt - dy * per * .6f, -.75f, .32f);
            if (realDt > 1e-4f) TurnGo += (-dx * per / realDt - TurnGo) * (std::min)(1.f, realDt * 18);
            mLastX = x; mLastY = y;
        }
        void Release() { Dragging = false; TurnGo = std::clamp(TurnGo, -9.f, 9.f); }
        void Wheel(float notches) { Dist = std::clamp(Dist * std::pow(.88f, notches), NearDist, FarDist); }

        // Every frame while it is open, after the engine's camera: her feet and the way she faces, how tall her picture is
        // (1: her own), the land's height (the camera is kept over it), and how far from her the camera may stand on its line
        // before something is in the way (the scene's collision: a cliff beside a nest; a negative: nothing in the way)
        void Place(Camera& camera, const DirectX::XMFLOAT3& feet, float herYaw, float tall, float realDt, const std::function<float(float, float)>& land,
                   const std::function<float(const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&)>& clear = {})
        {
            if (!On) return;
            if (!Dragging && std::abs(TurnGo) > 1e-3f)
            {   // run on after a drag, slowing
                Turn += TurnGo * realDt;
                TurnGo *= std::exp(-realDt * 3.2f);
            }
            const float ease = mFresh ? 1.f : (std::min)(1.f, realDt * 14);
            mShownTurn += (Turn - mShownTurn) * ease; mShownTilt += (Tilt - mShownTilt) * ease; mShownDist += (Dist - mShownDist) * (std::min)(1.f, realDt * 9);
            if (mFresh) mShownDist = Dist;
            mFresh = false;
            // looked at: her middle, a little higher the closer it comes (her face and shoulders close up)
            const float h = 1.79f * tall, closeUp = std::clamp((HomeDist - mShownDist) / (HomeDist - NearDist), 0.f, 1.f);
            const DirectX::XMFLOAT3 focus{feet.x, feet.y + h * (.50f + .32f * closeUp), feet.z};
            const float yaw = herYaw + DirectX::XM_PI + mShownTurn;   // the camera looks along yaw: from her front, at her
            const float fx = std::sin(yaw), fz = std::cos(yaw), c = std::cos(mShownTilt), sn = std::sin(mShownTilt);
            float dist = mShownDist;
            if (clear)
            {   // rock in the way: the camera comes in front of it at once, and goes back out slowly when it is gone
                const float room = clear(focus, {focus.x - fx * c * dist, focus.y - sn * dist, focus.z - fz * c * dist});
                const float target = room >= 0 ? room : dist;
                mRoom = mRoom < 0 || target < mRoom ? target : mRoom + (target - mRoom) * (std::min)(1.f, realDt * 4);
                dist = (std::max)(.45f, (std::min)(dist, mRoom));
            }
            DirectX::XMFLOAT3 at{focus.x - fx * c * dist, focus.y - sn * dist, focus.z - fz * c * dist};
            if (land)
            {
                const float ground = land(at.x, at.z);
                if (ground > -1e8f && at.y < ground + .35f) at.y = ground + .35f;   // (never under the ground)
            }
            camera.Position = at; camera.ViewPitchOffset = 0;   // (the follow camera's lift over a slope is not this view's)
            camera.Yaw = yaw;
            camera.Pitch = std::atan2(focus.y - at.y, std::hypot(focus.x - at.x, focus.z - at.z));
            camera.NearZ = (std::min)(mNearZ, .05f);
        }

    private:
        float mYaw = 0, mPitch = 0, mFollow = 0, mNearZ = .1f, mLastX = 0, mLastY = 0;
        float mShownTurn = 0, mShownTilt = 0, mShownDist = 0, mRoom = -1;
        bool mFresh = true;
    };
}
