#include "Terrain.h"

#include "../Core/Window.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <initializer_list>

namespace Atlas
{
    namespace
    {
        // Deterministic per-pixel hash used to give every terrain pixel a
        // slight brightness variation so large fills don't look flat.
        std::uint32_t HashPixel(std::uint32_t x, std::uint32_t y)
        {
            std::uint32_t h = x * 374761393u + y * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return h ^ (h >> 16);
        }

        std::uint8_t Shade(std::uint8_t channel, float factor)
        {
            const float value = static_cast<float>(channel) * factor;
            return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
        }
    }

    Terrain::Terrain()
        : m_Width(0),
        m_Height(0),
        m_Texture(nullptr),
        m_ChunksX(0),
        m_ChunksY(0),
        m_Stamp(0),
        m_SimTick(0),
        m_DirtyAll(false),
        m_DirtyMinX(0),
        m_DirtyMinY(0),
        m_DirtyMaxX(-1),
        m_DirtyMaxY(-1)
    {
    }

    Terrain::~Terrain()
    {
        Destroy();
    }

    bool Terrain::Create(
        int width,
        int height,
        unsigned int seed)
    {
        Destroy();

        m_Width = width;
        m_Height = height;

        m_Materials.assign(
            static_cast<std::size_t>(width) * height,
            static_cast<std::uint8_t>(Material::Air));

        m_Pixels.assign(
            static_cast<std::size_t>(width) * height * 4,
            0);

        m_DepthShade.assign(
            static_cast<std::size_t>(width) * height,
            255);

        m_Backwall.assign(
            static_cast<std::size_t>(width) * height,
            static_cast<std::uint8_t>(Material::Air));

        m_ChunksX = (width + ChunkSize - 1) / ChunkSize;
        m_ChunksY = (height + ChunkSize - 1) / ChunkSize;
        m_ChunkAwake.assign(static_cast<std::size_t>(m_ChunksX) * m_ChunksY, 0);
        m_ChunkAwakeNext.assign(m_ChunkAwake.size(), 0);
        m_MoveStamp.assign(static_cast<std::size_t>(width) * height, 0);
        m_Stamp = 0;
        m_SimTick = 0;
        m_Reactions.clear();

        Generate(seed);
        RebuildAllPixels();

        // Let freshly generated sand and water find their rest before the
        // match starts, so the first seconds aren't a landslide.
        for (int i = 0; i < 900 && GetAwakeRegionCount() > 0; i++)
            Simulate();

        m_Reactions.clear();

        m_DirtyAll = true;
        ResetDirtyRegion();

        return true;
    }

    bool Terrain::CreateTexture(SDL_Renderer* renderer)
    {
        if (m_Texture)
        {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
        }

        m_Texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            m_Width,
            m_Height);

        if (!m_Texture)
        {
            std::cout << "Failed to create terrain texture: "
                << SDL_GetError() << std::endl;
            return false;
        }

        SDL_SetTextureBlendMode(m_Texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(m_Texture, SDL_SCALEMODE_NEAREST);

        // The whole color layer goes up on the next Update().
        m_DirtyAll = true;
        ResetDirtyRegion();

        return true;
    }

    const std::uint8_t* Terrain::GetMaterialData() const
    {
        return m_Materials.data();
    }

    const std::uint8_t* Terrain::GetPixelData() const
    {
        return m_Pixels.data();
    }

    void Terrain::Destroy()
    {
        if (m_Texture)
        {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
        }

        m_Materials.clear();
        m_Pixels.clear();

        m_Width = 0;
        m_Height = 0;
    }

    int Terrain::GetWidth() const
    {
        return m_Width;
    }

    int Terrain::GetHeight() const
    {
        return m_Height;
    }

    Material Terrain::GetMaterial(int x, int y) const
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return Material::Air;

        return static_cast<Material>(
            m_Materials[static_cast<std::size_t>(y) * m_Width + x]);
    }

    void Terrain::SetMaterial(int x, int y, Material material)
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return;

        std::uint8_t& slot =
            m_Materials[static_cast<std::size_t>(y) * m_Width + x];

        if (slot == static_cast<std::uint8_t>(material))
            return;

        slot = static_cast<std::uint8_t>(material);

        Wake(x, y);

        // A change here also changes the edge lighting of the pixels
        // directly above and below.
        for (int dy = -1; dy <= 1; dy++)
        {
            WritePixelColor(x, y + dy, GetMaterial(x, y + dy));
            MarkDirty(
                std::clamp(x, 0, m_Width - 1),
                std::clamp(y + dy, 0, m_Height - 1));
        }
    }

    bool Terrain::IsSolid(float worldX, float worldY) const
    {
        const int x = static_cast<int>(std::floor(worldX));
        const int y = static_cast<int>(std::floor(worldY));

        // The sky is open; everything past the side and bottom edges is a
        // solid wall so nothing can leave the scene.
        if (y < 0)
            return false;

        if (x < 0 || x >= m_Width || y >= m_Height)
            return true;

        return GetMaterialInfo(GetMaterial(x, y)).Solid;
    }

    bool Terrain::RaycastSolid(
        float startX,
        float startY,
        float dirX,
        float dirY,
        float maxDistance,
        float& hitX,
        float& hitY) const
    {
        float x = startX;
        float y = startY;

        const int steps = std::max(1, static_cast<int>(maxDistance));

        for (int i = 0; i <= steps; i++)
        {
            if (IsSolid(x, y))
            {
                hitX = x;
                hitY = y;
                return true;
            }

            x += dirX;
            y += dirY;
        }

        return false;
    }

    int Terrain::CarveCircle(float centerX, float centerY, float radius)
    {
        const int minX = static_cast<int>(std::floor(centerX - radius));
        const int maxX = static_cast<int>(std::ceil(centerX + radius));
        const int minY = static_cast<int>(std::floor(centerY - radius));
        const int maxY = static_cast<int>(std::ceil(centerY + radius));

        const float radiusSquared = radius * radius;

        int removed = 0;

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                const float dx = static_cast<float>(x) + 0.5f - centerX;
                const float dy = static_cast<float>(y) + 0.5f - centerY;

                if (dx * dx + dy * dy > radiusSquared)
                    continue;

                const Material material = GetMaterial(x, y);
                const MaterialInfo& info = GetMaterialInfo(material);

                if (!info.Solid || info.Indestructible)
                    continue;

                // Very hard materials (concrete) only give way near the
                // middle of the blast, and a brain's tissue is tough
                // enough that a blast must be nearly on top of it.
                if (info.Strength > 12.0f || material == Material::Brain)
                {
                    const float reach = material == Material::Brain
                        ? 0.3f
                        : std::max(0.25f, 12.0f / info.Strength);

                    if (dx * dx + dy * dy > radiusSquared * reach * reach)
                        continue;
                }

                SetMaterial(x, y, Material::Air);
                removed++;
            }
        }

        return removed;
    }

    int Terrain::CarveCircleCollect(
        float centerX,
        float centerY,
        float radius,
        std::vector<DestroyedPixel>& outDebris,
        int maxSamples,
        const Box* spareBrain)
    {
        const int minX = static_cast<int>(std::floor(centerX - radius));
        const int maxX = static_cast<int>(std::ceil(centerX + radius));
        const int minY = static_cast<int>(std::floor(centerY - radius));
        const int maxY = static_cast<int>(std::ceil(centerY + radius));

        const float radiusSquared = radius * radius;

        int removed = 0;

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                const float dx = static_cast<float>(x) + 0.5f - centerX;
                const float dy = static_cast<float>(y) + 0.5f - centerY;

                if (dx * dx + dy * dy > radiusSquared)
                    continue;

                const Material material = GetMaterial(x, y);
                const MaterialInfo& info = GetMaterialInfo(material);

                if (!info.Solid || info.Indestructible)
                    continue;

                if (spareBrain && material == Material::Brain &&
                    x >= spareBrain->MinX && x <= spareBrain->MaxX &&
                    y >= spareBrain->MinY && y <= spareBrain->MaxY)
                    continue;

                // Very hard materials (concrete) only give way near the
                // middle of the blast, and a brain's tissue is tough
                // enough that a blast must be nearly on top of it.
                if (info.Strength > 12.0f || material == Material::Brain)
                {
                    const float reach = material == Material::Brain
                        ? 0.3f
                        : std::max(0.25f, 12.0f / info.Strength);

                    if (dx * dx + dy * dy > radiusSquared * reach * reach)
                        continue;
                }

                SetMaterial(x, y, Material::Air);
                removed++;

                // Sample roughly every fifth destroyed pixel as debris.
                if (static_cast<int>(outDebris.size()) < maxSamples &&
                    (removed % 5) == 0)
                {
                    outDebris.push_back({
                        static_cast<float>(x),
                        static_cast<float>(y),
                        material });
                }
            }
        }

        return removed;
    }

    Material Terrain::DestroyPixel(int x, int y)
    {
        const Material material = GetMaterial(x, y);
        const MaterialInfo& info = GetMaterialInfo(material);

        if (!info.Solid || info.Indestructible)
            return Material::Air;

        SetMaterial(x, y, Material::Air);

        return material;
    }

    void Terrain::StainPixel(
        int x,
        int y,
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b)
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return;

        if (!GetMaterialInfo(GetMaterial(x, y)).Solid)
            return;

        const std::size_t index =
            (static_cast<std::size_t>(y) * m_Width + x) * 4;

        // Blend 60% toward the stain color.
        m_Pixels[index + 0] = static_cast<std::uint8_t>(
            (m_Pixels[index + 0] * 2 + r * 3) / 5);
        m_Pixels[index + 1] = static_cast<std::uint8_t>(
            (m_Pixels[index + 1] * 2 + g * 3) / 5);
        m_Pixels[index + 2] = static_cast<std::uint8_t>(
            (m_Pixels[index + 2] * 2 + b * 3) / 5);

        MarkDirty(x, y);
    }

    int Terrain::PlaceCircle(
        float centerX,
        float centerY,
        float radius,
        Material material)
    {
        const int minX = static_cast<int>(std::floor(centerX - radius));
        const int maxX = static_cast<int>(std::ceil(centerX + radius));
        const int minY = static_cast<int>(std::floor(centerY - radius));
        const int maxY = static_cast<int>(std::ceil(centerY + radius));

        const float radiusSquared = radius * radius;

        int placed = 0;

        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                const float dx = static_cast<float>(x) + 0.5f - centerX;
                const float dy = static_cast<float>(y) + 0.5f - centerY;

                if (dx * dx + dy * dy > radiusSquared)
                    continue;

                if (GetMaterial(x, y) != Material::Air)
                    continue;

                SetMaterial(x, y, material);
                placed++;
            }
        }

        return placed;
    }

    void Terrain::Update()
    {
        if (!m_Texture)
            return;

        if (m_DirtyAll)
        {
            SDL_UpdateTexture(
                m_Texture,
                nullptr,
                m_Pixels.data(),
                m_Width * 4);

            m_DirtyAll = false;
            ResetDirtyRegion();
            return;
        }

        if (m_DirtyMaxX < m_DirtyMinX || m_DirtyMaxY < m_DirtyMinY)
            return;

        SDL_Rect rect;
        rect.x = m_DirtyMinX;
        rect.y = m_DirtyMinY;
        rect.w = m_DirtyMaxX - m_DirtyMinX + 1;
        rect.h = m_DirtyMaxY - m_DirtyMinY + 1;

        const std::size_t offset =
            (static_cast<std::size_t>(rect.y) * m_Width + rect.x) * 4;

        SDL_UpdateTexture(
            m_Texture,
            &rect,
            m_Pixels.data() + offset,
            m_Width * 4);

        ResetDirtyRegion();
    }

    void Terrain::Draw(Window& window)
    {
        if (!m_Texture)
            return;

        window.DrawTexture(
            m_Texture,
            0.0f,
            0.0f,
            static_cast<float>(m_Width),
            static_cast<float>(m_Height));
    }

    namespace
    {
        // Portable generator for world building: std distributions are
        // allowed to differ between standard libraries, and every peer
        // in a match must build the exact same world from a seed.
        class GenRandom
        {
        public:
            explicit GenRandom(unsigned int seed)
                : m_State(0x9E3779B97F4A7C15ull ^ (static_cast<std::uint64_t>(seed) * 0xD1B54A32D192ED03ull))
            {
                if (m_State == 0)
                    m_State = 1;
            }

            std::uint32_t Next()
            {
                m_State ^= m_State << 13;
                m_State ^= m_State >> 7;
                m_State ^= m_State << 17;
                return static_cast<std::uint32_t>(m_State >> 32);
            }

            // Inclusive range.
            int Range(int lo, int hi)
            {
                if (hi <= lo)
                    return lo;

                return lo + static_cast<int>(Next() % static_cast<std::uint32_t>(hi - lo + 1));
            }

            float Unit() // [0, 1)
            {
                return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f);
            }

        private:
            std::uint64_t m_State;
        };
    }

    void Terrain::Generate(unsigned int seed)
    {
        GenRandom rng(seed);

        auto at = [&](int x, int y) -> std::uint8_t&
        {
            return m_Materials[static_cast<std::size_t>(y) * m_Width + x];
        };

        auto inside = [&](int x, int y)
        {
            return x >= 0 && x < m_Width && y >= 0 && y < m_Height;
        };

        // Paints a filled circle, but only over the listed materials.
        auto blob = [&](int cx, int cy, int radius, Material paint,
            std::initializer_list<Material> over)
        {
            for (int y = cy - radius; y <= cy + radius; y++)
            {
                for (int x = cx - radius; x <= cx + radius; x++)
                {
                    if (!inside(x, y))
                        continue;

                    const int dx = x - cx;
                    const int dy = y - cy;

                    if (dx * dx + dy * dy > radius * radius)
                        continue;

                    for (Material m : over)
                    {
                        if (at(x, y) == static_cast<std::uint8_t>(m))
                        {
                            at(x, y) = static_cast<std::uint8_t>(paint);
                            break;
                        }
                    }
                }
            }
        };

        const float tau = 6.28318f;

        // Rolling surface height built from a few octaves of sine waves,
        // each with a random phase so every seed produces a new landscape.
        const float phase1 = rng.Unit() * tau;
        const float phase2 = rng.Unit() * tau;
        const float phase3 = rng.Unit() * tau;

        const float baseHeight = static_cast<float>(m_Height) * 0.40f;

        std::vector<int> surface(m_Width);

        for (int x = 0; x < m_Width; x++)
        {
            const float fx = static_cast<float>(x);

            float height = baseHeight;
            height += 60.0f * std::sin(fx * 0.0040f + phase1);
            height += 28.0f * std::sin(fx * 0.0110f + phase2);
            height += 10.0f * std::sin(fx * 0.0330f + phase3);

            surface[x] = std::clamp(
                static_cast<int>(height),
                32,
                m_Height - 160);
        }

        // Dirt depth and the bedrock floor wander a little.
        const float dirtPhase = rng.Unit() * tau;
        const float bedPhase = rng.Unit() * tau;

        std::vector<int> dirtDepth(m_Width);
        std::vector<int> bedrockTop(m_Width);

        for (int x = 0; x < m_Width; x++)
        {
            const float fx = static_cast<float>(x);

            dirtDepth[x] = 80 + static_cast<int>(
                18.0f * std::sin(fx * 0.007f + dirtPhase) +
                6.0f * std::sin(fx * 0.031f + dirtPhase * 2.0f));

            bedrockTop[x] = m_Height - 22 - static_cast<int>(
                6.0f + 6.0f * std::sin(fx * 0.013f + bedPhase) +
                3.0f * std::sin(fx * 0.057f + bedPhase * 3.0f));
        }

        // Desert stretches: the top layers turn to loose sand. Kept away
        // from the middle of the map, where players drop in.
        std::vector<std::uint8_t> desert(m_Width, 0);
        const int duneCount = 1 + rng.Range(0, 1) + m_Width / 3000;

        for (int i = 0; i < duneCount; i++)
        {
            const int width = rng.Range(220, 420);
            int start = rng.Range(0, std::max(0, m_Width - width));

            const int center = m_Width / 2;

            if (start < center + 260 && start + width > center - 260)
                start = (start + width / 2 < center)
                    ? std::max(0, center - 260 - width)
                    : std::min(m_Width - width, center + 260);

            for (int x = std::max(0, start); x < std::min(m_Width, start + width); x++)
            {
                // Ramp in and out so the dunes blend into the grassland.
                const int edge = std::min(x - start, start + width - 1 - x);
                desert[x] = static_cast<std::uint8_t>(std::min(255, edge * 6));
            }
        }

        // Layers: grass (or sand), dirt, stone, bedrock.
        constexpr int GrassDepth = 5;

        for (int x = 0; x < m_Width; x++)
        {
            const int sandDepth = desert[x] > 0
                ? 4 + (desert[x] * 22) / 255
                : 0;

            for (int y = surface[x]; y < m_Height; y++)
            {
                const int depth = y - surface[x];

                Material material = Material::Stone;

                if (y >= bedrockTop[x])
                    material = Material::Bedrock;
                else if (depth < sandDepth)
                    material = Material::Sand;
                else if (depth < GrassDepth)
                    material = Material::Grass;
                else if (depth < dirtDepth[x])
                    material = Material::Dirt;

                at(x, y) = static_cast<std::uint8_t>(material);
                m_Backwall[static_cast<std::size_t>(y) * m_Width + x] =
                    static_cast<std::uint8_t>(material == Material::Grass ? Material::Dirt : material);
            }
        }

        // Sparse grass tufts poking 1-3px above the surface line - they
        // are real Grass pixels, destructible (and walkable) like any
        // other terrain, purely to break up the razor-straight top edge.
        for (int x = 0; x < m_Width; x++)
        {
            if (desert[x] > 0)
                continue;

            const std::uint32_t h = HashPixel(
                static_cast<std::uint32_t>(x), seed | 1u);

            if (h % 3u != 0u)
                continue;

            const int tuftHeight = 1 + static_cast<int>((h >> 8) % 3u);

            for (int t = 1; t <= tuftHeight; t++)
            {
                const int y = surface[x] - t;

                if (y < 1)
                    break;

                at(x, y) = static_cast<std::uint8_t>(Material::Grass);
            }
        }

        // Bake depth shading: the deeper below the surface, the darker,
        // so craters and tunnels reveal dark underground.
        for (int x = 0; x < m_Width; x++)
        {
            for (int y = surface[x]; y < m_Height; y++)
            {
                const float depth = static_cast<float>(y - surface[x]);

                const float factor =
                    1.0f - std::min(0.5f, depth / 300.0f * 0.5f);

                m_DepthShade[static_cast<std::size_t>(y) * m_Width + x] =
                    static_cast<std::uint8_t>(factor * 255.0f);
            }
        }

        // A random point at least minDepth below the surface (or below
        // the dirt layer) and marginAboveBedrock above the floor.
        auto randomDepthPoint = [&](int minDepth, int marginAboveBedrock,
            bool belowDirt, int& outX, int& outY)
        {
            outX = rng.Range(8, m_Width - 9);
            const int top = surface[outX] + minDepth + (belowDirt ? dirtDepth[outX] : 0);
            const int bottom = bedrockTop[outX] - marginAboveBedrock;

            if (top >= bottom)
                return false;

            outY = rng.Range(top, bottom);
            return true;
        };

        // Sand pockets in the dirt: dig into one and it pours out.
        for (int i = 0; i < m_Width / 40; i++)
        {
            int x = 0;
            int y = 0;

            if (randomDepthPoint(12, 200, false, x, y))
                blob(x, y, rng.Range(5, 13), Material::Sand, { Material::Dirt });
        }

        // Gravel pockets in the stone.
        for (int i = 0; i < m_Width / 30; i++)
        {
            int x = 0;
            int y = 0;

            if (randomDepthPoint(10, 30, true, x, y))
                blob(x, y, rng.Range(5, 12), Material::Gravel, { Material::Stone });
        }

        // Gold veins: blobs of gold scattered through the stone layer,
        // richer the deeper you go.
        for (int i = 0; i < m_Width / 10; i++)
        {
            int x = 0;
            int y = 0;

            if (!randomDepthPoint(8, 12, true, x, y))
                continue;

            const float depth01 = static_cast<float>(y - surface[x]) /
                static_cast<float>(std::max(1, bedrockTop[x] - surface[x]));

            const int radius = rng.Range(3, 7) + static_cast<int>(depth01 * 5.0f);
            blob(x, y, radius, Material::Gold, { Material::Stone });
        }

        // Caves: worms that wander through the rock, carving tunnels.
        // Some get a pool of water; the deepest ones a pool of lava.
        struct Chamber
        {
            int X;
            int Y;
            int RX;
            int RY;
            Material Fill;
        };

        std::vector<Chamber> chambers;

        auto is = [&](int x, int y, Material m)
        {
            return inside(x, y) && at(x, y) == static_cast<std::uint8_t>(m);
        };

        const int caveCount = std::max(3, m_Width / 170);

        for (int i = 0; i < caveCount; i++)
        {
            int x0 = 0;
            int y0 = 0;

            if (!randomDepthPoint(40, 40, false, x0, y0))
                continue;

            float x = static_cast<float>(x0);
            float y = static_cast<float>(y0);
            float angle = rng.Unit() * tau;
            const int steps = rng.Range(120, 380);
            float radius = static_cast<float>(rng.Range(6, 11));

            for (int step = 0; step < steps; step++)
            {
                // Mostly sideways, gently drifting up and down.
                angle += (rng.Unit() - 0.5f) * 0.5f;
                radius = std::clamp(radius + (rng.Unit() - 0.5f) * 1.2f, 4.0f, 15.0f);

                x += std::cos(angle) * 2.0f;
                y += std::sin(angle) * 1.2f;

                const int ix = static_cast<int>(x);
                const int iy = static_cast<int>(y);

                if (ix < 6 || ix >= m_Width - 6)
                    break;

                // Stay underground and off the floor.
                if (iy < surface[ix] + 30 || iy > bedrockTop[ix] - 10)
                {
                    angle = -angle;
                    continue;
                }

                blob(ix, iy, static_cast<int>(radius), Material::Air,
                    { Material::Dirt, Material::Stone, Material::Gold, Material::Gravel });

                // Now and then the tunnel opens into a chamber.
                if (rng.Unit() < 0.018f)
                {
                    const int rx = rng.Range(16, 32);
                    const int ry = rng.Range(10, 18);

                    if (iy - ry > surface[ix] + 24 && iy + ry < bedrockTop[ix] - 4)
                    {
                        for (int py = iy - ry; py <= iy + ry; py++)
                        {
                            for (int px = ix - rx; px <= ix + rx; px++)
                            {
                                if (!inside(px, py))
                                    continue;

                                const float ex = static_cast<float>(px - ix) / static_cast<float>(rx);
                                const float ey = static_cast<float>(py - iy) / static_cast<float>(ry);

                                // Ragged walls instead of a perfect ellipse.
                                const float wobble = static_cast<float>(
                                    HashPixel(static_cast<std::uint32_t>(px / 3),
                                        static_cast<std::uint32_t>(py / 3)) % 100u) * 0.002f;

                                if (ex * ex + ey * ey > 1.0f - wobble)
                                    continue;

                                const Material here = static_cast<Material>(at(px, py));

                                if (here != Material::Bedrock && here != Material::Air)
                                    at(px, py) = static_cast<std::uint8_t>(Material::Air);
                            }
                        }

                        chambers.push_back({ ix, iy, rx, ry, Material::Air });
                    }
                }
            }
        }

        // Pools of water, and lava down deep. Each is filled as the basin
        // under a waterline, raised from the floor until the pool holds
        // its volume or would spill into the tunnels, so it starts level
        // and at rest. Done after all caves exist so a later tunnel
        // can't slice through a pool.
        std::vector<int> basin;
        std::vector<int> open;

        // Fills the basin below waterline from (x, y) with fill, or leaves
        // everything as it was and returns false if it holds more than
        // maxCells.
        auto fillBasin = [&](int x, int y, int waterline, Material fill, int maxCells)
        {
            basin.clear();
            open.assign(1, y * m_Width + x);

            while (!open.empty())
            {
                const int cell = open.back();
                open.pop_back();

                const int cx = cell % m_Width;
                const int cy = cell / m_Width;

                if (cy < waterline || !is(cx, cy, Material::Air))
                    continue;

                at(cx, cy) = static_cast<std::uint8_t>(fill);
                basin.push_back(cell);

                if (static_cast<int>(basin.size()) > maxCells)
                {
                    for (int filled : basin)
                        m_Materials[static_cast<std::size_t>(filled)] =
                            static_cast<std::uint8_t>(Material::Air);

                    return false;
                }

                if (cx + 1 < m_Width) open.push_back(cell + 1);
                if (cx > 0) open.push_back(cell - 1);
                if (cy + 1 < m_Height) open.push_back(cell + m_Width);
                if (cy > 0) open.push_back(cell - m_Width);
            }

            return true;
        };

        auto pour = [&](int x, int y, Material fill, int volume)
        {
            if (!is(x, y, Material::Air))
                return;

            while (is(x, y + 1, Material::Air))
                y++;

            if (fill == Material::Water)
            {
                // Pours meeting existing lava would just make obsidian.
                for (int dy = 0; dy < 8; dy++)
                {
                    if (is(x, y + dy, Material::Lava))
                        return;
                }
            }

            // Highest waterline that doesn't overflow.
            int best = -1;

            for (int rise = 1; rise < 60; rise += 2)
            {
                if (!fillBasin(x, y, y - rise, fill, volume))
                    break;

                // Undo; only the final waterline is kept.
                for (int filled : basin)
                    m_Materials[static_cast<std::size_t>(filled)] =
                        static_cast<std::uint8_t>(Material::Air);

                best = rise;
            }

            if (best > 2)
                fillBasin(x, y, y - best, fill, volume);
        };

        auto liquidFor = [&](int y)
        {
            const float depth01 = static_cast<float>(y) / static_cast<float>(m_Height);
            return depth01 > 0.7f && rng.Unit() < 0.7f ? Material::Lava : Material::Water;
        };

        for (const Chamber& chamber : chambers)
        {
            if (rng.Unit() < 0.75f)
                pour(chamber.X, chamber.Y, liquidFor(chamber.Y), rng.Range(1200, 2600));
        }

        // Low spots along the tunnels collect a little too.
        const int puddles = m_Width / 20;

        for (int i = 0; i < puddles; i++)
        {
            const int px = rng.Range(8, m_Width - 8);
            const int py = rng.Range(surface[px] + 30, bedrockTop[px] - 6);
            pour(px, py, liquidFor(py), rng.Range(300, 1400));
        }

        // Everything that can move starts awake.
        for (int y = 0; y < m_Height; y++)
        {
            for (int x = 0; x < m_Width; x++)
            {
                const MaterialBehavior behavior =
                    GetMaterialInfo(static_cast<Material>(at(x, y))).Behavior;

                if (behavior == MaterialBehavior::Powder ||
                    behavior == MaterialBehavior::Liquid)
                {
                    m_ChunkAwakeNext[static_cast<std::size_t>(y / ChunkSize) * m_ChunksX + x / ChunkSize] = 1;
                }
            }
        }

    }

    // --- Falling-sand simulation ------------------------------------------

    void Terrain::Wake(int x, int y)
    {
        if (m_ChunkAwakeNext.empty())
            return;

        const int cx = x / ChunkSize;
        const int cy = y / ChunkSize;
        const int lx = x % ChunkSize;
        const int ly = y % ChunkSize;

        // A change on a chunk's edge can unsettle its neighbor too.
        const int minCX = std::max(0, lx == 0 ? cx - 1 : cx);
        const int maxCX = std::min(m_ChunksX - 1, lx == ChunkSize - 1 ? cx + 1 : cx);
        const int minCY = std::max(0, ly == 0 ? cy - 1 : cy);
        const int maxCY = std::min(m_ChunksY - 1, ly == ChunkSize - 1 ? cy + 1 : cy);

        for (int wy = minCY; wy <= maxCY; wy++)
        {
            for (int wx = minCX; wx <= maxCX; wx++)
                m_ChunkAwakeNext[static_cast<std::size_t>(wy) * m_ChunksX + wx] = 1;
        }
    }

    int Terrain::GetAwakeRegionCount() const
    {
        int count = 0;

        for (std::uint8_t awake : m_ChunkAwakeNext)
            count += awake;

        return count;
    }

    std::vector<Terrain::DestroyedPixel> Terrain::TakeReactions()
    {
        std::vector<DestroyedPixel> out;
        out.swap(m_Reactions);
        return out;
    }

    bool Terrain::CanDisplace(int x, int y, Material mover) const
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return false;

        const Material target = GetMaterial(x, y);

        if (target == Material::Air)
            return true;

        // Heavier things sink through lighter liquids.
        const MaterialInfo& targetInfo = GetMaterialInfo(target);

        return targetInfo.Behavior == MaterialBehavior::Liquid &&
            targetInfo.Density < GetMaterialInfo(mover).Density;
    }

    void Terrain::MoveCell(int fromX, int fromY, int toX, int toY)
    {
        const Material mover = GetMaterial(fromX, fromY);
        const Material displaced = GetMaterial(toX, toY);

        SetMaterial(toX, toY, mover);
        SetMaterial(fromX, fromY, displaced);

        m_MoveStamp[static_cast<std::size_t>(toY) * m_Width + toX] = m_Stamp;
        m_MoveStamp[static_cast<std::size_t>(fromY) * m_Width + fromX] = m_Stamp;
    }

    void Terrain::UpdateCell(int x, int y)
    {
        const std::size_t index = static_cast<std::size_t>(y) * m_Width + x;

        if (m_MoveStamp[index] == m_Stamp)
            return;

        const Material material = static_cast<Material>(m_Materials[index]);
        const MaterialInfo& info = GetMaterialInfo(material);

        if (info.Behavior != MaterialBehavior::Powder &&
            info.Behavior != MaterialBehavior::Liquid)
            return;

        // Lava meeting water: the water flashes to steam, the lava
        // hardens to obsidian.
        if (material == Material::Lava)
        {
            static constexpr int Offsets[4][2] = { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 } };

            for (const auto& offset : Offsets)
            {
                const int nx = x + offset[0];
                const int ny = y + offset[1];

                if (GetMaterial(nx, ny) == Material::Water)
                {
                    SetMaterial(nx, ny, Material::Air);
                    SetMaterial(x, y, Material::Obsidian);
                    m_Reactions.push_back({
                        static_cast<float>(nx), static_cast<float>(ny), Material::Water });
                    return;
                }
            }

        }

        // Which diagonal / side to try first alternates per cell and
        // tick, so piles and pools spread evenly instead of leaning.
        const int dir = ((HashPixel(static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y)) ^ m_SimTick) & 1u) ? 1 : -1;

        int toX = x;
        int toY = y;

        if (CanDisplace(x, y + 1, material))
        {
            toY = y + 1;
        }
        else if (CanDisplace(x + dir, y + 1, material) && CanDisplace(x + dir, y, material))
        {
            toX = x + dir;
            toY = y + 1;
        }
        else if (CanDisplace(x - dir, y + 1, material) && CanDisplace(x - dir, y, material))
        {
            toX = x - dir;
            toY = y + 1;
        }
        else if (info.Behavior == MaterialBehavior::Liquid)
        {
            // Liquids also flow sideways, a few pixels per tick, but only
            // toward somewhere lower to fall into. A level surface is at
            // rest, so a settled pool stops costing anything. Looking
            // well past the per-tick step keeps long pools from resting
            // on a visible slope.
            const bool lava = material == Material::Lava;
            const int step = lava ? 2 : 6;
            const int search = lava ? 24 : 64;

            for (int side = 0; side < 2 && toX == x; side++)
            {
                const int sign = side == 0 ? dir : -dir;

                for (int d = 1; d <= search; d++)
                {
                    const int sx = x + sign * d;

                    if (!CanDisplace(sx, y, material) || GetMaterial(sx, y) == material)
                        break;

                    if (CanDisplace(sx, y + 1, material))
                    {
                        toX = x + sign * std::min(d, step);
                        break;
                    }
                }
            }
        }

        if (toX == x && toY == y)
            return;

        // Thick and slow: lava only flows every third tick.
        if (material == Material::Lava && m_SimTick % 3u != 0u)
        {
            Wake(x, y);
            return;
        }

        MoveCell(x, y, toX, toY);
    }

    void Terrain::Simulate()
    {
        if (m_ChunkAwake.empty())
            return;

        m_SimTick++;
        m_Stamp = static_cast<std::uint8_t>(m_Stamp == 255 ? 1 : m_Stamp + 1);

        m_ChunkAwake.swap(m_ChunkAwakeNext);
        std::fill(m_ChunkAwakeNext.begin(), m_ChunkAwakeNext.end(), 0);

        // Bottom-up so a falling column moves as one; the horizontal
        // sweep direction alternates every tick to avoid drift.
        const bool leftToRight = (m_SimTick & 1u) != 0u;

        for (int cy = m_ChunksY - 1; cy >= 0; cy--)
        {
            const std::uint8_t* rowAwake =
                &m_ChunkAwake[static_cast<std::size_t>(cy) * m_ChunksX];

            bool any = false;

            for (int cx = 0; cx < m_ChunksX && !any; cx++)
                any = rowAwake[cx] != 0;

            if (!any)
                continue;

            const int yTop = cy * ChunkSize;
            const int yBottom = std::min(m_Height - 1, yTop + ChunkSize - 1);

            for (int y = yBottom; y >= yTop; y--)
            {
                for (int i = 0; i < m_ChunksX; i++)
                {
                    const int cx = leftToRight ? i : m_ChunksX - 1 - i;

                    if (!rowAwake[cx])
                        continue;

                    const int xStart = cx * ChunkSize;
                    const int xEnd = std::min(m_Width, xStart + ChunkSize);

                    if (leftToRight)
                    {
                        for (int x = xStart; x < xEnd; x++)
                            UpdateCell(x, y);
                    }
                    else
                    {
                        for (int x = xEnd - 1; x >= xStart; x--)
                            UpdateCell(x, y);
                    }
                }
            }
        }
    }

    void Terrain::RebuildAllPixels()
    {
        for (int y = 0; y < m_Height; y++)
        {
            for (int x = 0; x < m_Width; x++)
            {
                WritePixelColor(x, y, GetMaterial(x, y));
            }
        }
    }

    void Terrain::WritePixelColor(int x, int y, Material material)
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return;

        const std::size_t index =
            (static_cast<std::size_t>(y) * m_Width + x) * 4;

        const MaterialInfo& info = GetMaterialInfo(material);

        const std::uint32_t hash = HashPixel(
            static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y));

        const std::size_t cell = static_cast<std::size_t>(y) * m_Width + x;
        const Material wall = static_cast<Material>(m_Backwall[cell]);

        // The rock wall behind dug-out space: the same material, much
        // darker, with a coarse texture so tunnels read as cut into the
        // ground rather than holes onto the sky.
        float wallR = 0.0f;
        float wallG = 0.0f;
        float wallB = 0.0f;

        if (wall != Material::Air)
        {
            const MaterialInfo& wallInfo = GetMaterialInfo(wall);

            float f = 0.30f + static_cast<float>(HashPixel(
                static_cast<std::uint32_t>(x / 4),
                static_cast<std::uint32_t>(y / 3)) % 100u) * 0.0012f;

            f += static_cast<float>(hash % 100u) * 0.0004f;
            f *= static_cast<float>(m_DepthShade[cell]) / 255.0f;

            wallR = wallInfo.R * f;
            wallG = wallInfo.G * f;
            wallB = wallInfo.B * f * 1.08f;
        }

        if (info.Alpha == 0)
        {
            const bool hasWall = wall != Material::Air;

            m_Pixels[index + 0] = static_cast<std::uint8_t>(std::min(255.0f, wallR));
            m_Pixels[index + 1] = static_cast<std::uint8_t>(std::min(255.0f, wallG));
            m_Pixels[index + 2] = static_cast<std::uint8_t>(std::min(255.0f, wallB));
            m_Pixels[index + 3] = hasWall ? 255 : 0;
            return;
        }

        if (info.Behavior == MaterialBehavior::Liquid)
        {
            // Liquids: translucent, with a bright skin where they meet
            // the air. Lava ignores depth darkening - it glows.
            const Material above = GetMaterial(x, y - 1);
            const bool surfaceSkin = above != material && y > 0;

            float factor = 0.9f + static_cast<float>(hash % 1000u) * 0.0002f;

            if (material == Material::Lava)
            {
                factor = 0.8f + static_cast<float>(hash % 1000u) * 0.0004f;

                if (surfaceSkin)
                    factor *= 1.25f;
            }
            else
            {
                factor *= static_cast<float>(
                    m_DepthShade[static_cast<std::size_t>(y) * m_Width + x]) / 255.0f;

                if (surfaceSkin)
                    factor *= 1.55f;
            }

            const std::uint8_t alpha = surfaceSkin
                ? static_cast<std::uint8_t>(std::min(255, info.Alpha + 50))
                : info.Alpha;

            if (wall != Material::Air)
            {
                // Underground, blend over the rock wall ourselves so the
                // pixel stays opaque.
                const float a = static_cast<float>(alpha) / 255.0f;

                m_Pixels[index + 0] = static_cast<std::uint8_t>(std::min(255.0f,
                    Shade(info.R, factor) * a + wallR * (1.0f - a)));
                m_Pixels[index + 1] = static_cast<std::uint8_t>(std::min(255.0f,
                    Shade(info.G, factor) * a + wallG * (1.0f - a)));
                m_Pixels[index + 2] = static_cast<std::uint8_t>(std::min(255.0f,
                    Shade(info.B, factor) * a + wallB * (1.0f - a)));
                m_Pixels[index + 3] = 255;
                return;
            }

            m_Pixels[index + 0] = Shade(info.R, factor);
            m_Pixels[index + 1] = Shade(info.G, factor);
            m_Pixels[index + 2] = Shade(info.B, factor);
            m_Pixels[index + 3] = alpha;
            return;
        }

        // Per-pixel grain plus baked depth darkening.
        float factor = 0.85f + static_cast<float>(hash % 1000u) * 0.0003f;

        factor *= static_cast<float>(
            m_DepthShade[static_cast<std::size_t>(y) * m_Width + x]) / 255.0f;

        // Material texture patterns.
        switch (material)
        {
        case Material::Stone:
        {
            // Large soft blotches give stone a mottled look.
            const std::uint32_t blotch = HashPixel(
                static_cast<std::uint32_t>(x / 9),
                static_cast<std::uint32_t>(y / 7));

            factor *= 0.92f + static_cast<float>(blotch % 100u) * 0.0016f;
            break;
        }

        case Material::Dirt:
            // Occasional dark speck (pebbles, roots).
            if (hash % 19u == 0u)
                factor *= 0.72f;
            break;

        case Material::Gold:
            // Sparkle.
            if (hash % 11u == 0u)
                factor *= 1.75f;
            break;

        case Material::Sand:
            // Fine speckle of darker and lighter grains.
            if (hash % 7u == 0u)
                factor *= 0.86f;
            else if (hash % 13u == 0u)
                factor *= 1.1f;
            break;

        case Material::Gravel:
            // Pebbles: chunky 2x2 cells of varying tone.
            factor *= 0.75f + static_cast<float>(HashPixel(
                static_cast<std::uint32_t>(x / 2),
                static_cast<std::uint32_t>(y / 2)) % 100u) * 0.005f;
            break;

        case Material::Obsidian:
            // Glassy glints.
            if (hash % 17u == 0u)
                factor *= 1.9f;
            break;

        case Material::Bedrock:
            factor *= 0.8f + static_cast<float>(HashPixel(
                static_cast<std::uint32_t>(x / 5),
                static_cast<std::uint32_t>(y / 4)) % 100u) * 0.003f;
            break;

        case Material::Concrete:
            // Cast panels: dark seams on a 24x16 grid, a little grit.
            if (x % 24 == 0 || y % 16 == 0)
                factor *= 0.7f;
            else if (hash % 23u == 0u)
                factor *= 0.85f;
            break;

        case Material::Brain:
        {
            // Folds: a wavy dark crease pattern.
            const float fold = std::sin(static_cast<float>(x) * 0.9f +
                std::sin(static_cast<float>(y) * 0.7f) * 2.2f);

            factor = 0.95f + fold * 0.18f + static_cast<float>(hash % 100u) * 0.001f;
            break;
        }

        case Material::Glass:
            factor = (x + y) % 9 == 0 ? 1.25f : 1.0f;
            break;

        default:
            break;
        }

        // Directional light: sunlit top edges, dark undersides.
        const bool airAbove = !GetMaterialInfo(GetMaterial(x, y - 1)).Solid;
        const bool airBelow = !GetMaterialInfo(GetMaterial(x, y + 1)).Solid;

        if (airAbove && y > 0)
            factor *= material == Material::Grass ? 1.5f : 1.33f;
        else if (airBelow)
            factor *= 0.6f;

        // Chroma grain: each pixel leans slightly warm or cool instead
        // of only lighter/darker, so big fields of one material read as
        // natural color variation rather than grayscale noise.
        float chroma = 0.10f;

        switch (material)
        {
        case Material::Dirt: chroma = 0.15f; break;
        case Material::Stone: chroma = 0.09f; break;
        case Material::Grass: chroma = 0.12f; break;
        case Material::Gold: chroma = 0.05f; break;
        case Material::Sand: chroma = 0.08f; break;
        default: break;
        }

        const float warm =
            (static_cast<float>((hash >> 12) % 256u) / 255.0f - 0.5f) *
            chroma * 2.0f;

        float rFactor = factor * (1.0f + warm);
        float gFactor = factor;
        float bFactor = factor * (1.0f - warm);

        // Grass tilts yellow-green rather than red-blue.
        if (material == Material::Grass)
        {
            gFactor *= 1.0f + warm * 0.6f;
            bFactor = factor * (1.0f - warm * 0.4f);
        }

        if (info.Alpha < 255)
        {
            // See-through solids (glass) blend over whatever is behind.
            const float a = static_cast<float>(info.Alpha) / 255.0f;
            const bool hasWall = wall != Material::Air;

            m_Pixels[index + 0] = static_cast<std::uint8_t>(std::min(255.0f,
                Shade(info.R, rFactor) * a + wallR * (1.0f - a)));
            m_Pixels[index + 1] = static_cast<std::uint8_t>(std::min(255.0f,
                Shade(info.G, gFactor) * a + wallG * (1.0f - a)));
            m_Pixels[index + 2] = static_cast<std::uint8_t>(std::min(255.0f,
                Shade(info.B, bFactor) * a + wallB * (1.0f - a)));
            m_Pixels[index + 3] = hasWall ? 255 : info.Alpha;
            return;
        }

        m_Pixels[index + 0] = Shade(info.R, rFactor);
        m_Pixels[index + 1] = Shade(info.G, gFactor);
        m_Pixels[index + 2] = Shade(info.B, bFactor);
        m_Pixels[index + 3] = 255;
    }

    void Terrain::SetBackwall(int x, int y, Material material)
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return;

        m_Backwall[static_cast<std::size_t>(y) * m_Width + x] =
            static_cast<std::uint8_t>(material);

        WritePixelColor(x, y, GetMaterial(x, y));
        MarkDirty(x, y);
    }

    int Terrain::GetSurfaceY(int x) const
    {
        for (int y = 0; y < m_Height; y++)
        {
            if (GetMaterialInfo(GetMaterial(x, y)).Solid)
                return y;
        }

        return m_Height;
    }

    void Terrain::MarkDirty(int x, int y)
    {
        m_DirtyMinX = std::min(m_DirtyMinX, x);
        m_DirtyMinY = std::min(m_DirtyMinY, y);
        m_DirtyMaxX = std::max(m_DirtyMaxX, x);
        m_DirtyMaxY = std::max(m_DirtyMaxY, y);
    }

    void Terrain::ResetDirtyRegion()
    {
        m_DirtyMinX = m_Width;
        m_DirtyMinY = m_Height;
        m_DirtyMaxX = -1;
        m_DirtyMaxY = -1;
    }
}
