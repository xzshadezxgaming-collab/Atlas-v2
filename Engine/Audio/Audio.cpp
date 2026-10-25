#include "Audio.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace Atlas
{
    namespace
    {
        constexpr int SampleRate = 44100;
        constexpr int StreamPoolSize = 24;

        bool g_Available = false;
        float g_MasterVolume = 1.0f;
        SDL_AudioStream* g_Streams[StreamPoolSize] = {};
        std::vector<float> g_Buffers[static_cast<int>(Sfx::Count)];

        std::uint32_t g_Noise = 0x1234567u;

        // Music: two loops of the same length, mixed on the fly.
        SDL_AudioStream* g_MusicStream = nullptr;
        std::vector<float> g_MusicCalm;
        std::vector<float> g_MusicCombat;
        std::size_t g_MusicCursor = 0;
        float g_MusicVolume = 0.6f;
        float g_MusicIntensity = 0.0f;
        bool g_MusicBuilt = false;

        // The digger/shovel retrigger their sound roughly 10 times a
        // second while held, so a few slightly different takes are kept
        // and rotated between - otherwise the exact same buffer repeating
        // reads as a mechanical, grating tick rather than a tool at work.
        constexpr int DigVariantCount = 3;
        std::vector<float> g_DigVariants[DigVariantCount];
        int g_DigVariantIndex = 0;

        // Sound pack file names, in Sfx order.
        constexpr const char* SfxNames[] =
        {
            "shot", "shotgun", "rifle", "dig", "explosion", "jump", "land",
            "hurt", "gib", "throw", "reload", "jetburst", "sniper", "rocket",
            "flame", "dronedown", "uiclick", "squadorder", "weaponswitch",
            "bodyswitch", "heal", "pickup", "steam", "alarm", "ordersent",
            "ship", "victory", "defeat",
        };

        static_assert(std::size(SfxNames) == static_cast<std::size_t>(Sfx::Count),
            "every Sfx needs a sound pack name");

        // Takes loaded from the sound pack; an effect with none plays its
        // synthesized buffer instead.
        std::vector<std::vector<float>> g_Takes[static_cast<int>(Sfx::Count)];
        int g_LastTake[static_cast<int>(Sfx::Count)] = {};
        std::uint32_t g_TakeRng = 0x9e3779b9u;

        // Effects added with the sound pack borrow an older effect's
        // synthesized sound when there is no pack (they used to play it).
        Sfx SynthFallback(Sfx sfx)
        {
            switch (sfx)
            {
            case Sfx::UiClick:
            case Sfx::SquadOrder:
            case Sfx::WeaponSwitch:
            case Sfx::BodySwitch:
            case Sfx::Heal:
            case Sfx::Pickup:
            case Sfx::OrderSent:
                return Sfx::Reload;
            case Sfx::Steam:
                return Sfx::JetBurst;
            default:
                return sfx;
            }
        }

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

        // --- Music ---------------------------------------------------

        float MidiHz(int note)
        {
            return 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
        }

        enum class Wave { Sine, Saw, Square, Triangle };

        // Adds a note into a looping buffer (writes wrap around, so the
        // loop is seamless). Attack/release envelope, optional decay.
        void AddNote(
            std::vector<float>& loop,
            float startSeconds,
            float lengthSeconds,
            float hz,
            float amplitude,
            Wave wave,
            float attack,
            float release,
            float decayRate = 0.0f,
            float detune = 0.0f)
        {
            const std::size_t size = loop.size();
            const std::size_t start = static_cast<std::size_t>(startSeconds * SampleRate);
            const std::size_t length = static_cast<std::size_t>((lengthSeconds + release) * SampleRate);

            float phase = 0.0f;
            float phase2 = 0.37f;
            float low = 0.0f;

            for (std::size_t i = 0; i < length; i++)
            {
                const float t = static_cast<float>(i) / SampleRate;

                float env = 1.0f;

                if (t < attack)
                    env = t / attack;
                else if (t > lengthSeconds)
                    env = std::max(0.0f, 1.0f - (t - lengthSeconds) / std::max(0.001f, release));

                if (decayRate > 0.0f)
                    env *= std::exp(-t * decayRate);

                phase += hz / SampleRate;
                phase2 += hz * (1.0f + detune) / SampleRate;
                phase -= std::floor(phase);
                phase2 -= std::floor(phase2);

                auto shape = [&](float p)
                {
                    switch (wave)
                    {
                    case Wave::Saw: return 2.0f * p - 1.0f;
                    case Wave::Square: return p < 0.5f ? 1.0f : -1.0f;
                    case Wave::Triangle: return 4.0f * std::fabs(p - 0.5f) - 1.0f;
                    default: return std::sin(p * 6.28318f);
                    }
                };

                float sample = shape(phase);

                if (detune != 0.0f)
                    sample = 0.5f * (sample + shape(phase2));

                // Soften the buzzy waves.
                if (wave == Wave::Saw || wave == Wave::Square)
                {
                    low += (sample - low) * 0.08f;
                    sample = low;
                }

                loop[(start + i) % size] += sample * amplitude * env;
            }
        }

        void AddDrum(std::vector<float>& loop, float startSeconds, int kind, float amplitude)
        {
            const std::size_t size = loop.size();
            const std::size_t start = static_cast<std::size_t>(startSeconds * SampleRate);

            // 0 kick, 1 snare, 2 hat.
            const float length = kind == 0 ? 0.22f : kind == 1 ? 0.16f : 0.04f;
            const std::size_t samples = static_cast<std::size_t>(length * SampleRate);

            float phase = 0.0f;
            float low = 0.0f;
            float previous = 0.0f;

            for (std::size_t i = 0; i < samples; i++)
            {
                const float t = static_cast<float>(i) / SampleRate;
                const float fall = 1.0f - t / length;
                float sample = 0.0f;

                if (kind == 0)
                {
                    phase += (45.0f + 110.0f * std::exp(-t * 30.0f)) / SampleRate;
                    sample = std::sin(phase * 6.28318f) * fall * fall;
                }
                else if (kind == 1)
                {
                    phase += 190.0f / SampleRate;
                    const float noise = NoiseSample();
                    low += (noise - low) * 0.35f;
                    sample = (low * 0.8f + std::sin(phase * 6.28318f) * 0.4f) * fall * fall * fall;
                }
                else
                {
                    const float noise = NoiseSample();
                    sample = (noise - previous) * 0.5f * fall; // crude highpass
                    previous = noise;
                }

                loop[(start + i) % size] += sample * amplitude;
            }
        }

        void BuildMusic()
        {
            // A minor, 96 bpm, eight bars: Am F C G, two bars each.
            constexpr float Beat = 60.0f / 96.0f;
            constexpr float Bar = Beat * 4.0f;
            constexpr int Bars = 8;

            const std::size_t size = static_cast<std::size_t>(Bar * Bars * SampleRate);
            g_MusicCalm.assign(size, 0.0f);
            g_MusicCombat.assign(size, 0.0f);

            struct Chord { int Root; int Third; int Fifth; };
            const Chord chords[4] =
            {
                { 57, 60, 64 }, // Am
                { 53, 57, 60 }, // F
                { 48, 52, 55 }, // C
                { 55, 59, 62 }, // G
            };

            for (int c = 0; c < 4; c++)
            {
                const Chord& chord = chords[c];
                const float start = static_cast<float>(c) * Bar * 2.0f;
                const float length = Bar * 2.0f;
                const int tones[3] = { chord.Root, chord.Third, chord.Fifth };

                // Pad: detuned saws, slow swell, in both loops.
                for (int tone : tones)
                {
                    AddNote(g_MusicCalm, start, length - 0.3f, MidiHz(tone), 0.10f,
                        Wave::Saw, 0.9f, 0.9f, 0.0f, 0.004f);
                    AddNote(g_MusicCombat, start, length - 0.3f, MidiHz(tone), 0.06f,
                        Wave::Saw, 0.4f, 0.6f, 0.0f, 0.004f);
                }

                // Calm: a deep held root and a slow plucked arpeggio.
                AddNote(g_MusicCalm, start, length - 0.2f, MidiHz(chord.Root - 24), 0.16f,
                    Wave::Sine, 0.5f, 0.6f);

                const int arp[8] = { 0, 1, 2, 1, 0, 2, 1, 2 };

                for (int step = 0; step < 8; step++)
                {
                    if (step % 2 == 1 && c % 2 == 0)
                        continue;

                    const int note = tones[arp[step]] + 12;
                    AddNote(g_MusicCalm, start + static_cast<float>(step) * Beat, Beat * 0.5f,
                        MidiHz(note), 0.07f, Wave::Triangle, 0.005f, 0.6f, 4.0f);
                }

                // Combat: driving eighth-note bass, drums, faster arp.
                for (int eighth = 0; eighth < 16; eighth++)
                {
                    const float at = start + static_cast<float>(eighth) * Beat * 0.5f;
                    const int bassNote = chord.Root - 24 + ((eighth % 8 == 6) ? 7 : 0);

                    AddNote(g_MusicCombat, at, Beat * 0.35f, MidiHz(bassNote), 0.22f,
                        Wave::Square, 0.004f, 0.05f, 3.0f);

                    AddDrum(g_MusicCombat, at, 2, eighth % 2 == 0 ? 0.10f : 0.06f);

                    if (eighth % 4 == 0)
                        AddDrum(g_MusicCombat, at, 0, 0.55f);
                    else if (eighth % 4 == 2)
                        AddDrum(g_MusicCombat, at, 1, 0.32f);

                    if (eighth == 15)
                        AddDrum(g_MusicCombat, at + Beat * 0.25f, 0, 0.3f);
                }

                for (int sixteenth = 0; sixteenth < 32; sixteenth++)
                {
                    const int note = tones[(sixteenth * 2 + sixteenth / 4) % 3] + 12 + (sixteenth % 8 == 7 ? 12 : 0);
                    AddNote(g_MusicCombat, start + static_cast<float>(sixteenth) * Beat * 0.25f,
                        Beat * 0.12f, MidiHz(note), 0.045f, Wave::Square, 0.003f, 0.08f, 8.0f);
                }
            }

            Normalize(g_MusicCalm, 0.5f);
            Normalize(g_MusicCombat, 0.6f);
            g_MusicBuilt = true;
        }

        void BuildAllSfx()
        {
            // Shot: sharp mid noise crack. The ~2ms attack ramp keeps the
            // punch but removes the raw digital click at sample zero.
            {
                std::vector<float> b = Synth(0.16f);
                AddNoiseBurst(b, 0.0f, 0.14f, 1.0f, 3.0f, 2, 0.002f);
                AddTone(b, 0.0f, 0.05f, 420.0f, 160.0f, 0.5f, false);
                Normalize(b, 0.55f);
                g_Buffers[static_cast<int>(Sfx::Shot)] = b;
            }

            // Shotgun: deeper, longer boom.
            {
                std::vector<float> b = Synth(0.3f);
                AddNoiseBurst(b, 0.0f, 0.28f, 1.0f, 2.4f, 5, 0.003f);
                AddTone(b, 0.0f, 0.1f, 180.0f, 70.0f, 0.8f, false);
                Normalize(b, 0.65f);
                g_Buffers[static_cast<int>(Sfx::Shotgun)] = b;
            }

            // Rifle: loud crack with a tail.
            {
                std::vector<float> b = Synth(0.4f);
                AddNoiseBurst(b, 0.0f, 0.1f, 1.0f, 2.0f, 1, 0.002f);
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

            // Sniper: hard supersonic crack and a long rolling echo.
            {
                std::vector<float> b = Synth(0.8f);
                AddNoiseBurst(b, 0.0f, 0.06f, 1.0f, 1.6f, 0, 0.001f);
                AddTone(b, 0.0f, 0.05f, 1400.0f, 300.0f, 0.4f, false);
                AddNoiseBurst(b, 0.04f, 0.7f, 0.45f, 2.2f, 10);
                AddTone(b, 0.0f, 0.25f, 110.0f, 45.0f, 0.6f, false);
                Normalize(b, 0.7f);
                g_Buffers[static_cast<int>(Sfx::Sniper)] = b;
            }

            // Rocket: a thump and a rising motor whoosh.
            {
                std::vector<float> b = Synth(0.55f);
                AddNoiseBurst(b, 0.0f, 0.08f, 1.0f, 2.0f, 4, 0.002f);
                AddTone(b, 0.0f, 0.08f, 160.0f, 60.0f, 0.8f, false);
                AddNoiseBurst(b, 0.03f, 0.5f, 0.6f, 0.6f, 3, 0.05f);
                AddTone(b, 0.03f, 0.5f, 140.0f, 260.0f, 0.15f, true);
                Normalize(b, 0.55f);
                g_Buffers[static_cast<int>(Sfx::Rocket)] = b;
            }

            // Flame: a soft roaring hiss (retriggered while held).
            {
                std::vector<float> b = Synth(0.22f);
                AddNoiseBurst(b, 0.0f, 0.22f, 0.8f, 0.7f, 6, 0.03f);
                AddTone(b, 0.0f, 0.22f, 70.0f, 60.0f, 0.25f, false);
                Normalize(b, 0.22f);
                g_Buffers[static_cast<int>(Sfx::Flame)] = b;
            }

            // Drone down: falling whine into a crunch.
            {
                std::vector<float> b = Synth(0.45f);
                AddTone(b, 0.0f, 0.25f, 900.0f, 200.0f, 0.4f, true);
                AddNoiseBurst(b, 0.2f, 0.25f, 1.0f, 2.0f, 3);
                Normalize(b, 0.45f);
                g_Buffers[static_cast<int>(Sfx::DroneDown)] = b;
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
        spec.channels = 2;
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

        g_MusicStream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

        if (g_MusicStream)
            SDL_ResumeAudioStreamDevice(g_MusicStream);

        BuildAllSfx();
        BuildMusic();

        g_Available = true;
        return true;
    }

    int Audio::LoadSoundPack(const std::string& folder)
    {
        if (!g_Available)
            return 0;

        SDL_AudioSpec want;
        want.format = SDL_AUDIO_F32;
        want.channels = 1;
        want.freq = SampleRate;

        int loaded = 0;

        for (int i = 0; i < static_cast<int>(Sfx::Count); i++)
        {
            g_Takes[i].clear();

            const std::string base = folder + "/" + SfxNames[i];

            for (int take = 0; take <= 16; take++)
            {
                const std::string path = take == 0
                    ? base + ".wav"
                    : base + "_" + std::to_string(take) + ".wav";

                SDL_AudioSpec spec;
                Uint8* data = nullptr;
                Uint32 length = 0;

                if (!SDL_LoadWAV(path.c_str(), &spec, &data, &length))
                {
                    if (take > 0)
                        break;
                    continue;
                }

                Uint8* converted = nullptr;
                int convertedLength = 0;

                if (SDL_ConvertAudioSamples(&spec, data, static_cast<int>(length),
                    &want, &converted, &convertedLength))
                {
                    const float* samples = reinterpret_cast<const float*>(converted);
                    g_Takes[i].emplace_back(samples,
                        samples + convertedLength / static_cast<int>(sizeof(float)));
                    loaded++;
                    SDL_free(converted);
                }

                SDL_free(data);
            }
        }

        return loaded;
    }

    const char* Audio::GetSoundName(Sfx sfx)
    {
        const int index = static_cast<int>(sfx);
        return index >= 0 && index < static_cast<int>(Sfx::Count) ? SfxNames[index] : "";
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

        if (g_MusicStream)
        {
            SDL_DestroyAudioStream(g_MusicStream);
            g_MusicStream = nullptr;
        }

        g_Available = false;
    }

    void Audio::SetMasterVolume(float volume)
    {
        g_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    void Audio::SetMusicVolume(float volume)
    {
        g_MusicVolume = std::clamp(volume, 0.0f, 1.0f);
    }

    void Audio::UpdateMusic(float intensity)
    {
        if (!g_Available || !g_MusicStream)
            return;

        if (!g_MusicBuilt)
            BuildMusic();

        const float gain = g_MusicVolume * g_MasterVolume;
        const std::size_t size = g_MusicCalm.size();

        // Keep about a third of a second queued; feed it in small
        // chunks so the crossfade follows the fighting closely.
        constexpr int ChunkFrames = SampleRate / 20;
        const int queuedBytes = SDL_GetAudioStreamQueued(g_MusicStream);
        const int wantBytes = static_cast<int>(SampleRate / 3 * 2 * sizeof(float));

        if (queuedBytes >= wantBytes)
            return;

        std::vector<float> chunk(static_cast<std::size_t>(ChunkFrames) * 2);

        for (int frame = 0; frame < ChunkFrames; frame++)
        {
            // Swells into combat fast, settles back slowly.
            const float target = std::clamp(intensity, 0.0f, 1.0f);
            const float rate = target > g_MusicIntensity ? 1.0f / (SampleRate * 0.8f) : 1.0f / (SampleRate * 4.0f);
            g_MusicIntensity += std::clamp(target - g_MusicIntensity, -rate, rate);

            const float calm = g_MusicCalm[g_MusicCursor];
            const float combat = g_MusicCombat[g_MusicCursor];
            const float mixed = (calm * (1.0f - g_MusicIntensity * 0.65f) + combat * g_MusicIntensity) * gain;

            // A little width: the combat loop leans one way, the calm
            // pad the other.
            chunk[static_cast<std::size_t>(frame) * 2] = mixed + (calm - combat) * 0.05f * gain;
            chunk[static_cast<std::size_t>(frame) * 2 + 1] = mixed - (calm - combat) * 0.05f * gain;

            g_MusicCursor = (g_MusicCursor + 1) % size;
        }

        SDL_PutAudioStreamData(g_MusicStream, chunk.data(),
            static_cast<int>(chunk.size() * sizeof(float)));
    }

    void Audio::StopMusic()
    {
        if (g_MusicStream)
            SDL_ClearAudioStream(g_MusicStream);
    }

    void Audio::Play(Sfx sfx, float volume, float pan)
    {
        if (!g_Available)
            return;

        volume *= g_MasterVolume;

        if (volume <= 0.001f)
            return;

        const std::vector<float>* bufferPtr =
            &g_Buffers[static_cast<int>(SynthFallback(sfx))];

        std::vector<std::vector<float>>& takes = g_Takes[static_cast<int>(sfx)];

        if (!takes.empty())
        {
            // A random take, never the same one twice running.
            int& last = g_LastTake[static_cast<int>(sfx)];
            int take = 0;

            if (takes.size() > 1)
            {
                g_TakeRng ^= g_TakeRng << 13;
                g_TakeRng ^= g_TakeRng >> 17;
                g_TakeRng ^= g_TakeRng << 5;

                take = static_cast<int>(g_TakeRng % (takes.size() - 1));

                if (take >= last)
                    take++;
            }

            last = take;
            bufferPtr = &takes[static_cast<std::size_t>(take)];
        }
        else if (sfx == Sfx::Dig)
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

        // Equal-ish power pan into the stereo stream.
        const float p = std::clamp(pan, -1.0f, 1.0f);
        const float left = volume * std::min(1.0f, 1.0f - p * 0.8f);
        const float right = volume * std::min(1.0f, 1.0f + p * 0.8f);

        std::vector<float> stereo(buffer.size() * 2);

        for (std::size_t i = 0; i < buffer.size(); i++)
        {
            stereo[i * 2] = buffer[i] * left;
            stereo[i * 2 + 1] = buffer[i] * right;
        }

        SDL_PutAudioStreamData(
            stream,
            stereo.data(),
            static_cast<int>(stereo.size() * sizeof(float)));
    }

    bool Audio::IsAvailable()
    {
        return g_Available;
    }
}
