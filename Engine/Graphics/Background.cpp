#include "Background.h"

#include "../Core/Window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Atlas
{
    namespace
    {
        std::uint32_t Hash(std::uint32_t x, std::uint32_t y)
        {
            std::uint32_t h = x * 374761393u + y * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return h ^ (h >> 16);
        }

        struct Color
        {
            float R, G, B;
        };

        Color Lerp(const Color& a, const Color& b, float t)
        {
            return {
                a.R + (b.R - a.R) * t,
                a.G + (b.G - a.G) * t,
                a.B + (b.B - a.B) * t };
        }

        // Dusk palette.
        constexpr Color SkyTop{ 16.0f, 18.0f, 38.0f };
        constexpr Color SkyMid{ 52.0f, 44.0f, 74.0f };
        constexpr Color SkyHorizon{ 158.0f, 96.0f, 68.0f };

        // Mountain silhouettes, far to near (hazier when distant).
        constexpr Color LayerColors[3] =
        {
            { 76.0f, 62.0f, 82.0f },
            { 52.0f, 44.0f, 64.0f },
            { 33.0f, 30.0f, 46.0f },
        };

        constexpr float LayerParallax[3] = { 0.12f, 0.26f, 0.45f };
        constexpr float LayerBase[3] = { 300.0f, 380.0f, 470.0f };
        constexpr float LayerAmp[3] = { 90.0f, 120.0f, 150.0f };
    }

    Background::Background()
        : m_Seed(1u)
    {
        for (int layer = 0; layer < 3; layer++)
            for (int octave = 0; octave < 3; octave++)
                m_Phase[layer][octave] = 0.0f;
    }

    void Background::Create(unsigned int seed)
    {
        m_Seed = seed | 1u;

        for (int layer = 0; layer < 3; layer++)
        {
            for (int octave = 0; octave < 3; octave++)
            {
                m_Phase[layer][octave] = static_cast<float>(
                    Hash(seed + layer * 7u, octave * 13u) % 6283u) * 0.001f;
            }
        }
    }

    float Background::LayerHeight(int layer, float worldX) const
    {
        const float x = worldX;

        float height = 0.0f;
        height += std::sin(x * 0.0016f + m_Phase[layer][0]) * 1.0f;
        height += std::sin(x * 0.0043f + m_Phase[layer][1]) * 0.45f;
        height += std::sin(x * 0.0110f + m_Phase[layer][2]) * 0.18f;

        return height * LayerAmp[layer];
    }

    void Background::Draw(
        Window& window,
        float cameraX,
        float cameraY,
        int viewWidth,
        int viewHeight,
        float timeSeconds) const
    {
        const float width = static_cast<float>(viewWidth);
        const float height = static_cast<float>(viewHeight);

        // --- Sky gradient (three stops) ---
        constexpr int Bands = 28;

        for (int band = 0; band < Bands; band++)
        {
            const float t =
                static_cast<float>(band) / static_cast<float>(Bands - 1);

            const Color color = t < 0.55f
                ? Lerp(SkyTop, SkyMid, t / 0.55f)
                : Lerp(SkyMid, SkyHorizon, (t - 0.55f) / 0.45f);

            window.DrawScreenRect(
                0.0f,
                height * static_cast<float>(band) / Bands,
                width,
                height / Bands + 1.0f,
                static_cast<Uint8>(color.R),
                static_cast<Uint8>(color.G),
                static_cast<Uint8>(color.B),
                255);
        }

        // --- Stars in the dark upper sky (tiny parallax, some twinkle) ---
        for (int i = 0; i < 90; i++)
        {
            const std::uint32_t h = Hash(m_Seed, static_cast<std::uint32_t>(i));

            float x = static_cast<float>(h % 2600u);
            float y = static_cast<float>((h >> 11) % 300u);

            x = std::fmod(x - cameraX * 0.04f, width + 40.0f) - 20.0f;
            y = y - cameraY * 0.02f;

            if (x < 0.0f || x > width || y < 0.0f || y > height * 0.45f)
                continue;

            float brightness = 120.0f + static_cast<float>((h >> 20) % 100u);

            if ((h & 7u) == 0u)
            {
                brightness *= 0.6f + 0.4f * std::sin(
                    timeSeconds * 2.0f + static_cast<float>(h % 100u));
            }

            const Uint8 value = static_cast<Uint8>(
                std::clamp(brightness, 0.0f, 235.0f));

            window.DrawScreenRect(x, y, 2.0f, 2.0f, value, value,
                static_cast<Uint8>(std::min(255.0f, brightness * 1.05f)), 255);
        }

        // --- Moon with soft halo ---
        {
            const float moonX = width * 0.76f - cameraX * 0.05f;
            const float moonY = 110.0f - cameraY * 0.03f;
            const float radius = 26.0f;

            // Halo (additive).
            window.DrawGlowScreen(moonX, moonY, radius * 3.0f, 210, 200, 170, 22);

            // Disc, drawn as row strips.
            for (float dy = -radius; dy <= radius; dy += 2.0f)
            {
                const float half = std::sqrt(
                    std::max(0.0f, radius * radius - dy * dy));

                window.DrawScreenRect(
                    moonX - half, moonY + dy, half * 2.0f, 2.0f,
                    226, 222, 204, 255);
            }

            // Craters.
            window.DrawScreenRect(moonX - 8.0f, moonY - 6.0f, 7.0f, 5.0f,
                204, 199, 182, 255);
            window.DrawScreenRect(moonX + 4.0f, moonY + 6.0f, 5.0f, 4.0f,
                208, 203, 186, 255);
            window.DrawScreenRect(moonX + 9.0f, moonY - 9.0f, 4.0f, 3.0f,
                206, 201, 184, 255);
        }

        // --- Clouds: soft layered strips drifting slowly ---
        for (int i = 0; i < 6; i++)
        {
            const std::uint32_t h = Hash(m_Seed * 3u, static_cast<std::uint32_t>(i));

            const float baseX = static_cast<float>(h % 2200u);
            const float y = 90.0f + static_cast<float>((h >> 9) % 200u) -
                cameraY * 0.05f;
            const float cloudWidth = 140.0f + static_cast<float>((h >> 18) % 160u);

            float x = std::fmod(
                baseX + timeSeconds * 5.0f - cameraX * 0.08f,
                width + cloudWidth * 2.0f) - cloudWidth;

            window.DrawScreenRect(x, y, cloudWidth, 10.0f, 168, 148, 158, 26);
            window.DrawScreenRect(x + cloudWidth * 0.12f, y - 6.0f,
                cloudWidth * 0.7f, 8.0f, 178, 158, 166, 22);
            window.DrawScreenRect(x + cloudWidth * 0.2f, y + 8.0f,
                cloudWidth * 0.55f, 6.0f, 150, 132, 144, 20);
        }

        // --- Mountain layers, far to near ---
        constexpr float ColumnWidth = 4.0f;

        for (int layer = 0; layer < 3; layer++)
        {
            const Color& color = LayerColors[layer];

            for (float screenX = 0.0f; screenX < width; screenX += ColumnWidth)
            {
                const float worldX =
                    screenX + cameraX * LayerParallax[layer];

                const float top =
                    LayerBase[layer] -
                    LayerHeight(layer, worldX) -
                    cameraY * LayerParallax[layer] * 0.4f;

                if (top >= height)
                    continue;

                window.DrawScreenRect(
                    screenX,
                    top,
                    ColumnWidth,
                    height - top,
                    static_cast<Uint8>(color.R),
                    static_cast<Uint8>(color.G),
                    static_cast<Uint8>(color.B),
                    255);
            }
        }

        // --- Horizon haze where mountains meet the playfield ---
        window.DrawScreenRect(0.0f, height * 0.72f, width, height * 0.28f,
            30, 28, 44, 60);
    }
}
