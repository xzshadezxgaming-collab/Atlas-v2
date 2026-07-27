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

        static bool IsAvailable();
    };
}
