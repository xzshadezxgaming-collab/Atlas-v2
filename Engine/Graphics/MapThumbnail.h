#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace Atlas
{
    class Terrain;

    // A small picture of the whole terrain (the setup screen's map
    // preview, the in-game minimap). Rebuilt from the terrain's color
    // layer whenever Update is called.
    class MapThumbnail
    {
    public:
        MapThumbnail() = default;
        ~MapThumbnail();

        MapThumbnail(const MapThumbnail&) = delete;
        MapThumbnail& operator=(const MapThumbnail&) = delete;

        void Update(SDL_Renderer* renderer, const Terrain& terrain, int width, int height);

        // Draws it in screen space.
        void Draw(SDL_Renderer* renderer, float x, float y) const;

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }

    private:
        SDL_Texture* m_Texture = nullptr;
        int m_Width = 0;
        int m_Height = 0;
        std::vector<std::uint8_t> m_Buffer;
    };
}
