#pragma once

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

        Count
    };

    // Procedurally synthesized retro sound effects on SDL3 audio streams.
    // If no audio device is available (headless), every call is a no-op.
    class Audio
    {
    public:
        static bool Init();
        static void Shutdown();

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
