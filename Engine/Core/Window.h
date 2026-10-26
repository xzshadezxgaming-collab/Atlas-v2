#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace Atlas
{
    class Camera;
    struct SpriteFrame;

    // How the light map is lit before any lights are added: open sky
    // gets SkyAmbient, and below the ground surface it fades to
    // CaveAmbient over Fade pixels. SurfaceY holds, per column of
    // ColumnWidth world pixels starting at FirstColumnX, the world y of
    // the first solid pixel from the top.
    struct AmbientLight
    {
        SDL_Color SkyAmbient = { 255, 255, 255, 255 };
        SDL_Color CaveAmbient = { 255, 255, 255, 255 };
        const float* SurfaceY = nullptr;
        int Columns = 0;
        float ColumnWidth = 8.0f;
        float FirstColumnX = 0.0f;
        float Fade = 120.0f;
    };

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
        const Camera* GetCamera() const;

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

        // --- Smooth primitives (vertex-colored geometry, no banding) ---

        // Vertical gradient rect in screen space.
        void DrawGradientScreen(
            float x, float y, float width, float height,
            SDL_Color top, SDL_Color bottom);

        // Soft round puff in world space, alpha-blended (smoke, dust).
        void DrawSoft(
            float x, float y, float radius,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        // Same, in screen space (clouds, haze).
        void DrawSoftScreen(
            float x, float y, float radiusX, float radiusY,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        // Filled vertex-colored triangles in screen space (each three
        // vertices one triangle), alpha-blended.
        void DrawTrianglesScreen(const std::vector<SDL_Vertex>& vertices);

        // Additive ring in world space (shockwaves). Squash < 1 flattens
        // it vertically.
        void DrawRing(
            float x, float y, float radius, float thickness, float squash,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        // Additive tapered streak in world space (tracers): full width
        // and alpha at (x0, y0), fading to nothing at (x1, y1).
        void DrawStreak(
            float x0, float y0, float x1, float y1, float width,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        // Screen-space edge darkening; tinted for damage pulses.
        void DrawVignette(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        // --- Dynamic lighting ---
        //
        // Lights collected during a frame are drawn into a half-res light
        // map that is multiplied over everything drawn so far by
        // ApplyLighting. World-space DrawGlow calls add a light
        // automatically; AddLight adds one without a visible glow.
        void SetLightingEnabled(bool enabled);
        bool IsLightingEnabled() const;

        void AddLight(
            float x, float y, float radius,
            Uint8 r, Uint8 g, Uint8 b, float intensity = 1.0f);

        void ApplyLighting(const AmbientLight& ambient);

        // --- World pass ---
        //
        // Between BeginWorld and EndWorld everything lands in an offscreen
        // target holding the view at one texel per world pixel. EndWorld
        // blooms it and scales it onto the screen by the zoom with
        // pixel-art filtering, shifted by the camera's sub-pixel remainder
        // so scrolling glides instead of stepping a whole pixel at a time.
        // Screen-space draws inside the pass (the sky) are in target
        // coordinates, which span GetViewWidth x GetViewHeight.
        void SetZoom(float zoom);
        float GetZoom() const;

        void BeginWorld();
        void EndWorld();

        // World pixels visible across and down the screen.
        float GetViewWidth() const;
        float GetViewHeight() const;

        // Between world positions and screen (HUD) positions, through
        // the camera and the zoom.
        float WorldToScreenX(float x) const;
        float WorldToScreenY(float y) const;
        float ScreenToWorldX(float x) const;
        float ScreenToWorldY(float y) const;

        // Glow around bright pixels; 0 turns it off. Needs a renderer
        // with custom blend modes (every GPU backend; not the software
        // one).
        void SetBloom(float strength);
        bool IsBloomSupported() const;

        // A brief full-screen color wash on top of the world (hits,
        // explosions), faded by the caller each frame.
        void SetScreenTint(Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        SDL_Renderer* GetRenderer() const;

        // Real screen pixels per view pixel (window size / 1280x720,
        // letterboxed), for drawing text and UI crisply.
        float GetPixelScale() const;

        int GetWidth() const;
        int GetHeight() const;

    private:
        struct Light
        {
            float X, Y, Radius;
            Uint8 R, G, B;
            float Intensity;
        };

        void CreateEffectTextures();
        void EnsureWorldTargets();
        void ApplyBloom();

        // The camera position world draws subtract: in the world pass the
        // whole-pixel part (the sub-pixel rest is applied when the target
        // is scaled onto the screen).
        float OriginX() const;
        float OriginY() const;

        // Size of whatever is being drawn into right now.
        float SurfaceWidth() const;
        float SurfaceHeight() const;
        void DrawGlowTexture(float x, float y, float radius,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);

        SDL_Window* m_Window;
        SDL_Renderer* m_Renderer;

        const Camera* m_Camera;

        int m_Width = 0;
        int m_Height = 0;

        SDL_Texture* m_GlowTexture = nullptr;
        SDL_Texture* m_VignetteTexture = nullptr;
        SDL_Texture* m_LightMap = nullptr;

        SDL_Texture* m_WorldTarget = nullptr;
        int m_TargetWidth = 0;
        int m_TargetHeight = 0;
        float m_Zoom = 1.0f;
        bool m_InWorld = false;
        float m_SubX = 0.0f;
        float m_SubY = 0.0f;

        static constexpr int BloomLevels = 4;
        SDL_Texture* m_Bloom[BloomLevels] = {};
        float m_BloomStrength = 1.0f;
        bool m_BloomSupported = false;
        SDL_BlendMode m_SubtractBlend = SDL_BLENDMODE_INVALID;

        SDL_Color m_Tint = { 0, 0, 0, 0 };
        float m_PixelScale = 1.0f;

        bool m_LightingEnabled = true;
        std::vector<Light> m_Lights;
    };
}