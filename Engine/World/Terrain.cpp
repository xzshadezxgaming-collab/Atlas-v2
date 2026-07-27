#include "Terrain.h"

namespace Atlas
{
    Terrain::Terrain()
    {
        for (int y = 0; y < Height; y++)
        {
            for (int x = 0; x < Width; x++)
            {
                // Simple flat world.
                m_Data[y][x] = (y >= Height / 2);
            }
        }
    }

    void Terrain::Draw(Window& window)
    {
        for (int y = 0; y < Height; y++)
        {
            for (int x = 0; x < Width; x++)
            {
                if (!m_Data[y][x])
                    continue;

                window.DrawFilledRect(
                    x * TileSize,
                    y * TileSize,
                    TileSize,
                    TileSize,
                    95,
                    70,
                    35,
                    255);
            }
        }
    }

    bool Terrain::IsSolid(float worldX, float worldY) const
    {
        const int tileX = static_cast<int>(worldX / TileSize);
        const int tileY = static_cast<int>(worldY / TileSize);

        if (tileX < 0 || tileX >= Width)
            return false;

        if (tileY < 0 || tileY >= Height)
            return false;

        return m_Data[tileY][tileX];
    }
}