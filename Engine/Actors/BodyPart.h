#pragma once

#include <vector>

namespace Atlas
{
    // A small destructible pixel grid for a single limb - the same
    // erosion-style granularity as the main Terrain, scaled down to fit
    // one body part. Hits are addressed in local01 space (0..1 across the
    // part's current world-space bounding box) so the grid doesn't need to
    // track the limb's actual moving/rotating position itself.
    class BodyPart
    {
    public:
        explicit BodyPart(int gridWidth = 6, int gridHeight = 10);

        void Reset();

        bool IsAlive() const;
        float GetHealthFraction() const;

        int GetGridWidth() const;
        int GetGridHeight() const;
        bool IsCellAlive(int x, int y) const;

        // Removes pixels in a disc around the local01-mapped cell. Returns
        // true if this call destroyed the part's last remaining pixels.
        bool DamageAtLocal(float local01X, float local01Y, float radiusCells);

    private:
        int m_Width;
        int m_Height;
        std::vector<bool> m_Alive;
        int m_AliveCount;
    };
}
