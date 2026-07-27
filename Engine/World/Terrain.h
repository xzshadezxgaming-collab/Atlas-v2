#pragma once

#include "../Core/Window.h"

namespace Atlas
{
    class Terrain
    {
    public:
        Terrain();

        void Draw(Window& window);

        bool IsSolid(float worldX, float worldY) const;

    private:
        static constexpr int Width = 200;
        static constexpr int Height = 100;
        static constexpr float TileSize = 32.0f;

        bool m_Data[Height][Width];
    };
}