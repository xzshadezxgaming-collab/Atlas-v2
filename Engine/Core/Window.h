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

        // Like DrawFilledRect but in screen space (ignores the camera) —
        // for HUD elements.
        void DrawScreenRect(
            float x,
            float y,
            float width,
            float height,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a);

        // Soft additive light circle in world space (muzzle flashes,
        // fire, explosions).
        void DrawGlow(
            float x,
            float y,
            float radius,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a);

        // Same, in screen space (backdrop halos).
        void DrawGlowScreen(
            float x,
            float y,
            float radius,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a);

        void DrawTexture(
            SDL_Texture* texture,
            float x,
            float y,
            float width,
            float height,
            bool flipHorizontal = false);

        // Draws rotated around the center of the destination rect.
        void DrawTextureRotated(
            SDL_Texture* texture,
            float x,
            float y,
            float width,
            float height,
            float angleDegrees,
            bool flipHorizontal = false);

        SDL_Renderer* GetRenderer() const;

    private:
        SDL_Window* m_Window;
        SDL_Renderer* m_Renderer;

        const Camera* m_Camera;
    };
}