#include "Window.h"

#include "../Graphics/Camera.h"
#include "../Graphics/SpriteBank.h"
#include "../Input/Input.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Atlas
{
    Window::Window()
        : m_Window(nullptr),
        m_Renderer(nullptr),
        m_Camera(nullptr)
    {
    }

    Window::~Window()
    {
        Destroy();
    }

    bool Window::Create(const std::string& title, int width, int height)
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

        m_Window = SDL_CreateWindow(
            title.c_str(),
            width,
            height,
            0
        );

        if (!m_Window)
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

        m_Renderer = SDL_CreateRenderer(m_Window, nullptr);

        if (!m_Renderer)
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }

        return true;
    }

    void Window::Destroy()
    {
        if (m_Renderer)
        {
            SDL_DestroyRenderer(m_Renderer);
            m_Renderer = nullptr;
        }

        if (m_Window)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }

        SDL_Quit();
    }

    bool Window::PollEvents()
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                return false;

            if (event.type == SDL_EVENT_MOUSE_WHEEL)
                Input::AccumulateWheel(event.wheel.y);

            if (event.type == SDL_EVENT_TEXT_INPUT)
                Input::AppendText(event.text.text);
        }

        return true;
    }

    void Window::BeginFrame()
    {
        SDL_SetRenderDrawColor(m_Renderer, 40, 40, 40, 255);
        SDL_RenderClear(m_Renderer);
    }

    void Window::EndFrame()
    {
        SDL_RenderPresent(m_Renderer);
    }

    void Window::SetFullscreen(bool fullscreen)
    {
        if (m_Window)
            SDL_SetWindowFullscreen(m_Window, fullscreen);
    }

    void Window::SetTextInput(bool enabled)
    {
        if (!m_Window)
            return;

        if (enabled && !SDL_TextInputActive(m_Window))
            SDL_StartTextInput(m_Window);
        else if (!enabled && SDL_TextInputActive(m_Window))
            SDL_StopTextInput(m_Window);
    }

    void Window::SetTitle(const std::string& title)
    {
        SDL_SetWindowTitle(m_Window, title.c_str());
    }

    void Window::SetCamera(const Camera* camera)
    {
        m_Camera = camera;
    }

    void Window::DrawFilledRect(
        float x,
        float y,
        float width,
        float height,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a)
    {
        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        SDL_FRect rect;
        rect.x = x;
        rect.y = y;
        rect.w = width;
        rect.h = height;

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_Renderer, r, g, b, a);
        SDL_RenderFillRect(m_Renderer, &rect);
    }

    void Window::DrawScreenRect(
        float x,
        float y,
        float width,
        float height,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a)
    {
        SDL_FRect rect;
        rect.x = x;
        rect.y = y;
        rect.w = width;
        rect.h = height;

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_Renderer, r, g, b, a);
        SDL_RenderFillRect(m_Renderer, &rect);
    }

    namespace
    {
        void FillGlowCircle(
            SDL_Renderer* renderer,
            float x,
            float y,
            float radius,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a)
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

            // Two nested discs: soft outer, brighter core.
            for (int pass = 0; pass < 2; pass++)
            {
                const float passRadius = pass == 0 ? radius : radius * 0.5f;
                const Uint8 passAlpha = pass == 0
                    ? a
                    : static_cast<Uint8>(std::min(255, a * 2));

                SDL_SetRenderDrawColor(renderer, r, g, b, passAlpha);

                for (float dy = -passRadius; dy <= passRadius; dy += 2.0f)
                {
                    const float half = std::sqrt(
                        std::max(0.0f, passRadius * passRadius - dy * dy));

                    SDL_FRect rect;
                    rect.x = x - half;
                    rect.y = y + dy;
                    rect.w = half * 2.0f;
                    rect.h = 2.0f;

                    SDL_RenderFillRect(renderer, &rect);
                }
            }

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        }
    }

    void Window::DrawGlow(
        float x,
        float y,
        float radius,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a)
    {
        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        FillGlowCircle(m_Renderer, x, y, radius, r, g, b, a);
    }

    void Window::DrawGlowScreen(
        float x,
        float y,
        float radius,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a)
    {
        FillGlowCircle(m_Renderer, x, y, radius, r, g, b, a);
    }

    void Window::DrawTextureRotated(
        SDL_Texture* texture,
        float x,
        float y,
        float width,
        float height,
        float angleDegrees,
        bool flipHorizontal)
    {
        if (!texture)
            return;

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        SDL_FRect destination;
        destination.x = x;
        destination.y = y;
        destination.w = width;
        destination.h = height;

        SDL_RenderTextureRotated(
            m_Renderer,
            texture,
            nullptr,
            &destination,
            static_cast<double>(angleDegrees),
            nullptr,
            flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    void Window::DrawTexture(
        SDL_Texture* texture,
        float x,
        float y,
        float width,
        float height,
        bool flipHorizontal)
    {
        if (!texture)
            return;

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        SDL_FRect destination;
        destination.x = x;
        destination.y = y;
        destination.w = width;
        destination.h = height;

        SDL_RenderTextureRotated(
            m_Renderer,
            texture,
            nullptr,
            &destination,
            0.0,
            nullptr,
            flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    void Window::DrawSprite(
        const SpriteFrame& sprite,
        float x,
        float y,
        float angleDegrees,
        bool flipX,
        bool flipY,
        float scale,
        Uint8 flash)
    {
        if (!sprite.Texture)
            return;

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        // The pivot is a pixel; aim for its centre. Flipping mirrors the
        // image inside the destination rect, so the pivot moves with it.
        const float pivotX = (flipX ? sprite.Width - sprite.PivotX - 1.0f : sprite.PivotX) + 0.5f;
        const float pivotY = (flipY ? sprite.Height - sprite.PivotY - 1.0f : sprite.PivotY) + 0.5f;

        SDL_FRect destination;
        destination.x = x - pivotX * scale;
        destination.y = y - pivotY * scale;
        destination.w = sprite.Width * scale;
        destination.h = sprite.Height * scale;

        const SDL_FPoint center = { pivotX * scale, pivotY * scale };

        const SDL_FlipMode flip = static_cast<SDL_FlipMode>(
            (flipX ? SDL_FLIP_HORIZONTAL : 0) | (flipY ? SDL_FLIP_VERTICAL : 0));

        SDL_RenderTextureRotated(
            m_Renderer, sprite.Texture, nullptr, &destination,
            static_cast<double>(angleDegrees), &center, flip);

        if (flash > 0)
        {
            Uint8 r = 255, g = 255, b = 255;
            SDL_GetTextureColorMod(sprite.Texture, &r, &g, &b);

            SDL_SetTextureBlendMode(sprite.Texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(sprite.Texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(sprite.Texture, flash);

            SDL_RenderTextureRotated(
                m_Renderer, sprite.Texture, nullptr, &destination,
                static_cast<double>(angleDegrees), &center, flip);

            SDL_SetTextureBlendMode(sprite.Texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(sprite.Texture, 255);
            SDL_SetTextureColorMod(sprite.Texture, r, g, b);
        }
    }

    void Window::DrawSpriteScreen(
        const SpriteFrame& sprite,
        float x,
        float y,
        float scale,
        Uint8 alpha)
    {
        if (!sprite.Texture)
            return;

        SDL_FRect destination;
        destination.x = x;
        destination.y = y;
        destination.w = sprite.Width * scale;
        destination.h = sprite.Height * scale;

        SDL_SetTextureAlphaMod(sprite.Texture, alpha);
        SDL_RenderTexture(m_Renderer, sprite.Texture, nullptr, &destination);
        SDL_SetTextureAlphaMod(sprite.Texture, 255);
    }

    SDL_Renderer* Window::GetRenderer() const
    {
        return m_Renderer;
    }
}