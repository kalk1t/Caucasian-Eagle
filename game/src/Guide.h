#pragma once

// The guide: how to play, written out in full, and what the game needs to run.
// Opened from the start screen (Guide) and from her menu (Esc). Drawn in the same panel, gold and ink as
// her other windows. The words are in Guide.cpp; the player manual (docs/manual.md) says the same at length.

namespace CaucasianEagle
{
    class Ui;

    namespace Guide
    {
        constexpr int PageCount = 7;   // the last one says who made the game and the engine (the start screen's Credits opens it)
        void Draw(Ui& ui, int page, float fade);
    }
}
