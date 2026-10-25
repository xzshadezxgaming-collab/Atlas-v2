#include "Window.h"

#include "../Graphics/Camera.h"
#include "../Graphics/SpriteBank.h"
#include "../Input/Input.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

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

        // Present in step with the display. Uncapped, frames land at
        // uneven points between 120 Hz simulation ticks, so the player
        // judders against the smoothly moving camera (and the image
        // tears). Not every driver supports it; that's fine.
        SDL_SetRenderVSync(m_Renderer, 1);

        // The game always draws a fixed-size view; fullscreen or a
        // resized window scales it (letterboxed) instead of leaving it in
        // a corner. The mouse is mapped back into view coordinates.
        m_Width = width;
        m_Height = height;
        SDL_SetRenderLogicalPresentation(
            m_Renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        Input::SetRenderer(m_Renderer);

        CreateEffectTextures();

        return true;
    }

    void Window::CreateEffectTextures()
    {
        // Soft round falloff: the shared stamp for glows, lights and
        // smoke puffs. White, with the shape in alpha.
        {
            constexpr int Size = 64;
            std::vector<Uint32> pixels(Size * Size);

            for (int y = 0; y < Size; y++)
            {
                for (int x = 0; x < Size; x++)
                {
                    const float dx = (static_cast<float>(x) + 0.5f) / (Size * 0.5f) - 1.0f;
                    const float dy = (static_cast<float>(y) + 0.5f) / (Size * 0.5f) - 1.0f;
                    const float d = std::min(1.0f, std::sqrt(dx * dx + dy * dy));
                    const float body = (1.0f - d * d) * (1.0f - d * d);
                    const float core = std::pow(1.0f - d, 4.0f);
                    const float a = std::clamp(body * 0.7f + core * 0.3f, 0.0f, 1.0f);

                    pixels[static_cast<std::size_t>(y) * Size + x] =
                        (static_cast<Uint32>(a * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
                }
            }

            m_GlowTexture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STATIC, Size, Size);

            if (m_GlowTexture)
            {
                SDL_UpdateTexture(m_GlowTexture, nullptr, pixels.data(), Size * 4);
                SDL_SetTextureScaleMode(m_GlowTexture, SDL_SCALEMODE_LINEAR);
            }
        }

        // Vignette: clear in the middle, darkening toward the corners.
        {
            constexpr int W = 160;
            constexpr int H = 90;
            std::vector<Uint32> pixels(W * H);

            for (int y = 0; y < H; y++)
            {
                for (int x = 0; x < W; x++)
                {
                    const float u = ((static_cast<float>(x) + 0.5f) / W - 0.5f) * 2.0f;
                    const float v = ((static_cast<float>(y) + 0.5f) / H - 0.5f) * 2.0f;
                    const float d = std::sqrt(u * u * 0.8f + v * v);
                    const float t = std::clamp((d - 0.55f) / 0.75f, 0.0f, 1.0f);
                    const float a = t * t * (3.0f - 2.0f * t);

                    pixels[static_cast<std::size_t>(y) * W + x] =
                        (static_cast<Uint32>(a * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
                }
            }

            m_VignetteTexture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STATIC, W, H);

            if (m_VignetteTexture)
            {
                SDL_UpdateTexture(m_VignetteTexture, nullptr, pixels.data(), W * 4);
                SDL_SetTextureScaleMode(m_VignetteTexture, SDL_SCALEMODE_LINEAR);
                SDL_SetTextureBlendMode(m_VignetteTexture, SDL_BLENDMODE_BLEND);
            }
        }

        // Light map at half resolution; bilinear upscaling softens it.
        m_LightMap = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET, std::max(1, m_Width / 2), std::max(1, m_Height / 2));

        if (m_LightMap)
        {
            SDL_SetTextureScaleMode(m_LightMap, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(m_LightMap, SDL_BLENDMODE_MOD);
        }
        else
        {
            // No render targets on this driver: play unlit.
            m_LightingEnabled = false;
        }
    }

    void Window::Destroy()
    {
        for (SDL_Texture** texture : { &m_GlowTexture, &m_VignetteTexture, &m_LightMap })
        {
            if (*texture)
            {
                SDL_DestroyTexture(*texture);
                *texture = nullptr;
            }
        }

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

        m_Lights.clear();
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

    const Camera* Window::GetCamera() const
    {
        return m_Camera;
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

    void Window::DrawGlowTexture(
        float x, float y, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (!m_GlowTexture || radius <= 0.0f || a == 0)
            return;

        // The stamp's soft falloff reaches a little past the old hard
        // disc, and is brighter in the middle to carry the same energy.
        const float half = radius * 1.3f;
        const SDL_FRect destination = { x - half, y - half, half * 2.0f, half * 2.0f };

        SDL_SetTextureBlendMode(m_GlowTexture, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(m_GlowTexture, r, g, b);
        SDL_SetTextureAlphaMod(m_GlowTexture,
            static_cast<Uint8>(std::min(255, static_cast<int>(a) * 5 / 2)));
        SDL_RenderTexture(m_Renderer, m_GlowTexture, nullptr, &destination);
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
        // Anything that glows also lights its surroundings.
        AddLight(x, y, radius * 2.4f, r, g, b,
            std::min(1.0f, static_cast<float>(a) / 90.0f));

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        DrawGlowTexture(x, y, radius, r, g, b, a);
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
        DrawGlowTexture(x, y, radius, r, g, b, a);
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

    namespace
    {
        SDL_FColor ToFColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }

        SDL_FColor ToFColor(SDL_Color c)
        {
            return ToFColor(c.r, c.g, c.b, c.a);
        }

        SDL_Vertex Vertex(float x, float y, SDL_FColor color)
        {
            SDL_Vertex v;
            v.position = { x, y };
            v.color = color;
            v.tex_coord = { 0.0f, 0.0f };
            return v;
        }

        void PushQuad(std::vector<SDL_Vertex>& out, const SDL_Vertex& a, const SDL_Vertex& b,
            const SDL_Vertex& c, const SDL_Vertex& d)
        {
            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
            out.push_back(a);
            out.push_back(c);
            out.push_back(d);
        }
    }

    void Window::DrawGradientScreen(
        float x, float y, float width, float height, SDL_Color top, SDL_Color bottom)
    {
        const SDL_FColor t = ToFColor(top);
        const SDL_FColor b = ToFColor(bottom);

        const SDL_Vertex vertices[4] = {
            Vertex(x, y, t),
            Vertex(x + width, y, t),
            Vertex(x + width, y + height, b),
            Vertex(x, y + height, b),
        };
        const int indices[6] = { 0, 1, 2, 0, 2, 3 };

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(m_Renderer, nullptr, vertices, 4, indices, 6);
    }

    void Window::DrawSoft(float x, float y, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (!m_GlowTexture || radius <= 0.0f || a == 0)
            return;

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        const SDL_FRect destination = { x - radius, y - radius, radius * 2.0f, radius * 2.0f };

        SDL_SetTextureBlendMode(m_GlowTexture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(m_GlowTexture, r, g, b);
        SDL_SetTextureAlphaMod(m_GlowTexture, a);
        SDL_RenderTexture(m_Renderer, m_GlowTexture, nullptr, &destination);
    }

    void Window::DrawSoftScreen(
        float x, float y, float radiusX, float radiusY, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (!m_GlowTexture || radiusX <= 0.0f || radiusY <= 0.0f || a == 0)
            return;

        const SDL_FRect destination = { x - radiusX, y - radiusY, radiusX * 2.0f, radiusY * 2.0f };

        SDL_SetTextureBlendMode(m_GlowTexture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(m_GlowTexture, r, g, b);
        SDL_SetTextureAlphaMod(m_GlowTexture, a);
        SDL_RenderTexture(m_Renderer, m_GlowTexture, nullptr, &destination);
    }

    void Window::DrawTrianglesScreen(const std::vector<SDL_Vertex>& vertices)
    {
        if (vertices.size() < 3)
            return;

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(m_Renderer, nullptr, vertices.data(),
            static_cast<int>(vertices.size() / 3 * 3), nullptr, 0);
    }

    void Window::DrawRing(
        float x, float y, float radius, float thickness, float squash,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (radius <= 0.0f || a == 0)
            return;

        if (m_Camera)
        {
            x -= m_Camera->GetX();
            y -= m_Camera->GetY();
        }

        constexpr int Segments = 40;
        const SDL_FColor clear = ToFColor(r, g, b, 0);
        const SDL_FColor lit = ToFColor(r, g, b, a);

        std::vector<SDL_Vertex> vertices;
        vertices.reserve(Segments * 12);

        const float inner = std::max(0.0f, radius - thickness);
        const float outer = radius + thickness * 0.5f;

        for (int i = 0; i < Segments; i++)
        {
            const float a0 = static_cast<float>(i) * 6.2831853f / Segments;
            const float a1 = static_cast<float>(i + 1) * 6.2831853f / Segments;
            const float c0 = std::cos(a0), s0 = std::sin(a0) * squash;
            const float c1 = std::cos(a1), s1 = std::sin(a1) * squash;

            // Soft on both sides: inner -> crest -> outer.
            PushQuad(vertices,
                Vertex(x + c0 * inner, y + s0 * inner, clear),
                Vertex(x + c1 * inner, y + s1 * inner, clear),
                Vertex(x + c1 * radius, y + s1 * radius, lit),
                Vertex(x + c0 * radius, y + s0 * radius, lit));
            PushQuad(vertices,
                Vertex(x + c0 * radius, y + s0 * radius, lit),
                Vertex(x + c1 * radius, y + s1 * radius, lit),
                Vertex(x + c1 * outer, y + s1 * outer, clear),
                Vertex(x + c0 * outer, y + s0 * outer, clear));
        }

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_ADD);
        SDL_RenderGeometry(m_Renderer, nullptr, vertices.data(),
            static_cast<int>(vertices.size()), nullptr, 0);
        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
    }

    void Window::DrawStreak(
        float x0, float y0, float x1, float y1, float width,
        Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (m_Camera)
        {
            x0 -= m_Camera->GetX();
            y0 -= m_Camera->GetY();
            x1 -= m_Camera->GetX();
            y1 -= m_Camera->GetY();
        }

        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float length = std::sqrt(dx * dx + dy * dy);

        if (length < 0.01f)
            return;

        const float nx = -dy / length;
        const float ny = dx / length;
        const float head = width * 0.5f;
        const float tail = width * 0.15f;

        const SDL_FColor hot = ToFColor(r, g, b, a);
        const SDL_FColor clear = ToFColor(r, g, b, 0);

        const SDL_Vertex vertices[4] = {
            Vertex(x0 + nx * head, y0 + ny * head, hot),
            Vertex(x1 + nx * tail, y1 + ny * tail, clear),
            Vertex(x1 - nx * tail, y1 - ny * tail, clear),
            Vertex(x0 - nx * head, y0 - ny * head, hot),
        };
        const int indices[6] = { 0, 1, 2, 0, 2, 3 };

        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_ADD);
        SDL_RenderGeometry(m_Renderer, nullptr, vertices, 4, indices, 6);
        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);
    }

    void Window::DrawVignette(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        if (!m_VignetteTexture || a == 0)
            return;

        SDL_SetTextureColorMod(m_VignetteTexture, r, g, b);
        SDL_SetTextureAlphaMod(m_VignetteTexture, a);
        SDL_RenderTexture(m_Renderer, m_VignetteTexture, nullptr, nullptr);
    }

    void Window::SetLightingEnabled(bool enabled)
    {
        m_LightingEnabled = enabled && m_LightMap;
    }

    bool Window::IsLightingEnabled() const
    {
        return m_LightingEnabled;
    }

    void Window::AddLight(
        float x, float y, float radius, Uint8 r, Uint8 g, Uint8 b, float intensity)
    {
        // Plenty for a firefight; past it, the dimmest extra lights are
        // the ones nobody would miss.
        constexpr std::size_t MaxLights = 384;

        if (!m_LightingEnabled || m_Lights.size() >= MaxLights || intensity <= 0.01f)
            return;

        m_Lights.push_back({ x, y, radius, r, g, b, std::min(1.0f, intensity) });
    }

    void Window::ApplyLighting(const AmbientLight& ambient)
    {
        if (!m_LightingEnabled || !m_LightMap)
        {
            m_Lights.clear();
            return;
        }

        const float camX = m_Camera ? m_Camera->GetX() : 0.0f;
        const float camY = m_Camera ? m_Camera->GetY() : 0.0f;
        constexpr float Scale = 0.5f;

        SDL_Texture* previous = SDL_GetRenderTarget(m_Renderer);
        SDL_SetRenderTarget(m_Renderer, m_LightMap);

        SDL_SetRenderDrawColor(m_Renderer,
            ambient.SkyAmbient.r, ambient.SkyAmbient.g, ambient.SkyAmbient.b, 255);
        SDL_RenderClear(m_Renderer);

        // Underground fades to the cave ambient below the surface line.
        if (ambient.SurfaceY && ambient.Columns > 1)
        {
            const SDL_FColor sky = ToFColor(ambient.SkyAmbient);
            const SDL_FColor cave = ToFColor(ambient.CaveAmbient);
            const float bottom = static_cast<float>(m_Height) * Scale + 4.0f;

            std::vector<SDL_Vertex> vertices;
            vertices.reserve(static_cast<std::size_t>(ambient.Columns) * 12);

            for (int i = 0; i + 1 < ambient.Columns; i++)
            {
                const float x0 = (ambient.FirstColumnX + (static_cast<float>(i) + 0.5f) *
                    ambient.ColumnWidth - camX) * Scale;
                const float x1 = x0 + ambient.ColumnWidth * Scale;
                const float s0 = (ambient.SurfaceY[i] - camY) * Scale;
                const float s1 = (ambient.SurfaceY[i + 1] - camY) * Scale;
                const float fade = ambient.Fade * Scale;

                PushQuad(vertices,
                    Vertex(x0, s0, sky), Vertex(x1, s1, sky),
                    Vertex(x1, s1 + fade, cave), Vertex(x0, s0 + fade, cave));
                PushQuad(vertices,
                    Vertex(x0, s0 + fade, cave), Vertex(x1, s1 + fade, cave),
                    Vertex(x1, std::max(bottom, s1 + fade), cave),
                    Vertex(x0, std::max(bottom, s0 + fade), cave));
            }

            SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_NONE);
            SDL_RenderGeometry(m_Renderer, nullptr, vertices.data(),
                static_cast<int>(vertices.size()), nullptr, 0);
        }

        if (m_GlowTexture)
        {
            SDL_SetTextureBlendMode(m_GlowTexture, SDL_BLENDMODE_ADD);

            for (const Light& light : m_Lights)
            {
                const float half = light.Radius * Scale;
                const float cx = (light.X - camX) * Scale;
                const float cy = (light.Y - camY) * Scale;

                if (cx + half < 0.0f || cy + half < 0.0f ||
                    cx - half > m_Width * Scale || cy - half > m_Height * Scale)
                    continue;

                const SDL_FRect destination = { cx - half, cy - half, half * 2.0f, half * 2.0f };

                SDL_SetTextureColorMod(m_GlowTexture, light.R, light.G, light.B);
                SDL_SetTextureAlphaMod(m_GlowTexture,
                    static_cast<Uint8>(std::clamp(light.Intensity * 255.0f, 0.0f, 255.0f)));
                SDL_RenderTexture(m_Renderer, m_GlowTexture, nullptr, &destination);
            }
        }

        SDL_SetRenderTarget(m_Renderer, previous);
        SDL_SetRenderDrawBlendMode(m_Renderer, SDL_BLENDMODE_BLEND);

        // Multiply the light map over the scene.
        SDL_RenderTexture(m_Renderer, m_LightMap, nullptr, nullptr);

        m_Lights.clear();
    }

    SDL_Renderer* Window::GetRenderer() const
    {
        return m_Renderer;
    }

    int Window::GetWidth() const
    {
        return m_Width;
    }

    int Window::GetHeight() const
    {
        return m_Height;
    }
}