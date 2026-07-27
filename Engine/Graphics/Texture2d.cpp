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

        m_Texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_DestroySurface(surface);

        if (m_Texture == nullptr)
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

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