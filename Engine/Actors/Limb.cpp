#include "Limb.h"

#include "../Core/Window.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        // The foot hitbox: a small pad under the foot position.
        constexpr float FootHalfWidth = 4.0f;
        constexpr float FootDepth = 3.0f;

        constexpr float LimbThickness = 5.0f;

        void DrawSegment(
            Window& window,
            float x0,
            float y0,
            float x1,
            float y1,
            Uint8 r,
            Uint8 g,
            Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(length / (LimbThickness * 0.5f))));

            for (int i = 0; i <= steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    x0 + dx * t - LimbThickness * 0.5f,
                    y0 + dy * t - LimbThickness * 0.5f,
                    LimbThickness,
                    LimbThickness,
                    r,
                    g,
                    b,
                    255);
            }
        }
    }

    Limb::Limb()
        : m_HipOffsetX(0.0f),
        m_HipOffsetY(0.0f),
        m_Thigh(14.0f),
        m_Shin(14.0f),
        m_FootX(0.0f),
        m_FootY(0.0f),
        m_Planted(false)
    {
    }

    void Limb::Configure(
        float hipOffsetX,
        float hipOffsetY,
        float thighLength,
        float shinLength)
    {
        m_HipOffsetX = hipOffsetX;
        m_HipOffsetY = hipOffsetY;
        m_Thigh = thighLength;
        m_Shin = shinLength;
    }

    float Limb::GetHipOffsetX() const
    {
        return m_HipOffsetX;
    }

    float Limb::GetHipOffsetY() const
    {
        return m_HipOffsetY;
    }

    float Limb::GetReach() const
    {
        return m_Thigh + m_Shin;
    }

    bool Limb::IsPlanted() const
    {
        return m_Planted;
    }

    void Limb::Plant(float footX, float footY)
    {
        m_FootX = footX;
        m_FootY = footY;
        m_Planted = true;
    }

    void Limb::Unplant()
    {
        m_Planted = false;
    }

    float Limb::GetFootX() const
    {
        return m_FootX;
    }

    float Limb::GetFootY() const
    {
        return m_FootY;
    }

    void Limb::SetFootPosition(float x, float y)
    {
        m_FootX = x;
        m_FootY = y;
    }

    bool Limb::IsSupported(const Terrain& terrain) const
    {
        if (!m_Planted)
            return false;

        for (float x = m_FootX - FootHalfWidth;
            x <= m_FootX + FootHalfWidth;
            x += 2.0f)
        {
            for (float y = m_FootY; y <= m_FootY + FootDepth; y += 1.0f)
            {
                if (terrain.IsSolid(x, y))
                    return true;
            }
        }

        return false;
    }

    bool Limb::SwingToward(
        float targetX,
        float targetY,
        float speed,
        float lift,
        float deltaTime)
    {
        const float dx = targetX - m_FootX;
        const float dy = targetY - m_FootY;
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 3.0f)
        {
            m_FootX = targetX;
            m_FootY = targetY;
            return true;
        }

        // Aim above the foothold while far away so the foot arcs over
        // small obstacles instead of dragging through them.
        const float liftAmount = lift * std::min(1.0f, distance / 12.0f);

        float aimX = targetX;
        float aimY = targetY - liftAmount;

        const float ax = aimX - m_FootX;
        const float ay = aimY - m_FootY;
        const float aimDistance = std::sqrt(ax * ax + ay * ay);

        const float step = std::min(speed * deltaTime, aimDistance);

        if (aimDistance > 0.0001f)
        {
            m_FootX += ax / aimDistance * step;
            m_FootY += ay / aimDistance * step;
        }

        return false;
    }

    void Limb::DangleToward(float restX, float restY, float deltaTime)
    {
        const float blend = std::min(1.0f, 8.0f * deltaTime);

        m_FootX += (restX - m_FootX) * blend;
        m_FootY += (restY - m_FootY) * blend;
    }

    void Limb::Draw(
        Window& window,
        float hipX,
        float hipY,
        float facingDir,
        bool nearSide,
        unsigned char tintR,
        unsigned char tintG,
        unsigned char tintB) const
    {
        // Two-bone IK: place the knee on the circle intersection of thigh
        // and shin, bent toward the facing direction.
        float dx = m_FootX - hipX;
        float dy = m_FootY - hipY;
        float distance = std::sqrt(dx * dx + dy * dy);

        const float maxDistance = m_Thigh + m_Shin - 0.5f;

        float footX = m_FootX;
        float footY = m_FootY;

        if (distance > maxDistance)
        {
            footX = hipX + dx / distance * maxDistance;
            footY = hipY + dy / distance * maxDistance;
            dx = footX - hipX;
            dy = footY - hipY;
            distance = maxDistance;
        }

        if (distance < 0.001f)
        {
            distance = 0.001f;
            dy = distance;
        }

        const float ux = dx / distance;
        const float uy = dy / distance;

        const float along =
            (m_Thigh * m_Thigh - m_Shin * m_Shin + distance * distance) /
            (2.0f * distance);

        const float heightSquared = m_Thigh * m_Thigh - along * along;
        const float height = std::sqrt(std::max(0.0f, heightSquared));

        const float kneeX = hipX + ux * along + uy * height * facingDir;
        const float kneeY = hipY + uy * along - ux * height * facingDir;

        Uint8 pantsR = 86, pantsG = 90, pantsB = 104;
        Uint8 bootR = 56, bootG = 46, bootB = 38;

        if (!nearSide)
        {
            pantsR = 60; pantsG = 63; pantsB = 74;
            bootR = 42; bootG = 35; bootB = 29;
        }

        pantsR = static_cast<Uint8>(pantsR * tintR / 255);
        pantsG = static_cast<Uint8>(pantsG * tintG / 255);
        pantsB = static_cast<Uint8>(pantsB * tintB / 255);
        bootR = static_cast<Uint8>(bootR * tintR / 255);
        bootG = static_cast<Uint8>(bootG * tintG / 255);
        bootB = static_cast<Uint8>(bootB * tintB / 255);

        DrawSegment(window, hipX, hipY, kneeX, kneeY, pantsR, pantsG, pantsB);
        DrawSegment(window, kneeX, kneeY, footX, footY, pantsR, pantsG, pantsB);

        // Boot, toe pointing the way the actor faces.
        const float bootWidth = 10.0f;
        const float bootHeight = 4.0f;

        const float bootX = facingDir > 0.0f
            ? footX - 4.0f
            : footX - bootWidth + 4.0f;

        window.DrawFilledRect(
            bootX,
            footY - 2.0f,
            bootWidth,
            bootHeight,
            bootR,
            bootG,
            bootB,
            255);
    }
}
