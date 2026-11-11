#include <windows.h>

#include "CaucasianEagleGame.h"
#include "DoorwaysApplication.h"

// Caucasian Eagle entry point: registers the game with the Doorways engine and runs it.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, PSTR commandLine, int showCommand)
{
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(showCommand);
    static CaucasianEagle::Game game;
    Doorways::RegisterDoorwaysGame(&game);
    return Doorways::RunDoorwaysApplication(instance, commandLine);
}
