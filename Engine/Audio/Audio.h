#pragma once

#include <string>

namespace Atlas
{
    enum class Sfx
    {
        Shot,
        Shotgun,
        Rifle,
        Dig,
        Explosion,
        Jump,
        Land,
        Hurt,
        Gib,
        Throw,
        Reload,
        JetBurst,
        Sniper,
        Rocket,
        Flame,
        DroneDown,
        UiClick,
        SquadOrder,
        WeaponSwitch,
        BodySwitch,
        Heal,
        Pickup,
        Steam,
        Alarm,
        OrderSent,
        Ship,
        Victory,
        Defeat,

        Count
    };

    // Sound effects on SDL3 audio streams. Init synthesizes a retro set;
    // LoadSoundPack then replaces it with recorded takes where the pack
    // has them. If no audio device is available (headless), every call
    // is a no-op.
    class Audio
    {
    public:
        static bool Init();
        static void Shutdown();

        // Loads <folder>/<name>.wav or <name>_1.wav, <name>_2.wav, ... for
        // each effect (names in Audio.cpp); an effect with several takes
        // plays a different one each time. Returns how many takes loaded.
        static int LoadSoundPack(const std::string& folder);

        // The effect's file name in the sound pack ("shot", "uiclick").
        static const char* GetSoundName(Sfx sfx);

        // pan: -1 hard left .. +1 hard right.
        static void Play(Sfx sfx, float volume = 1.0f, float pan = 0.0f);

        // Scales every sound played from now on (0..1).
        static void SetMasterVolume(float volume);

        // Procedural soundtrack: a calm loop and a combat loop over the
        // same chords, crossfaded by intensity (0 calm .. 1 fighting).
        // Call UpdateMusic every frame while it should play.
        static void SetMusicVolume(float volume);
        static void UpdateMusic(float intensity);
        static void StopMusic();

        static bool IsAvailable();
    };
}
