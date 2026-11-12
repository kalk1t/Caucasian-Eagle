#pragma once

// The sounds of Caucasian Eagle.
//
// Every sound is a WAV file under game/assets/sounds (PCM 16-bit). A sound is asked for by
// its folder and name without the number and extension: "combat/hit" plays one of
// combat/hit_1.wav, combat/hit_2.wav, ... chosen at random. A name with no file is silent,
// so a sound is added or replaced by putting a file into the folder; no code changes.
// game/assets/sounds/README.md lists every name the game asks for and when.
//
// The engine plays the files (Doorways::GameAudio); this decides which and when.

namespace CaucasianEagle::Sound
{
    void Initialize();   // lists the sound folder; call once after the engine started

    // One-shot. 'vary' is how far the pitch may wander (0 = always as recorded).
    void Play(const char* name, float volume = 1, float vary = .06f);
    void PlayUi(const char* name, float volume = 1);   // on the Interface volume

    // What is heard all the time. Set every frame; Update fades towards it.
    struct Scene
    {
        const char* Music = nullptr;   // "music/road", "music/battle", ... nullptr = none
        float Wind = 0;                // 0..1 open country
        float City = 0;                // 0..1 inside a city
        float Fire = 0;                // 0..1 near the walls of fire of a fight
        float Flight = 0;              // 0..1 she is in the air
    };
    void Update(float realDt, const Scene& scene);
    // Starts a piece at once, at full level (the start screen: the game is not updated while the menu shows).
    void StartMusic(const char* name);

    // The storyteller. One line at a time; a new line cuts the one being spoken.
    void Narrate(const char* name);    // "voice/chapter_01"
    void StopNarration();
    bool Narrating();

    // Footsteps: call every frame with how far she moved on the ground this frame.
    void Walk(float metres);
}
