#include "Audio.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace Atlas
{
    namespace
    {
        constexpr int SampleRate = 44100;
        constexpr int StreamPoolSize = 10;

        bool g_Available = false;
        SDL_AudioStream* g_Streams[StreamPoolSize] = {};
        std::vector<float> g_Buffers[static_cast<int>(Sfx::Count)];

        std::uint32_t g_Noise = 0x1234567u;

        // The digger/shovel retrigger their sound roughly 10 times a
        // second while held, so a few slightly different takes are kept
        // and rotated between - otherwise the exact same buffer repeating
        // reads as a mechanical, grating tick rather than a tool at work.
        constexpr int DigVariantCount = 3;
        std::vector<float> g_DigVariants[DigVariantCount];
        int g_DigVariantIndex = 0;

        float NoiseSample()
        {
            g_Noise ^= g_Noise << 13;
            g_Noise ^= g_Noise >> 17;
            g_Noise ^= g_Noise << 5;

            return static_cast<float>(g_Noise & 0xFFFF) / 32768.0f - 1.0f;
        }

        // --- Tiny synth helpers -----------------------------------------

        std::vector<float> Synth(float seconds)
        {
            return std::vector<float>(
                static_cast<std::size_t>(seconds * SampleRate),
                0.0f);
        }

        void AddNoiseBurst(
            std::vector<float>& buffer,
            float startSeconds,
            float lengthSeconds,
            float amplitude,
            float decayPower,
            int lowpassPasses,
            float attackSeconds = 0.0f)
        {
            const std::size_t start =
                static_cast<std::size_t>(startSeconds * SampleRate);
            const std::size_t length =
                static_cast<std::size_t>(lengthSeconds * SampleRate);

            std::vector<float> noise(length);

            for (std::size_t i = 0; i < length; i++)
            {
                const float t =
                    static_cast<float>(i) / static_cast<float>(length);

                noise[i] = NoiseSample() *
                    amplitude * std::pow(1.0f - t, decayPower);
            }

            // A hard instant onset reads as a harsh tick/click; a short
            // ramp-in softens the attack into more of a "poof".
            if (attackSeconds > 0.0f)
            {
                const std::size_t attackSamples = std::min(
                    length,
                    static_cast<std::size_t>(attackSeconds * SampleRate));

                for (std::size_t i = 0; i < attackSamples; i++)
                {
                    noise[i] *= static_cast<float>(i) /
                        static_cast<float>(attackSamples);
                }
            }

            // Cheap lowpass: repeated neighbor averaging. Each pass
            // attenuates high frequencies further, so more passes trade
            // harsh hiss for a duller, rounder thump.
            for (int pass = 0; pass < lowpassPasses; pass++)
            {
                float previous = 0.0f;

                for (std::size_t i = 0; i < length; i++)
                {
                    const float current = noise[i];
                    noise[i] = (current + previous) * 0.5f;
                    previous = current;
                }
            }

            for (std::size_t i = 0; i < length && start + i < buffer.size(); i++)
                buffer[start + i] += noise[i];
        }

        void AddTone(
            std::vector<float>& buffer,
            float startSeconds,
            float lengthSeconds,
            float startHz,
            float endHz,
            float amplitude,
            bool square)
        {
            const std::size_t start =
                static_cast<std::size_t>(startSeconds * SampleRate);
            const std::size_t length =
                static_cast<std::size_t>(lengthSeconds * SampleRate);

            float phase = 0.0f;

            for (std::size_t i = 0; i < length && start + i < buffer.size(); i++)
            {
                const float t =
                    static_cast<float>(i) / static_cast<float>(length);

                const float hz = startHz + (endHz - startHz) * t;

                phase += hz / static_cast<float>(SampleRate);

                float sample = std::sin(phase * 6.28318f);

                if (square)
                    sample = sample > 0.0f ? 1.0f : -1.0f;

                buffer[start + i] +=
                    sample * amplitude * (1.0f - t);
            }
        }

        void Normalize(std::vector<float>& buffer, float peak)
        {
            float maxSample = 0.0001f;

            for (float sample : buffer)
                maxSample = std::max(maxSample, std::fabs(sample));

            const float scale = peak / maxSample;

            for (float& sample : buffer)
                sample *= scale;
        }

        void BuildAllSfx()
        {
            // Shot: sharp mid noise crack.
            {
                std::vector<float> b = Synth(0.16f);
                AddNoiseBurst(b, 0.0f, 0.14f, 1.0f, 3.0f, 2);
                AddTone(b, 0.0f, 0.05f, 420.0f, 160.0f, 0.5f, false);
                Normalize(b, 0.55f);
                g_Buffers[static_cast<int>(Sfx::Shot)] = b;
            }

            // Shotgun: deeper, longer boom.
            {
                std::vector<float> b = Synth(0.3f);
                AddNoiseBurst(b, 0.0f, 0.28f, 1.0f, 2.4f, 5);
                AddTone(b, 0.0f, 0.1f, 180.0f, 70.0f, 0.8f, false);
                Normalize(b, 0.65f);
                g_Buffers[static_cast<int>(Sfx::Shotgun)] = b;
            }

            // Rifle: loud crack with a tail.
            {
                std::vector<float> b = Synth(0.4f);
                AddNoiseBurst(b, 0.0f, 0.1f, 1.0f, 2.0f, 1);
                AddNoiseBurst(b, 0.06f, 0.3f, 0.4f, 2.0f, 6);
                AddTone(b, 0.0f, 0.06f, 600.0f, 200.0f, 0.5f, false);
                Normalize(b, 0.6f);
                g_Buffers[static_cast<int>(Sfx::Rifle)] = b;
            }

            // Dig: soft, dull crumble rather than a harsh static crackle -
            // a gentle attack (no hard click), heavy lowpassing (rounds
            // off the hiss into more of a thump), and a faint low thud
            // underneath for a tactile "biting into material" feel.
            // Built as a few slightly different takes (see
            // g_DigVariants) so rapid retriggering doesn't sound like
            // the exact same click looping.
            for (int variant = 0; variant < DigVariantCount; variant++)
            {
                std::vector<float> b = Synth(0.14f);
                AddNoiseBurst(b, 0.0f, 0.13f, 0.5f, 2.6f, 18, 0.008f);
                AddTone(b, 0.0f, 0.05f, 95.0f, 65.0f, 0.16f, false);
                Normalize(b, 0.22f);
                g_DigVariants[variant] = b;
            }

            g_Buffers[static_cast<int>(Sfx::Dig)] = g_DigVariants[0];

            // Explosion: long rumble.
            {
                std::vector<float> b = Synth(0.9f);
                AddNoiseBurst(b, 0.0f, 0.25f, 1.0f, 1.6f, 2);
                AddNoiseBurst(b, 0.05f, 0.8f, 0.9f, 2.0f, 8);
                AddTone(b, 0.0f, 0.5f, 120.0f, 36.0f, 1.0f, false);
                Normalize(b, 0.75f);
                g_Buffers[static_cast<int>(Sfx::Explosion)] = b;
            }

            // Jump: quick upward blip.
            {
                std::vector<float> b = Synth(0.12f);
                AddTone(b, 0.0f, 0.12f, 220.0f, 460.0f, 0.6f, true);
                Normalize(b, 0.25f);
                g_Buffers[static_cast<int>(Sfx::Jump)] = b;
            }

            // Land: dull thud.
            {
                std::vector<float> b = Synth(0.14f);
                AddNoiseBurst(b, 0.0f, 0.12f, 0.8f, 2.4f, 8);
                AddTone(b, 0.0f, 0.08f, 130.0f, 60.0f, 0.7f, false);
                Normalize(b, 0.35f);
                g_Buffers[static_cast<int>(Sfx::Land)] = b;
            }

            // Hurt: descending square yelp.
            {
                std::vector<float> b = Synth(0.16f);
                AddTone(b, 0.0f, 0.16f, 520.0f, 180.0f, 0.6f, true);
                Normalize(b, 0.3f);
                g_Buffers[static_cast<int>(Sfx::Hurt)] = b;
            }

            // Gib: wet splat.
            {
                std::vector<float> b = Synth(0.3f);
                AddNoiseBurst(b, 0.0f, 0.28f, 1.0f, 1.8f, 10);
                AddTone(b, 0.0f, 0.1f, 200.0f, 60.0f, 0.5f, false);
                Normalize(b, 0.5f);
                g_Buffers[static_cast<int>(Sfx::Gib)] = b;
            }

            // Throw: short whoosh.
            {
                std::vector<float> b = Synth(0.14f);
                AddNoiseBurst(b, 0.0f, 0.14f, 0.6f, 1.2f, 12);
                Normalize(b, 0.22f);
                g_Buffers[static_cast<int>(Sfx::Throw)] = b;
            }

            // Reload: two clicks.
            {
                std::vector<float> b = Synth(0.24f);
                AddNoiseBurst(b, 0.0f, 0.03f, 0.8f, 1.0f, 1);
                AddNoiseBurst(b, 0.15f, 0.04f, 0.9f, 1.0f, 1);
                Normalize(b, 0.3f);
                g_Buffers[static_cast<int>(Sfx::Reload)] = b;
            }

            // Jet burst: short rocket hiss (retriggered while thrusting).
            {
                std::vector<float> b = Synth(0.2f);
                AddNoiseBurst(b, 0.0f, 0.2f, 0.8f, 0.8f, 4);
                AddTone(b, 0.0f, 0.2f, 90.0f, 70.0f, 0.4f, false);
                Normalize(b, 0.2f);
                g_Buffers[static_cast<int>(Sfx::JetBurst)] = b;
            }
        }
    }

    bool Audio::Init()
    {
        if (g_Available)
            return true;

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            return false;

        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = SampleRate;

        for (int i = 0; i < StreamPoolSize; i++)
        {
            g_Streams[i] = SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                &spec,
                nullptr,
                nullptr);

            if (!g_Streams[i])
            {
                // No device: tear down what we made and disable audio.
                for (int j = 0; j < i; j++)
                {
                    SDL_DestroyAudioStream(g_Streams[j]);
                    g_Streams[j] = nullptr;
                }

                return false;
            }

            SDL_ResumeAudioStreamDevice(g_Streams[i]);
        }

        BuildAllSfx();

        g_Available = true;
        return true;
    }

    void Audio::Shutdown()
    {
        for (int i = 0; i < StreamPoolSize; i++)
        {
            if (g_Streams[i])
            {
                SDL_DestroyAudioStream(g_Streams[i]);
                g_Streams[i] = nullptr;
            }
        }

        g_Available = false;
    }

    void Audio::Play(Sfx sfx, float volume)
    {
        if (!g_Available)
            return;

        const std::vector<float>* bufferPtr =
            &g_Buffers[static_cast<int>(sfx)];

        if (sfx == Sfx::Dig)
        {
            g_DigVariantIndex = (g_DigVariantIndex + 1) % DigVariantCount;
            bufferPtr = &g_DigVariants[g_DigVariantIndex];
        }

        const std::vector<float>& buffer = *bufferPtr;

        if (buffer.empty())
            return;

        // Pick an idle stream; give up silently if all are busy.
        SDL_AudioStream* stream = nullptr;

        for (int i = 0; i < StreamPoolSize; i++)
        {
            if (SDL_GetAudioStreamQueued(g_Streams[i]) == 0)
            {
                stream = g_Streams[i];
                break;
            }
        }

        if (!stream)
            return;

        if (volume >= 0.99f)
        {
            SDL_PutAudioStreamData(
                stream,
                buffer.data(),
                static_cast<int>(buffer.size() * sizeof(float)));
            return;
        }

        std::vector<float> scaled(buffer);

        for (float& sample : scaled)
            sample *= volume;

        SDL_PutAudioStreamData(
            stream,
            scaled.data(),
            static_cast<int>(scaled.size() * sizeof(float)));
    }

    bool Audio::IsAvailable()
    {
        return g_Available;
    }
}
