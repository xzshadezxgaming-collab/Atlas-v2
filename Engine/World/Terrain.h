#pragma once

#include "Material.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace Atlas
{
    class Window;

    // Per-pixel destructible terrain, Cortex Command style.
    //
    // The world is two parallel buffers: a material layer (one Material id
    // per pixel, used for all collision and gameplay queries) and a color
    // layer (RGBA pixels, streamed to a GPU texture for rendering). Editing
    // the terrain writes both layers and marks a dirty region that is
    // uploaded to the texture once per frame in Update().
    class Terrain
    {
    public:
        Terrain();
        ~Terrain();

        bool Create(
            SDL_Renderer* renderer,
            int width,
            int height,
            unsigned int seed = 1337u);

        void Destroy();

        int GetWidth() const;
        int GetHeight() const;

        Material GetMaterial(int x, int y) const;
        void SetMaterial(int x, int y, Material material);

        // World-space solidity query. Outside the world: the sides and the
        // bottom are treated as solid walls, the sky above is open.
        bool IsSolid(float worldX, float worldY) const;

        // Removes solid pixels inside the circle. Returns how many pixels
        // were removed.
        int CarveCircle(float centerX, float centerY, float radius);

        // Fills the circle with the given material (only over air).
        // Returns how many pixels were placed.
        int PlaceCircle(float centerX, float centerY, float radius, Material material);

        // Uploads any edited region to the GPU texture. Call once per frame
        // before drawing.
        void Update();

        void Draw(Window& window);

    private:
        void Generate(unsigned int seed);
        void RebuildAllPixels();
        void WritePixelColor(int x, int y, Material material);
        void MarkDirty(int x, int y);
        void ResetDirtyRegion();

        int m_Width;
        int m_Height;

        std::vector<std::uint8_t> m_Materials;
        std::vector<std::uint8_t> m_Pixels;

        SDL_Texture* m_Texture;

        bool m_DirtyAll;
        int m_DirtyMinX;
        int m_DirtyMinY;
        int m_DirtyMaxX;
        int m_DirtyMaxY;
    };
}
