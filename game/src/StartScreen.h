#pragma once

// The game's own start screen (StartScreen.cpp): the rendered picture, a menu in Nyxara's panel style,
// the guide and the credits, and the line that says who made the game and the engine.

struct DoorwaysHudRenderContext;
class HudTextRenderer;

namespace CaucasianEagle::StartScreen
{
    void Draw(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused);
}
