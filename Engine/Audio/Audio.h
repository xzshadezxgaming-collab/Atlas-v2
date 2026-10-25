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

        Count
    };

    // Procedurally synthesized retro sound effects on SDL3 audio streams.
    // If no audio device is available (headless), every call is a no-op.
    class Audio
    {
    public:
        static bool Init();
        static void Shutdown();

        static void Play(Sfx sfx, float volume = 1.0f);

        // Scales every sound played from now on (0..1).
        static void SetMasterVolume(float volume);

        static bool IsAvailable();
    };
}
