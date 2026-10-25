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

        // Builds the simulation side of the terrain (material and color
        // layers). Needs no renderer, so a headless server or test can
        // own a Terrain too.
        bool Create(
            int width,
            int height,
            unsigned int seed = 1337u);

        // Creates the GPU texture the color layer streams into. Only a
        // client that draws the terrain needs to call this.
        bool CreateTexture(SDL_Renderer* renderer);

        void Destroy();

        int GetWidth() const;
        int GetHeight() const;

        Material GetMaterial(int x, int y) const;

        // Raw material layer, row-major, one Material id per pixel
        // (state hashing, serialization).
        const std::uint8_t* GetMaterialData() const;

        // Color layer, row-major RGBA (minimap, screenshots).
        const std::uint8_t* GetPixelData() const;
        void SetMaterial(int x, int y, Material material);

        // What dug-out space at (x, y) shows behind it (Air = sky).
        void SetBackwall(int x, int y, Material material);

        // First solid row from the top in column x (height if none).
        int GetSurfaceY(int x) const;

        // World-space solidity query. Outside the world: the sides and the
        // bottom are treated as solid walls, the sky above is open.
        bool IsSolid(float worldX, float worldY) const;

        // Marches from (startX, startY) along the (normalized) direction
        // one pixel at a time and reports the first solid pixel within
        // maxDistance. Returns false if the ray stays in open air.
        bool RaycastSolid(
            float startX,
            float startY,
            float dirX,
            float dirY,
            float maxDistance,
            float& hitX,
            float& hitY) const;

        // Removes solid pixels inside the circle. Returns how many pixels
        // were removed.
        int CarveCircle(float centerX, float centerY, float radius);

        // A destroyed pixel that a carve reported back, so callers can
        // spawn matching debris.
        struct DestroyedPixel
        {
            float X;
            float Y;
            Material Mat;
        };

        // Inclusive pixel rectangle.
        struct Box
        {
            int MinX;
            int MinY;
            int MaxX;
            int MaxY;
        };

        // Like CarveCircle, but samples up to maxSamples of the destroyed
        // pixels into outDebris.
        int CarveCircleCollect(
            float centerX,
            float centerY,
            float radius,
            std::vector<DestroyedPixel>& outDebris,
            int maxSamples,
            const Box* spareBrain = nullptr); // brain pixels in here survive

        // Removes a single solid pixel and returns its material
        // (Material::Air if there was nothing to remove).
        Material DestroyPixel(int x, int y);

        // Darkens/tints the color of a solid pixel toward the given color
        // without changing its material (blood stains, scorch marks).
        void StainPixel(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b);

        // Fills the circle with the given material (only over air).
        // Returns how many pixels were placed.
        int PlaceCircle(float centerX, float centerY, float radius, Material material);

        // Advances the falling-sand simulation one tick: powders (sand,
        // gravel) fall and pile, liquids (water, lava) flow and level
        // out, and lava meeting water hardens into obsidian. Only regions
        // where something changed recently are processed, so a settled
        // world costs almost nothing. Fully deterministic.
        void Simulate();

        // Spots where lava met water since the last call (for steam
        // effects). Cleared by the call.
        std::vector<DestroyedPixel> TakeReactions();

        // Number of 32x32 regions the simulation will visit next tick
        // (0 = everything has settled).
        int GetAwakeRegionCount() const;

        // Uploads any edited region to the GPU texture. Call once per frame
        // before drawing. Does nothing without a texture.
        void Update();

        void Draw(Window& window);

    private:
        static constexpr int ChunkSize = 32;

        // How far below a ceiling its contact shadow reaches.
        static constexpr int ShadowReach = 5;

        void Generate(unsigned int seed);
        void Wake(int x, int y);
        void UpdateCell(int x, int y);
        bool CanDisplace(int x, int y, Material mover) const;
        void MoveCell(int fromX, int fromY, int toX, int toY);
        void RebuildAllPixels();
        void QueueRecolor(int x, int y);
        void WritePixelColor(int x, int y, Material material);
        void MarkDirty(int x, int y);
        void ResetDirtyRegion();

        int m_Width;
        int m_Height;

        std::vector<std::uint8_t> m_Materials;
        std::vector<std::uint8_t> m_Pixels;

        // What each pixel was at generation (Air above the surface): the
        // rock wall seen behind tunnels and caves once it's dug out.
        std::vector<std::uint8_t> m_Backwall;

        // Per-pixel brightness factor (255 = full) baked at generation:
        // terrain gets darker with depth below the original surface.
        std::vector<std::uint8_t> m_DepthShade;

        SDL_Texture* m_Texture;

        // Falling-sand bookkeeping: which chunks to visit this tick and
        // next, and a per-pixel stamp so nothing moves twice in a tick.
        int m_ChunksX;
        int m_ChunksY;
        std::vector<std::uint8_t> m_ChunkAwake;
        std::vector<std::uint8_t> m_ChunkAwakeNext;
        std::vector<std::uint8_t> m_MoveStamp;
        std::uint8_t m_Stamp;
        std::uint32_t m_SimTick;
        std::vector<DestroyedPixel> m_Reactions;

        bool m_DirtyAll;

        // Pixels waiting to be reshaded (see SetMaterial), deduplicated.
        std::vector<std::uint32_t> m_RecolorQueue;
        std::vector<std::uint8_t> m_RecolorQueued;
        bool m_PixelsStale = false;
        int m_DirtyMinX;
        int m_DirtyMinY;
        int m_DirtyMaxX;
        int m_DirtyMaxY;
    };
}
