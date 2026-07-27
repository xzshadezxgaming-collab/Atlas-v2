#pragma once

#include "Texture2D.h"

#include "../ECS/Transform.h"

#include <SDL3/SDL.h>
#include <string>

namespace Atlas
{
    class Window;

    class Sprite
    {
    public:
        Sprite();

        bool Load(SDL_Renderer* renderer, const std::string& filename);

        void SetTransform(Transform* transform);

        void SetSize(float width, float height);

        // Mirrors the sprite horizontally; art is authored facing right.
        void SetFlipX(bool flip);
        bool GetFlipX() const;

        float GetWidth() const;
        float GetHeight() const;

        Texture2D& GetTexture();

        void Draw(Window& window);

    private:
        Texture2D m_Texture;

        Transform* m_Transform;

        float m_Width;
        float m_Height;

        bool m_FlipX;
    };
}