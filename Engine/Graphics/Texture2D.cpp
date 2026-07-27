#include "Texture2D.h"

#include <SDL3/SDL.h>
#include <iostream>

namespace Atlas
{
    Texture2D::Texture2D()
        : m_Texture(nullptr),
        m_Width(0),
        m_Height(0)
    {
    }

    Texture2D::~Texture2D()
    {
        Unload();
    }

    bool Texture2D::Load(SDL_Renderer* renderer, const std::string& filename)
    {
        Unload();

        SDL_Surface* surface = SDL_LoadBMP(filename.c_str());

        if (surface == nullptr)
        {
            std::cout << "Failed to load BMP: " << filename << std::endl;
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

        m_Width = surface->w;
        m_Height = surface->h;

        // Opaque images use magenta (255, 0, 255) as the transparent
        // color, the classic sprite color key.
        if (!SDL_ISPIXELFORMAT_ALPHA(surface->format))
        {
            SDL_SetSurfaceColorKey(
                surface,
                true,
                SDL_MapSurfaceRGB(surface, 255, 0, 255));
        }

        m_Texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_DestroySurface(surface);

        if (m_Texture == nullptr)
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

        // Pixel art stays crisp when scaled up.
        SDL_SetTextureScaleMode(m_Texture, SDL_SCALEMODE_NEAREST);

        return true;
    }

    void Texture2D::Unload()
    {
        if (m_Texture)
        {
            SDL_DestroyTexture(m_Texture);
            m_Texture = nullptr;
        }

        m_Width = 0;
        m_Height = 0;
    }

    SDL_Texture* Texture2D::GetTexture() const
    {
        return m_Texture;
    }

    int Texture2D::GetWidth() const
    {
        return m_Width;
    }

    int Texture2D::GetHeight() const
    {
        return m_Height;
    }
}