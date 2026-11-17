#include <windows.h>

#include "CaucasianEagleGame.h"
#include "EaglesAscentGame.h"
#include "DoorwaysApplication.h"

#include <algorithm>
#include <cctype>
#include <string>

// Caucasian Eagle entry point: registers the game with the Doorways engine and runs it: the road, or with
// --eagles-ascent (CAUCASIAN_EAGLE_ASCENT=1) the Eagle's Ascent on its own (EaglesAscentGame.h).
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, PSTR commandLine, int showCommand)
{
    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(showCommand);
    static CaucasianEagle::Game game;
    static CaucasianEagle::EaglesAscentGame ascent;
    if (CaucasianEagle::EaglesAscentRequested()) Doorways::RegisterDoorwaysGame(&ascent);
    else Doorways::RegisterDoorwaysGame(&game);
    // the engine refuses words it does not know on its command line: the game's own switch is taken out first
    std::string line = commandLine ? commandLine : "";
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (size_t at; (at = lower.find("--eagles-ascent")) != std::string::npos;) { line.erase(at, 15); lower.erase(at, 15); }
    line.erase(0, line.find_first_not_of(' ') == std::string::npos ? line.size() : line.find_first_not_of(' '));
    while (!line.empty() && line.back() == ' ') line.pop_back();
    return Doorways::RunDoorwaysApplication(instance, line.data());
}
