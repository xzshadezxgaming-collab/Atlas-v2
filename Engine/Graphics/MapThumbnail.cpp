#include "MapThumbnail.h"

#include "../World/Terrain.h"

namespace Atlas
{
    MapThumbnail::~MapThumbnail()
    {
        if (m_Texture)
            SDL_DestroyTexture(m_Texture);
    }

    void MapThumbnail::Update(SDL_Renderer* renderer, const Terrain& terrain, int width, int height)
    {
        if (width <= 0 || height <= 0 || terrain.GetWidth() <= 0)
            return;

        if (!m_Texture || width != m_Width || height != m_Height)
        {
            if (m_Texture)
                SDL_DestroyTexture(m_Texture);

            m_Texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STREAMING, width, height);

            if (!m_Texture)
                return;

            SDL_SetTextureBlendMode(m_Texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(m_Texture, SDL_SCALEMODE_NEAREST);

            m_Width = width;
            m_Height = height;
            m_Buffer.assign(static_cast<std::size_t>(width) * height * 4, 0);
        }

        const std::uint8_t* pixels = terrain.GetPixelData();
        const int terrainWidth = terrain.GetWidth();
        const int terrainHeight = terrain.GetHeight();

        // Average four samples per thumbnail pixel; open space (sky, dug
        // tunnels with nothing behind) shows as a dark wash.
        for (int ty = 0; ty < height; ty++)
        {
            for (int tx = 0; tx < width; tx++)
            {
                int r = 0, g = 0, b = 0, solid = 0;

                for (int s = 0; s < 4; s++)
                {
                    const int x = ((tx * 4 + (s % 2) * 2 + 1) * terrainWidth) / (width * 4);
                    const int y = ((ty * 4 + (s / 2) * 2 + 1) * terrainHeight) / (height * 4);
                    const std::uint8_t* p = pixels + (static_cast<std::size_t>(y) * terrainWidth + x) * 4;

                    if (p[3] > 0)
                    {
                        r += p[0];
                        g += p[1];
                        b += p[2];
                        solid++;
                    }
                }

                std::uint8_t* out = &m_Buffer[(static_cast<std::size_t>(ty) * width + tx) * 4];

                if (solid == 0)
                {
                    out[0] = 16; out[1] = 20; out[2] = 34; out[3] = 200;
                }
                else
                {
                    out[0] = static_cast<std::uint8_t>(r / solid);
                    out[1] = static_cast<std::uint8_t>(g / solid);
                    out[2] = static_cast<std::uint8_t>(b / solid);
                    out[3] = 255;
                }
            }
        }

        SDL_UpdateTexture(m_Texture, nullptr, m_Buffer.data(), width * 4);
    }

    void MapThumbnail::Draw(SDL_Renderer* renderer, float x, float y) const
    {
        if (!m_Texture)
            return;

        const SDL_FRect dest{ x, y, static_cast<float>(m_Width), static_cast<float>(m_Height) };
        SDL_RenderTexture(renderer, m_Texture, nullptr, &dest);
    }
}
