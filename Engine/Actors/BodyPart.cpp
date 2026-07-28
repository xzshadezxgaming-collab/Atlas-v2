#include "BodyPart.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    BodyPart::BodyPart(int gridWidth, int gridHeight)
        : m_Width(std::max(1, gridWidth)),
        m_Height(std::max(1, gridHeight)),
        m_Alive(),
        m_AliveCount(0)
    {
        Reset();
    }

    void BodyPart::Reset()
    {
        m_Alive.assign(static_cast<size_t>(m_Width) * m_Height, true);
        m_AliveCount = m_Width * m_Height;
    }

    bool BodyPart::IsAlive() const
    {
        return m_AliveCount > 0;
    }

    float BodyPart::GetHealthFraction() const
    {
        const int total = m_Width * m_Height;
        if (total <= 0)
            return 0.0f;
        return static_cast<float>(m_AliveCount) / static_cast<float>(total);
    }

    int BodyPart::GetGridWidth() const
    {
        return m_Width;
    }

    int BodyPart::GetGridHeight() const
    {
        return m_Height;
    }

    bool BodyPart::IsCellAlive(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= m_Width || y >= m_Height)
            return false;
        return m_Alive[static_cast<size_t>(y) * m_Width + x];
    }

    bool BodyPart::DamageAtLocal(float local01X, float local01Y, float radiusCells)
    {
        if (!IsAlive())
            return false;

        const float cellX = std::clamp(local01X, 0.0f, 1.0f) * (m_Width - 1);
        const float cellY = std::clamp(local01Y, 0.0f, 1.0f) * (m_Height - 1);

        const int span = static_cast<int>(radiusCells) + 1;
        const float radiusSquared = radiusCells * radiusCells;

        for (int dy = -span; dy <= span; dy++)
        {
            for (int dx = -span; dx <= span; dx++)
            {
                const float distSquared = static_cast<float>(dx * dx + dy * dy);
                if (distSquared > radiusSquared)
                    continue;

                const int x = static_cast<int>(cellX) + dx;
                const int y = static_cast<int>(cellY) + dy;
                if (x < 0 || y < 0 || x >= m_Width || y >= m_Height)
                    continue;

                const size_t index = static_cast<size_t>(y) * m_Width + x;
                if (m_Alive[index])
                {
                    m_Alive[index] = false;
                    m_AliveCount--;
                }
            }
        }

        return m_AliveCount <= 0;
    }
}
