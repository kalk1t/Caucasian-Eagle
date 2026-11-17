#pragma once

// The game's own start screen (StartScreen.cpp): the rendered picture, a menu in Nyxara's panel style,
// the guide and the credits, and the line that says who made the game and the engine.

struct DoorwaysHudRenderContext;
class HudTextRenderer;

namespace CaucasianEagle::StartScreen
{
    void Draw(const DoorwaysHudRenderContext& context, HudTextRenderer& text, bool focused);
    // Another way into the game than the road (the Eagle's Ascent): the words under the title and on the first row.
    // Not called: the road's own words.
    void SetPlay(const wchar_t* subtitle, const wchar_t* label, const wchar_t* note);
}
