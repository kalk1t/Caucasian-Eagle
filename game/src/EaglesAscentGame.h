#pragma once

#include "DoorwaysGame.h"
#include "EaglesAscent.h"
#include "MountainView.h"

#include <array>
#include <fstream>
#include <string>

// The Eagle's Ascent: Map A on its own, another way into the game than the road (CaucasianEagle.exe --eagles-ascent).
// Nyxara climbs from the sea to Amiran's court at 4,200 m, on foot and in flight, with the road's controls, start screen,
// settings, sound and HUD. No journey, no monsters, no saves: checkpoints on the mountain bring her back after a fall.
// The map's rules are EaglesAscent (EaglesAscent.h), the same code Map Lab tests.
namespace CaucasianEagle
{
    // The switch or the environment variable asks for the Eagle's Ascent (main.cpp registers the module).
    bool EaglesAscentRequested();

    class EaglesAscentGame final : public Doorways::IDoorwaysGame
    {
    public:
        void Configure(Doorways::DoorwaysGameSession& session) override;
        bool Prepare(Doorways::DoorwaysGameHost& host, std::string& error) override;
        float ScaleFrameTime(float realDeltaTime, bool inputAllowed) override;
        void Update(Doorways::DoorwaysGameHost& host, float deltaTime, float realDeltaTime, bool inputAllowed) override;
        void Draw(Renderer& renderer, const WorldState& world, const DirectX::XMMATRIX& viewProjection) override;
        void AdjustEnvironment(EnvironmentSettings& environment) override;
        bool DrawHud(const DoorwaysHudRenderContext& context, HudTextRenderer& text) override;
        void DrawStartScreen(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused) override;

    private:
        bool Press(int key, bool allowed);
        void Notify(const std::wstring& title, const std::wstring& line, float seconds);
        void Smoke(Doorways::DoorwaysGameHost& host, float realDeltaTime);
        void Climb(Doorways::DoorwaysGameHost& host, float dt);
        void Evidence(const WorldState& world, const char* event);

        EaglesAscent mMap;
        std::array<bool, 256> mKeys{};
        bool mPaused = false, mFlying = false, mWasDead = false;
        MountainView mView;                        // M: the whole mountain from outside (MountainView.h)
        float mDeadFor = 0, mRespawnAsk = 0, mClock = 0;
        std::wstring mNoticeTitle, mNoticeLine; float mNotice = 0;
        int mReached = 0;                          // the highest station she has stood at
        // the smoke test (CAUCASIAN_EAGLE_DEMO=1): every station stood on, then a take-off
        bool mSmoke = false; int mSmokeStation = -1, mSmokeBad = 0; float mSmokeClock = 0, mSmokeY = 0;
        // the whole climb (CAUCASIAN_EAGLE_DEMO=2): on foot up the trail, every stretch only wings cross flown with the keys
        bool mClimb = false; size_t mClimbFly = 0; int mClimbPhase = 0, mClimbNext = 1; float mClimbBest = 0, mClimbStuck = 0, mClimbPhaseClock = 0, mClimbLastY = 0; bool mClimbFell = false;
        std::string mLog;
    };
}
