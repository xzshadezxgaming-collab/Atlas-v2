#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace Atlas
{
    class Texture2D
    {
    public:
        Texture2D();
        ~Texture2D();

        bool Load(SDL_Renderer* renderer, const std::string& filename);

        void Unload();

        SDL_Texture* GetTexture() const;

        int GetWidth() const;
        int GetHeight() const;

    private:
        SDL_Texture* m_Texture;

        int m_Width;
        int m_Height;
    };
}