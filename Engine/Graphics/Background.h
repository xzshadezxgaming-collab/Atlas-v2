#pragma once

namespace Atlas
{
    class Window;

    // Procedural parallax backdrop: smooth dusk sky gradient, galaxy
    // band, stars, a shaded moon, soft clouds, five gradient ridge
    // silhouettes with haze between them and pines on the nearest.
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
        float m_Phase[5][3];
    };
}
