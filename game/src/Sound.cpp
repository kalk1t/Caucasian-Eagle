#include "Sound.h"
#include "GameConstants.h"
#include "DoorwaysGameAudio.h"
#include "DoorwaysRuntimePathResolver.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace CaucasianEagle::Sound
{
    namespace
    {
        // Slots of the engine's game audio: two for music (one fades out while the other fades in),
        // the rest one each.
        enum Slot : size_t { MusicA, MusicB, WindSlot, PlaceSlot, VoiceSlot, FlightSlot };

        std::unordered_map<std::string, std::vector<std::string>> gFiles;   // "combat/hit" -> its files
        std::unordered_map<std::string, int> gLast;                          // the variant played last, not repeated
        std::string gMusic[2];          // what each music slot holds
        float gMusicVolume[2] = {0, 0};
        int gMusicNow = 0;              // the slot that is fading in
        std::unordered_map<std::string, float> gPlayedAt;                    // when each name was last played (gClock)
        float gClock = 0;
        float gWind = 0, gPlace = 0, gFlight = 0, gDuck = 1, gStride = 0;
        std::string gPlaceName;

        const std::string* Pick(const char* name)
        {
            const auto found = gFiles.find(name);
            if (found == gFiles.end() || found->second.empty()) return nullptr;
            const auto& files = found->second;
            int index = std::rand() % static_cast<int>(files.size());
            int& last = gLast[name];
            if (files.size() > 1 && index == last) index = (index + 1) % static_cast<int>(files.size());
            last = index;
            return &files[index];
        }
        float Towards(float value, float target, float step)
        { return value < target ? (std::min)(target, value + step) : (std::max)(target, value - step); }
    }

    void Initialize()
    {
        gFiles.clear();
        const auto root = Doorways::ResolveDoorwaysRuntimePath(SoundFolder);
        std::error_code error;
        if (root.empty() || !std::filesystem::is_directory(root, error)) return;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".wav") continue;
            const auto relative = std::filesystem::relative(entry.path(), root, error).generic_string();
            std::string name = relative.substr(0, relative.size() - 4);
            // "combat/hit_2" is a variant of "combat/hit"
            const auto underscore = name.find_last_of('_');
            if (underscore != std::string::npos && underscore + 1 < name.size() &&
                std::all_of(name.begin() + underscore + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }) &&
                name.compare(0, 6, "voice/") != 0)   // spoken lines are numbered, not varied
                name.resize(underscore);
            gFiles[name].push_back(std::string(SoundFolder) + "/" + relative);
        }
        for (auto& [name, files] : gFiles) std::sort(files.begin(), files.end());
    }

    void Play(const char* name, float volume, float vary)
    {
        // Several hits on the same frame are one sound, not a pile of them.
        float& last = gPlayedAt.try_emplace(name, -1.f).first->second;
        if (gClock - last < SoundRepeatSeconds) return;
        last = gClock;
        if (const auto* file = Pick(name))
            Doorways::GameAudio().Play(*file, volume, 1 + vary * (static_cast<float>(std::rand() % 2001) / 1000.f - 1));
    }
    void PlayUi(const char* name, float volume)
    {
        if (const auto* file = Pick(name)) Doorways::GameAudio().Play(*file, volume, 1, Doorways::DoorwaysAudioChannel::Interface);
    }

    void Narrate(const char* name)
    {
        const auto* file = Pick(name);
        if (file) Doorways::GameAudio().Say(VoiceSlot, *file, NarrationVolume);
        else Doorways::GameAudio().Stop(VoiceSlot);
    }
    void StopNarration() { Doorways::GameAudio().Stop(VoiceSlot); }
    bool Narrating() { return Doorways::GameAudio().Playing(VoiceSlot); }

    void Walk(float metres)
    {
        if (metres <= 0) { gStride = (std::min)(gStride, StepLength * .5f); return; }
        gStride += metres;
        if (gStride < StepLength) return;
        gStride = 0;
        Play("nyxara/step", StepVolume, .1f);
    }

    void StartMusic(const char* name)
    {
        const auto found = gFiles.find(name);
        if (found == gFiles.end() || found->second.empty()) return;
        gMusicNow = 0; gMusic[0] = found->second.front(); gMusicVolume[0] = 1;
        gMusic[1].clear(); gMusicVolume[1] = 0;
        Doorways::GameAudio().Stop(MusicB);
        Doorways::GameAudio().Loop(MusicA, gMusic[0], MusicVolume);
    }

    void Update(float dt, const Scene& scene)
    {
        auto& audio = Doorways::GameAudio();
        dt = (std::min)(dt, .1f);
        gClock += dt;

        // The storyteller is heard over everything else: music and the world step back.
        gDuck = Towards(gDuck, Narrating() ? NarrationDuck : 1.f, dt * 1.5f);

        // Music: the wanted piece fades in on one slot while the other fades out.
        std::string file;
        if (scene.Music)
            if (const auto found = gFiles.find(scene.Music); found != gFiles.end() && !found->second.empty()) file = found->second.front();
        if (gMusic[gMusicNow] != file)
        {
            if (gMusic[1 - gMusicNow] == file) gMusicNow = 1 - gMusicNow;          // back to the piece still fading out (or to silence)
            else if (gMusicVolume[1 - gMusicNow] <= .001f) { gMusicNow = 1 - gMusicNow; gMusic[gMusicNow] = file; }
            // else: wait until the other slot has faded out
        }
        for (int i = 0; i < 2; ++i)
        {
            const bool in = i == gMusicNow && gMusic[i] == file && !file.empty();
            gMusicVolume[i] = Towards(gMusicVolume[i], in ? 1.f : 0.f, dt / MusicFadeSeconds);
            if (gMusic[i].empty() || (!in && gMusicVolume[i] <= 0)) { audio.Stop(i == 0 ? MusicA : MusicB); gMusic[i].clear(); }
            else audio.Loop(i == 0 ? MusicA : MusicB, gMusic[i], gMusicVolume[i] * MusicVolume * gDuck);
        }

        // The world: wind in open country, the place she is in (a city, or the fire of a fight), the air in flight.
        gWind = Towards(gWind, scene.Wind, dt * .6f);
        if (const auto* wind = Pick("world/wind"); wind && gWind > .001f) audio.Loop(WindSlot, *wind, gWind * WindVolume * gDuck, Doorways::DoorwaysAudioChannel::Effects);
        else audio.Stop(WindSlot);

        const char* place = scene.Fire > scene.City ? "world/fire" : "world/city";
        const float placeLevel = (std::max)(scene.Fire, scene.City);
        if (gPlaceName != place && gPlace > .001f) gPlace = Towards(gPlace, 0, dt * 1.2f);   // fade the old place out first
        else { gPlaceName = place; gPlace = Towards(gPlace, placeLevel, dt * .8f); }
        if (const auto* loop = Pick(gPlaceName.c_str()); loop && gPlace > .001f) audio.Loop(PlaceSlot, *loop, gPlace * PlaceVolume * gDuck, Doorways::DoorwaysAudioChannel::Effects);
        else audio.Stop(PlaceSlot);

        gFlight = Towards(gFlight, scene.Flight, dt * 2.5f);
        if (const auto* air = Pick("nyxara/flight"); air && gFlight > .001f) audio.Loop(FlightSlot, *air, gFlight * FlightVolume * gDuck, Doorways::DoorwaysAudioChannel::Effects);
        else audio.Stop(FlightSlot);
    }
}
