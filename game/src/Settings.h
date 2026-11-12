#pragma once

// The game's own settings window (Settings.cpp), in the same panel, gold and ink as Nyxara's other windows.
// It shows only what this game uses: the window, shadows, the four volumes, the mouse, the size of the
// interface and the frame counter. The engine's settings screens are not used (DoorwaysGameSession::OwnSettings):
// the values still belong to the engine, which applies and saves them; this window reads them from
// GameSession().Settings and asks for changes there.

namespace CaucasianEagle
{
    class Ui;
}

namespace CaucasianEagle::Settings
{
    void Open();     // from the start screen or from her menu (Esc)
    void Close();
    bool IsOpen();
    void Back();     // Esc in play: the engine reports it to the game, the game passes it on here

    // Draws the window and reads its keys; call every frame. 'focused': the game window is in front.
    // 'startScreen': the pointer is free and Esc is read here (in play the game calls Back instead).
    void Draw(Ui& ui, bool focused, bool startScreen);
}
