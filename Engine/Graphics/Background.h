#pragma once

namespace Atlas
{
    class Window;

    // Procedural parallax backdrop: dusk sky gradient, stars, moon,
    // drifting clouds, and three layered mountain silhouettes that
    // scroll at different speeds behind the terrain.
    class Background
    {
    public:
        Background();

        void Create(unsigned int seed);

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
        float m_Phase[3][3];
    };
}
