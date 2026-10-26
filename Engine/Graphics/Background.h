#pragma once

#include "../World/Biome.h"

namespace Atlas
{
    class Window;

    // Procedural parallax backdrop: smooth dusk sky gradient, galaxy
    // band, stars, a shaded moon, soft clouds, five gradient ridge
    // silhouettes with haze between them and flora on the nearest.
    // Each biome has its own palette and touches: desert mesas and
    // cacti, an arctic aurora and snowy pines, a volcanic sky of smoke
    // and drifting embers under a blood moon.
    class Background
    {
    public:
        Background();

        void Create(unsigned int seed, Biome biome = Biome::Temperate);

        void Draw(
            Window& window,
            float cameraX,
            float cameraY,
            int viewWidth,
            int viewHeight,
            float timeSeconds) const;

    private:
        float LayerHeight(int layer, float worldX) const;

        unsigned int m_Seed;
        Biome m_Biome;
        float m_Phase[5][3];
    };
}
