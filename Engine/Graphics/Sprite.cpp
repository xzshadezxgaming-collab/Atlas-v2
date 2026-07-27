#include "Sprite.h"

#include "../Core/Window.h"

namespace Atlas
{
    Sprite::Sprite()
        : m_Transform(nullptr),
        m_Width(64.0f),
        m_Height(64.0f)
    {
    }

    bool Sprite::Load(SDL_Renderer* renderer, const std::string& filename)
    {
        return m_Texture.Load(renderer, filename);
    }

    void Sprite::SetTransform(Transform* transform)
    {
        m_Transform = transform;
    }

    void Sprite::SetSize(float width, float height)
    {
        m_Width = width;
        m_Height = height;
    }

    float Sprite::GetWidth() const
    {
        return m_Width;
    }

    float Sprite::GetHeight() const
    {
        return m_Height;
    }

    Texture2D& Sprite::GetTexture()
    {
        return m_Texture;
    }

    void Sprite::Draw(Window& window)
    {
        if (!m_Transform)
            return;

        if (!m_Texture.GetTexture())
            return;

        window.DrawTexture(
            m_Texture.GetTexture(),
            m_Transform->GetX(),
            m_Transform->GetY(),
            m_Width,
            m_Height);
    }
}