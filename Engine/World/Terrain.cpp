#include "Terrain.h"

#include "../Core/Window.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

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
        SDL_Renderer* renderer,
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

        m_Texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height);

        if (!m_Texture)
        {
            std::cout << "Failed to create terrain texture: "
                << SDL_GetError() << std::endl;
            return false;
        }

        SDL_SetTextureBlendMode(m_Texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(m_Texture, SDL_SCALEMODE_NEAREST);

        Generate(seed);
        RebuildAllPixels();

        m_DirtyAll = true;
        ResetDirtyRegion();

        return true;
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

        WritePixelColor(x, y, material);
        MarkDirty(x, y);
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

                if (!GetMaterialInfo(GetMaterial(x, y)).Solid)
                    continue;

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
        int maxSamples)
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

                if (!GetMaterialInfo(material).Solid)
                    continue;

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

        if (!GetMaterialInfo(material).Solid)
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

    void Terrain::Generate(unsigned int seed)
    {
        std::mt19937 rng(seed);

        // Rolling surface height built from a few octaves of sine waves,
        // each with a random phase so every seed produces a new landscape.
        std::uniform_real_distribution<float> phase(0.0f, 6.28318f);

        const float phase1 = phase(rng);
        const float phase2 = phase(rng);
        const float phase3 = phase(rng);

        const float baseHeight = static_cast<float>(m_Height) * 0.45f;

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
                m_Height - 64);
        }

        // Grass on top, a band of dirt, then stone all the way down.
        constexpr int GrassDepth = 5;
        constexpr int DirtDepth = 90;

        for (int x = 0; x < m_Width; x++)
        {
            for (int y = surface[x]; y < m_Height; y++)
            {
                const int depth = y - surface[x];

                Material material = Material::Stone;

                if (depth < GrassDepth)
                    material = Material::Grass;
                else if (depth < DirtDepth)
                    material = Material::Dirt;

                m_Materials[static_cast<std::size_t>(y) * m_Width + x] =
                    static_cast<std::uint8_t>(material);
            }
        }

        // Gold veins: blobs of gold scattered through the stone layer.
        std::uniform_int_distribution<int> veinX(0, m_Width - 1);
        std::uniform_int_distribution<int> veinRadius(3, 9);

        const int veinCount = m_Width / 12;

        for (int i = 0; i < veinCount; i++)
        {
            const int x = veinX(rng);

            const int stoneTop = surface[x] + DirtDepth;

            if (stoneTop >= m_Height - 8)
                continue;

            std::uniform_int_distribution<int> veinY(stoneTop + 8, m_Height - 8);

            const int y = veinY(rng);
            const int radius = veinRadius(rng);

            for (int py = y - radius; py <= y + radius; py++)
            {
                for (int px = x - radius; px <= x + radius; px++)
                {
                    if (px < 0 || px >= m_Width || py < 0 || py >= m_Height)
                        continue;

                    const int dx = px - x;
                    const int dy = py - y;

                    if (dx * dx + dy * dy > radius * radius)
                        continue;

                    std::size_t index =
                        static_cast<std::size_t>(py) * m_Width + px;

                    if (m_Materials[index] ==
                        static_cast<std::uint8_t>(Material::Stone))
                    {
                        m_Materials[index] =
                            static_cast<std::uint8_t>(Material::Gold);
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
        const std::size_t index =
            (static_cast<std::size_t>(y) * m_Width + x) * 4;

        const MaterialInfo& info = GetMaterialInfo(material);

        if (!info.Solid)
        {
            m_Pixels[index + 0] = 0;
            m_Pixels[index + 1] = 0;
            m_Pixels[index + 2] = 0;
            m_Pixels[index + 3] = 0;
            return;
        }

        const std::uint32_t hash = HashPixel(
            static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y));

        const float factor = 0.85f + static_cast<float>(hash % 1000u) * 0.0003f;

        m_Pixels[index + 0] = Shade(info.R, factor);
        m_Pixels[index + 1] = Shade(info.G, factor);
        m_Pixels[index + 2] = Shade(info.B, factor);
        m_Pixels[index + 3] = 255;
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
