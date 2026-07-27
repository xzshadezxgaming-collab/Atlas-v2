#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace Atlas
{
    class Camera;

    class Window
    {
    public:
        Window();
        ~Window();

        bool Create(const std::string& title, int width, int height);
        void Destroy();

        bool PollEvents();

        void BeginFrame();
        void EndFrame();

        void SetTitle(const std::string& title);

        void SetCamera(const Camera* camera);

        void DrawFilledRect(
            float x,
            float y,
            float width,
            float height,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a);

        void DrawTexture(
            SDL_Texture* texture,
            float x,
            float y,
            float width,
            float height);

        SDL_Renderer* GetRenderer() const;

    private:
        SDL_Window* m_Window;
        SDL_Renderer* m_Renderer;

        const Camera* m_Camera;
    };
}