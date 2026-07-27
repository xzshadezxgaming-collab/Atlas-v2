#include "Window.h"
#include "../Graphics/Camera.h"

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

    SDL_Renderer* Window::GetRenderer() const
    {
        return m_Renderer;
    }
}