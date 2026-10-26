#include "Background.h"

#include "../Core/Window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

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

        SDL_FColor ToF(const Color& c, float alpha = 1.0f)
        {
            return { c.R / 255.0f, c.G / 255.0f, c.B / 255.0f, alpha };
        }

        SDL_Color ToC(const Color& c, Uint8 alpha = 255)
        {
            return {
                static_cast<Uint8>(std::clamp(c.R, 0.0f, 255.0f)),
                static_cast<Uint8>(std::clamp(c.G, 0.0f, 255.0f)),
                static_cast<Uint8>(std::clamp(c.B, 0.0f, 255.0f)),
                alpha };
        }

        SDL_Vertex V(float x, float y, SDL_FColor color)
        {
            SDL_Vertex v;
            v.position = { x, y };
            v.color = color;
            v.tex_coord = { 0.0f, 0.0f };
            return v;
        }

        constexpr int LayerCount = 5;

        // A biome's sky. The scene's light map multiplies a cool ambient
        // over all of this, so it is authored a touch bright.
        struct Palette
        {
            Color SkyTop;
            Color SkyHigh;
            Color SkyMid;
            Color SkyHorizon;
            Color Haze;

            // Ridge silhouettes, far to near: each fades from a moonlit
            // rim into the haze at its foot (atmospheric perspective).
            Color LayerTop[LayerCount];
            Color LayerFoot[LayerCount];

            Color CloudHigh; // clouds up in the dark
            Color CloudLow;  // clouds catching the horizon glow
            Color Galaxy;
            Color MoonTint;  // multiplies the moon's gray
            float Stars;     // 0..1 of the stars that show
        };

        constexpr Palette Palettes[static_cast<int>(Biome::Count)] =
        {
            // Temperate: moonlit dusk.
            {
                { 14.0f, 16.0f, 40.0f }, { 34.0f, 34.0f, 76.0f }, { 86.0f, 62.0f, 110.0f },
                { 214.0f, 128.0f, 96.0f }, { 120.0f, 84.0f, 112.0f },
                { { 104.0f, 84.0f, 124.0f }, { 78.0f, 64.0f, 104.0f }, { 56.0f, 48.0f, 82.0f },
                    { 36.0f, 33.0f, 58.0f }, { 21.0f, 20.0f, 36.0f } },
                { { 130.0f, 92.0f, 118.0f }, { 100.0f, 76.0f, 108.0f }, { 70.0f, 58.0f, 90.0f },
                    { 44.0f, 40.0f, 66.0f }, { 26.0f, 24.0f, 42.0f } },
                { 120.0f, 110.0f, 160.0f }, { 200.0f, 130.0f, 130.0f },
                { 135.0f, 110.0f, 190.0f }, { 1.0f, 0.97f, 0.9f }, 1.0f,
            },
            // Desert: a hot amber afterglow over mesas.
            {
                { 22.0f, 16.0f, 42.0f }, { 60.0f, 38.0f, 82.0f }, { 156.0f, 80.0f, 92.0f },
                { 250.0f, 168.0f, 90.0f }, { 186.0f, 112.0f, 96.0f },
                { { 168.0f, 104.0f, 112.0f }, { 132.0f, 80.0f, 96.0f }, { 98.0f, 60.0f, 78.0f },
                    { 62.0f, 40.0f, 54.0f }, { 34.0f, 22.0f, 32.0f } },
                { { 206.0f, 130.0f, 110.0f }, { 160.0f, 98.0f, 100.0f }, { 116.0f, 72.0f, 84.0f },
                    { 72.0f, 46.0f, 60.0f }, { 40.0f, 26.0f, 36.0f } },
                { 150.0f, 104.0f, 150.0f }, { 240.0f, 150.0f, 110.0f },
                { 170.0f, 110.0f, 160.0f }, { 1.0f, 0.93f, 0.8f }, 0.8f,
            },
            // Arctic: a clear, freezing night.
            {
                { 4.0f, 10.0f, 28.0f }, { 14.0f, 32.0f, 64.0f }, { 38.0f, 76.0f, 112.0f },
                { 124.0f, 172.0f, 196.0f }, { 92.0f, 124.0f, 156.0f },
                { { 100.0f, 116.0f, 144.0f }, { 76.0f, 92.0f, 122.0f }, { 54.0f, 68.0f, 98.0f },
                    { 34.0f, 44.0f, 70.0f }, { 18.0f, 24.0f, 40.0f } },
                { { 84.0f, 106.0f, 132.0f }, { 64.0f, 84.0f, 112.0f }, { 46.0f, 62.0f, 90.0f },
                    { 28.0f, 38.0f, 62.0f }, { 16.0f, 22.0f, 36.0f } },
                { 110.0f, 140.0f, 180.0f }, { 160.0f, 190.0f, 210.0f },
                { 90.0f, 150.0f, 200.0f }, { 0.9f, 0.97f, 1.05f }, 1.0f,
            },
            // Volcanic: smoke lit from below by fire.
            {
                { 18.0f, 8.0f, 12.0f }, { 44.0f, 14.0f, 18.0f }, { 104.0f, 32.0f, 26.0f },
                { 232.0f, 92.0f, 36.0f }, { 132.0f, 46.0f, 34.0f },
                { { 96.0f, 52.0f, 52.0f }, { 74.0f, 40.0f, 42.0f }, { 52.0f, 30.0f, 32.0f },
                    { 32.0f, 20.0f, 22.0f }, { 16.0f, 11.0f, 13.0f } },
                { { 150.0f, 64.0f, 44.0f }, { 112.0f, 50.0f, 40.0f }, { 76.0f, 36.0f, 32.0f },
                    { 44.0f, 24.0f, 24.0f }, { 22.0f, 13.0f, 14.0f } },
                { 90.0f, 50.0f, 52.0f }, { 200.0f, 90.0f, 60.0f },
                { 150.0f, 60.0f, 50.0f }, { 1.15f, 0.55f, 0.4f }, 0.3f,
            },
        };

        constexpr float LayerParallax[LayerCount] = { 0.08f, 0.18f, 0.30f, 0.46f, 0.64f };
        constexpr float LayerBase[LayerCount] = { 380.0f, 430.0f, 485.0f, 540.0f, 600.0f };
        constexpr float LayerAmp[LayerCount] = { 70.0f, 85.0f, 100.0f, 95.0f, 55.0f };
    }

    Background::Background()
        : m_Seed(1u),
        m_Biome(Biome::Temperate)
    {
        for (int layer = 0; layer < LayerCount; layer++)
            for (int octave = 0; octave < 3; octave++)
                m_Phase[layer][octave] = 0.0f;
    }

    void Background::Create(unsigned int seed, Biome biome)
    {
        m_Seed = seed | 1u;
        m_Biome = biome < Biome::Count ? biome : Biome::Temperate;

        for (int layer = 0; layer < LayerCount; layer++)
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

        height *= LayerAmp[layer];

        if (m_Biome == Biome::Desert && layer >= 1 && layer <= 3)
        {
            // Clip the swells into flat-topped mesas with sheer sides.
            const float cap = LayerAmp[layer] * 0.45f;
            height = std::clamp(height * 2.2f, -cap, cap);
            return height + std::fabs(std::sin(x * 0.037f + m_Phase[layer][1])) * 3.0f;
        }

        // Far ranges get sharp peaks; the nearest layers get rocky,
        // craggy detail so they read as ridges, not more hills.
        if (layer <= 1)
            height += std::pow(std::fabs(std::sin(x * 0.0031f + m_Phase[layer][2])), 6.0f) * 70.0f;

        if (layer >= 3)
        {
            height += std::fabs(std::sin(x * 0.021f + m_Phase[layer][1])) * 22.0f;
            height += std::fabs(std::sin(x * 0.048f + m_Phase[layer][2])) * 9.0f;
        }

        return height;
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

        const Palette& pal = Palettes[static_cast<int>(m_Biome)];

        // The horizon drifts a little with the camera so looking down
        // into the ground pulls the sky's glow up with it.
        const float horizon = std::clamp(560.0f - cameraY * 0.08f, 420.0f, 640.0f);

        // --- Sky: smooth four-stop gradient ---
        {
            const float stops[5] = { 0.0f, horizon * 0.35f, horizon * 0.72f, horizon, height };
            const Color colors[5] = { pal.SkyTop, pal.SkyHigh, pal.SkyMid, pal.SkyHorizon, pal.SkyHorizon };

            for (int i = 0; i < 4; i++)
            {
                if (stops[i + 1] <= stops[i])
                    continue;

                window.DrawGradientScreen(0.0f, stops[i], width, stops[i + 1] - stops[i] + 1.0f,
                    ToC(colors[i]), ToC(colors[i + 1]));
            }
        }

        // --- A faint band of galaxy across the upper sky ---
        for (int i = 0; i < 26; i++)
        {
            const std::uint32_t h = Hash(m_Seed + 991u, static_cast<std::uint32_t>(i));
            const float t = static_cast<float>(i) / 25.0f;
            const float x = std::fmod(t * (width + 400.0f) - cameraX * 0.02f + 4000.0f, width + 400.0f) - 200.0f;
            const float y = 40.0f + t * 220.0f + static_cast<float>(h % 60u) - 30.0f - cameraY * 0.015f;
            const float r = 60.0f + static_cast<float>((h >> 8) % 70u);

            window.DrawSoftScreen(x, y, r * 1.6f, r * 0.7f,
                static_cast<Uint8>(pal.Galaxy.R * ((h & 1u) ? 0.88f : 1.1f)),
                static_cast<Uint8>(pal.Galaxy.G), static_cast<Uint8>(pal.Galaxy.B), 14);
        }

        // --- Stars: tiny parallax, a few big ones, some twinkle ---
        for (int i = 0; i < 220; i++)
        {
            const std::uint32_t h = Hash(m_Seed, static_cast<std::uint32_t>(i));

            if (static_cast<float>((h >> 4) % 1000u) >= pal.Stars * 1000.0f)
                continue;

            float x = static_cast<float>(h % 2600u);
            float y = static_cast<float>((h >> 11) % 420u);

            x = std::fmod(x - cameraX * 0.03f + 2600.0f * 4.0f, width + 40.0f) - 20.0f;
            y = std::floor(y - cameraY * 0.015f);
            x = std::floor(x);

            if (x < 0.0f || x > width || y < 0.0f || y > horizon * 0.8f)
                continue;

            // Fainter toward the glow of the horizon.
            float brightness = (110.0f + static_cast<float>((h >> 20) % 140u)) *
                std::clamp(1.2f - y / (horizon * 0.8f), 0.2f, 1.0f);

            if ((h & 7u) == 0u)
            {
                brightness *= 0.55f + 0.45f * std::sin(
                    timeSeconds * (1.5f + static_cast<float>(h % 7u) * 0.3f) +
                    static_cast<float>(h % 100u));
            }

            const Uint8 value = static_cast<Uint8>(std::clamp(brightness, 0.0f, 255.0f));
            const Uint8 blue = static_cast<Uint8>(std::clamp(brightness * 1.08f, 0.0f, 255.0f));

            if ((h >> 28) == 0u)
            {
                // A bright one: a plus shape with a halo.
                window.DrawScreenRect(x - 1.0f, y, 3.0f, 1.0f, value, value, blue, 255);
                window.DrawScreenRect(x, y - 1.0f, 1.0f, 3.0f, value, value, blue, 255);
                window.DrawGlowScreen(x + 0.5f, y + 0.5f, 6.0f, 150, 160, 255,
                    static_cast<Uint8>(value / 10));
            }
            else
            {
                window.DrawScreenRect(x, y, (h & 16u) ? 2.0f : 1.0f, (h & 16u) ? 2.0f : 1.0f,
                    value, value, blue, 255);
            }
        }

        // --- Arctic aurora: slow curtains of green and teal light ---
        if (m_Biome == Biome::Arctic)
        {
            for (int band = 0; band < 3; band++)
            {
                const float bandY = 70.0f + static_cast<float>(band) * 46.0f - cameraY * 0.02f;

                for (float sx = -40.0f; sx < width + 40.0f; sx += 14.0f)
                {
                    const float wx = sx + cameraX * 0.05f;
                    const float wave = std::sin(wx * 0.006f + timeSeconds * 0.21f + static_cast<float>(band) * 1.7f) * 34.0f +
                        std::sin(wx * 0.017f - timeSeconds * 0.37f + static_cast<float>(band)) * 12.0f;
                    const float pulse = 0.5f + 0.5f * std::sin(wx * 0.011f + timeSeconds * 0.6f + static_cast<float>(band) * 2.3f);
                    const Uint8 alpha = static_cast<Uint8>(6.0f + pulse * 16.0f);

                    window.DrawSoftScreen(sx, bandY + wave, 22.0f, 46.0f,
                        band == 1 ? 70 : 60, band == 2 ? 200 : 236, band == 1 ? 200 : 150, alpha);
                }
            }
        }

        // --- Volcanic: a far eruption glowing under its smoke column ---
        if (m_Biome == Biome::Volcanic)
        {
            const float ventX = std::fmod(width * 0.3f - cameraX * LayerParallax[0] + 40000.0f, width + 600.0f) - 300.0f;
            const float ventY = LayerBase[0] - 120.0f + (horizon - 560.0f) * 0.48f - cameraY * LayerParallax[0] * 0.35f;

            window.DrawGlowScreen(ventX, ventY + 40.0f, 240.0f, 255, 90, 30, 30);
            window.DrawGlowScreen(ventX, ventY + 40.0f, 90.0f, 255, 150, 60, 50);

            for (int p = 0; p < 14; p++)
            {
                const float age = std::fmod(timeSeconds * 0.05f + static_cast<float>(p) / 14.0f, 1.0f);
                const float py = ventY - age * 300.0f;
                const float px = ventX + std::sin(age * 3.0f + static_cast<float>(p)) * 14.0f + age * age * 160.0f;
                const float pr = 26.0f + age * 90.0f;
                const Uint8 glow = static_cast<Uint8>(std::max(0.0f, 120.0f - age * 160.0f));

                window.DrawSoftScreen(px, py, pr, pr * 0.7f,
                    static_cast<Uint8>(46 + glow), static_cast<Uint8>(26 + glow / 3), 26,
                    static_cast<Uint8>(40.0f * (1.0f - age)));
            }
        }

        // --- Moon: shaded pixel disc, craters, soft halo ---
        {
            const float moonX = std::floor(width * 0.76f - cameraX * 0.04f);
            const float moonY = std::floor(110.0f - cameraY * 0.025f);
            constexpr int Radius = 28;

            const Color tint = pal.MoonTint;

            window.DrawGlowScreen(moonX, moonY, 170.0f,
                static_cast<Uint8>(std::min(255.0f, 120.0f * tint.R)),
                static_cast<Uint8>(std::min(255.0f, 110.0f * tint.G)),
                static_cast<Uint8>(std::min(255.0f, 160.0f * tint.B)), 14);
            window.DrawGlowScreen(moonX, moonY, 80.0f,
                static_cast<Uint8>(std::min(255.0f, 220.0f * tint.R)),
                static_cast<Uint8>(std::min(255.0f, 210.0f * tint.G)),
                static_cast<Uint8>(std::min(255.0f, 190.0f * tint.B)), 40);

            for (int dy = -Radius; dy <= Radius; dy++)
            {
                for (int dx = -Radius; dx <= Radius; dx++)
                {
                    const float d2 = static_cast<float>(dx * dx + dy * dy);

                    if (d2 > static_cast<float>(Radius * Radius))
                        continue;

                    // Lit from the upper left: a soft terminator, with
                    // mottled maria from a coarse hash.
                    const float nx = static_cast<float>(dx) / Radius;
                    const float ny = static_cast<float>(dy) / Radius;
                    const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                    float light = std::clamp(-nx * 0.55f - ny * 0.35f + nz * 0.75f, 0.0f, 1.0f);

                    const std::uint32_t mare = Hash(static_cast<std::uint32_t>(dx + 64) / 5u,
                        static_cast<std::uint32_t>(dy + 64) / 4u);

                    if (mare % 5u == 0u)
                        light *= 0.82f;

                    const float v = 120.0f + light * 120.0f;

                    window.DrawScreenRect(moonX + static_cast<float>(dx), moonY + static_cast<float>(dy),
                        1.0f, 1.0f,
                        static_cast<Uint8>(std::min(255.0f, v * tint.R)),
                        static_cast<Uint8>(std::min(255.0f, v * tint.G)),
                        static_cast<Uint8>(std::min(255.0f, v * tint.B)), 255);
                }
            }
        }

        // --- Clouds: clusters of soft puffs drifting slowly ---
        for (int i = 0; i < 7; i++)
        {
            const std::uint32_t h = Hash(m_Seed * 3u, static_cast<std::uint32_t>(i));

            const float baseX = static_cast<float>(h % 2400u);
            const float y = 80.0f + static_cast<float>((h >> 9) % 230u) - cameraY * 0.05f;
            const float cloudWidth = 160.0f + static_cast<float>((h >> 18) % 180u);

            const float x = std::fmod(
                baseX + timeSeconds * (3.0f + static_cast<float>(i % 3)) - cameraX * 0.07f + 24000.0f,
                width + cloudWidth * 2.0f) - cloudWidth;

            // Lower clouds catch the horizon's warm light.
            const float warmth = std::clamp((y - 120.0f) / 260.0f, 0.0f, 1.0f);
            const Color tint = Lerp(pal.CloudHigh, pal.CloudLow, warmth);

            for (int p = 0; p < 7; p++)
            {
                const std::uint32_t q = Hash(h, static_cast<std::uint32_t>(p));
                const float px = x + cloudWidth * (static_cast<float>(p) / 6.0f);
                const float py = y + static_cast<float>(q % 14u) - 7.0f -
                    std::sin(static_cast<float>(p) / 6.0f * 3.14159f) * 10.0f;
                const float pr = cloudWidth * 0.18f + static_cast<float>((q >> 8) % 20u);

                window.DrawSoftScreen(px, py, pr, pr * 0.45f,
                    static_cast<Uint8>(tint.R), static_cast<Uint8>(tint.G), static_cast<Uint8>(tint.B), 34);
            }
        }

        // --- Ridges, far to near, each a gradient silhouette with haze
        // rising between them ---
        constexpr float Step = 3.0f;

        std::vector<SDL_Vertex> triangles;
        triangles.reserve(static_cast<std::size_t>(width / Step + 2.0f) * 6);

        auto drawLayer = [&](int layer)
        {
            triangles.clear();

            const SDL_FColor top = ToF(pal.LayerTop[layer]);
            const SDL_FColor foot = ToF(pal.LayerFoot[layer]);
            const float parallaxY = cameraY * LayerParallax[layer] * 0.35f;
            const float base = LayerBase[layer] + (horizon - 560.0f) * (0.4f + LayerParallax[layer]);

            float prevX = 0.0f;
            float prevTop = 0.0f;

            for (float screenX = 0.0f; screenX <= width + Step; screenX += Step)
            {
                const float worldX = screenX + cameraX * LayerParallax[layer];
                const float ridgeTop = std::floor(base - LayerHeight(layer, worldX) - parallaxY);

                if (screenX > 0.0f)
                {
                    // The ridge's own 120px fade, then solid foot color.
                    const float fadeA = std::min(height, prevTop + 120.0f);
                    const float fadeB = std::min(height, ridgeTop + 120.0f);

                    triangles.push_back(V(prevX, prevTop, top));
                    triangles.push_back(V(screenX, ridgeTop, top));
                    triangles.push_back(V(screenX, fadeB, foot));
                    triangles.push_back(V(prevX, prevTop, top));
                    triangles.push_back(V(screenX, fadeB, foot));
                    triangles.push_back(V(prevX, fadeA, foot));

                    triangles.push_back(V(prevX, fadeA, foot));
                    triangles.push_back(V(screenX, fadeB, foot));
                    triangles.push_back(V(screenX, height, foot));
                    triangles.push_back(V(prevX, fadeA, foot));
                    triangles.push_back(V(screenX, height, foot));
                    triangles.push_back(V(prevX, height, foot));
                }

                prevX = screenX;
                prevTop = ridgeTop;
            }

            window.DrawTrianglesScreen(triangles);

            // A thin moonlit rim along the far ranges.
            if (layer <= 2)
            {
                for (float screenX = 0.0f; screenX < width; screenX += 2.0f)
                {
                    const float worldX = screenX + cameraX * LayerParallax[layer];
                    const float ridgeTop = std::floor(base - LayerHeight(layer, worldX) - parallaxY);
                    const float slope = LayerHeight(layer, worldX + 2.0f) - LayerHeight(layer, worldX);

                    // Slopes facing the moon (to the right) catch it.
                    if (slope < 0.0f)
                    {
                        window.DrawScreenRect(screenX, ridgeTop, 2.0f, 1.0f,
                            static_cast<Uint8>(std::min(255.0f, pal.LayerTop[layer].R + 40.0f)),
                            static_cast<Uint8>(std::min(255.0f, pal.LayerTop[layer].G + 34.0f)),
                            static_cast<Uint8>(std::min(255.0f, pal.LayerTop[layer].B + 40.0f)),
                            static_cast<Uint8>(std::min(160.0f, -slope * 90.0f)));
                    }
                }
            }

            // Flora along the nearest ridge.
            if (layer == LayerCount - 1)
            {
                const Color c = pal.LayerTop[layer];
                const Uint8 cr = static_cast<Uint8>(c.R);
                const Uint8 cg = static_cast<Uint8>(c.G);
                const Uint8 cb = static_cast<Uint8>(c.B);

                const float spacing = 23.0f;
                const float scroll = cameraX * LayerParallax[layer];
                const float first = std::floor(scroll / spacing) * spacing;

                for (float worldX = first; worldX < scroll + width + spacing; worldX += spacing)
                {
                    const std::uint32_t h = Hash(m_Seed + 77u, static_cast<std::uint32_t>(worldX / spacing + 10000.0f));

                    if (h % 3u == 0u)
                        continue;

                    const float jitter = static_cast<float>(h % 15u);
                    const float treeX = std::floor(worldX + jitter - scroll);
                    const float groundY = std::floor(base - LayerHeight(layer, worldX + jitter) - parallaxY);
                    const float treeH = 14.0f + static_cast<float>((h >> 6) % 16u);

                    if (m_Biome == Biome::Desert)
                    {
                        // Saguaro: a thick trunk and one or two bent arms.
                        if (h % 5u > 1u)
                            continue;

                        const float top = groundY - treeH;
                        window.DrawScreenRect(treeX - 1.0f, top, 3.0f, treeH + 2.0f, cr, cg, cb, 255);

                        const float armY = top + treeH * 0.35f + static_cast<float>((h >> 12) % 4u);
                        window.DrawScreenRect(treeX - 4.0f, armY, 3.0f, 2.0f, cr, cg, cb, 255);
                        window.DrawScreenRect(treeX - 5.0f, armY - 6.0f, 2.0f, 7.0f, cr, cg, cb, 255);

                        if (h & 64u)
                        {
                            window.DrawScreenRect(treeX + 2.0f, armY + 3.0f, 3.0f, 2.0f, cr, cg, cb, 255);
                            window.DrawScreenRect(treeX + 4.0f, armY - 2.0f, 2.0f, 6.0f, cr, cg, cb, 255);
                        }

                        continue;
                    }

                    if (m_Biome == Biome::Volcanic)
                    {
                        // Burnt snags: bare trunks with a broken limb.
                        if (h % 4u != 0u)
                            continue;

                        window.DrawScreenRect(treeX, groundY - treeH, 1.0f, treeH + 2.0f, cr, cg, cb, 255);
                        const float limbY = groundY - treeH * 0.6f;
                        const float dir = (h & 128u) ? 1.0f : -1.0f;

                        for (int k = 1; k <= 4; k++)
                            window.DrawScreenRect(treeX + dir * static_cast<float>(k), limbY - static_cast<float>(k),
                                1.0f, 1.0f, cr, cg, cb, 255);

                        continue;
                    }

                    window.DrawScreenRect(treeX, groundY - treeH, 1.0f, treeH + 2.0f, cr, cg, cb, 255);

                    for (float k = 0.0f; k < treeH - 2.0f; k += 2.0f)
                    {
                        const float half = std::floor((k / treeH) * treeH * 0.28f) + 1.0f;

                        window.DrawScreenRect(treeX - half, groundY - treeH + 2.0f + k, half * 2.0f + 1.0f, 2.0f,
                            cr, cg, cb, 255);

                        // Snow settled on the arctic pines' boughs.
                        if (m_Biome == Biome::Arctic && static_cast<int>(k) % 4 == 0)
                            window.DrawScreenRect(treeX - half, groundY - treeH + 2.0f + k, half * 2.0f + 1.0f, 1.0f,
                                120, 136, 160, 255);
                    }
                }
            }
        };

        for (int layer = 0; layer < LayerCount; layer++)
        {
            drawLayer(layer);

            // Haze rising from behind each nearer layer.
            if (layer < LayerCount - 1)
            {
                const float hazeTop = LayerBase[layer] + (horizon - 560.0f) * 0.5f - cameraY * LayerParallax[layer] * 0.35f;

                window.DrawGradientScreen(0.0f, hazeTop - 30.0f, width, height - hazeTop + 30.0f,
                    ToC(pal.Haze, 0), ToC(pal.Haze, static_cast<Uint8>(36 - layer * 6)));
            }
        }

        // --- Volcanic embers drifting up past the ridges ---
        if (m_Biome == Biome::Volcanic)
        {
            for (int i = 0; i < 60; i++)
            {
                const std::uint32_t h = Hash(m_Seed + 4111u, static_cast<std::uint32_t>(i));
                const float speed = 12.0f + static_cast<float>(h % 20u);
                const float span = height + 40.0f;
                const float y = span - std::fmod(timeSeconds * speed + static_cast<float>((h >> 8) % 1000u), span) - 20.0f;
                const float x = std::fmod(static_cast<float>((h >> 4) % 3000u) - cameraX * 0.5f +
                    std::sin(timeSeconds * 0.8f + static_cast<float>(i)) * 10.0f + 30000.0f, width + 20.0f) - 10.0f;
                const float flicker = 0.6f + 0.4f * std::sin(timeSeconds * 6.0f + static_cast<float>(h % 50u));

                window.DrawScreenRect(std::floor(x), std::floor(y), 1.0f, 1.0f, 255,
                    static_cast<Uint8>(120.0f + 80.0f * flicker), 40, static_cast<Uint8>(200.0f * flicker));
                window.DrawGlowScreen(x, y, 5.0f, 255, 110, 40, static_cast<Uint8>(30.0f * flicker));
            }
        }

        // --- Arctic snowfall ---
        if (m_Biome == Biome::Arctic)
        {
            for (int i = 0; i < 90; i++)
            {
                const std::uint32_t h = Hash(m_Seed + 5003u, static_cast<std::uint32_t>(i));
                const float speed = 10.0f + static_cast<float>(h % 18u);
                const float span = height + 20.0f;
                const float y = std::fmod(timeSeconds * speed + static_cast<float>((h >> 8) % 1000u) - cameraY * 0.4f + 40000.0f, span) - 10.0f;
                const float x = std::fmod(static_cast<float>((h >> 4) % 3000u) - cameraX * 0.4f +
                    std::sin(timeSeconds * 0.5f + static_cast<float>(i)) * 16.0f + 30000.0f, width + 20.0f) - 10.0f;
                const float size = (h & 3u) == 0u ? 2.0f : 1.0f;

                window.DrawScreenRect(std::floor(x), std::floor(y), size, size, 220, 230, 245, 150);
            }
        }
    }
}
