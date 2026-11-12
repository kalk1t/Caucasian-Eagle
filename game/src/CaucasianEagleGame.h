#pragma once

#include "DoorwaysGame.h"

// The Caucasian Eagle game module registered with the Doorways engine (see main.cpp).
namespace CaucasianEagle
{
    class Game final : public Doorways::IDoorwaysGame
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
    };
}
