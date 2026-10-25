#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace Atlas
{
    class Camera;
    struct SpriteFrame;

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

        void SetFullscreen(bool fullscreen);

        // Typed characters arrive as text events only while enabled.
        void SetTextInput(bool enabled);

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

        // Draws a sprite-pack frame with its pivot at (x, y), rotated
        // around the pivot. Art faces right; flipY keeps a weapon upright
        // when it is aimed to the left. flash > 0 adds a white pass on
        // top (hit flashes).
        void DrawSprite(
            const SpriteFrame& sprite,
            float x,
            float y,
            float angleDegrees = 0.0f,
            bool flipX = false,
            bool flipY = false,
            float scale = 1.0f,
            Uint8 flash = 0);

        // Same, in screen space (HUD icons).
        void DrawSpriteScreen(
            const SpriteFrame& sprite,
            float x,
            float y,
            float scale = 1.0f,
            Uint8 alpha = 255);

        SDL_Renderer* GetRenderer() const;

    private:
        SDL_Window* m_Window;
        SDL_Renderer* m_Renderer;

        const Camera* m_Camera;
    };
}