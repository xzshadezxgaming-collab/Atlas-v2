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

        void DrawSegment(
            Window& window,
            float x0,
            float y0,
            float x1,
            float y1,
            float thickness,
            Uint8 r,
            Uint8 g,
            Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(length / (thickness * 0.5f))));

            for (int i = 0; i <= steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    x0 + dx * t - thickness * 0.5f,
                    y0 + dy * t - thickness * 0.5f,
                    thickness,
                    thickness,
                    r,
                    g,
                    b,
                    255);
            }
        }

        // A limb segment with a dark outline and a subtle top highlight.
        void DrawShadedSegment(
            Window& window,
            float x0,
            float y0,
            float x1,
            float y1,
            float thickness,
            Uint8 r,
            Uint8 g,
            Uint8 b)
        {
            DrawSegment(window, x0, y0, x1, y1, thickness + 2.0f, 24, 22, 27);
            DrawSegment(window, x0, y0, x1, y1, thickness, r, g, b);

            // Light catches the upper-left edge.
            DrawSegment(
                window,
                x0 - 1.0f, y0 - 1.0f,
                x1 - 1.0f, y1 - 1.0f,
                std::max(1.0f, thickness - 3.0f),
                static_cast<Uint8>(std::min(255, r + 26)),
                static_cast<Uint8>(std::min(255, g + 26)),
                static_cast<Uint8>(std::min(255, b + 28)));
        }
    }

    Limb::Limb()
        : m_HipOffsetX(0.0f),
        m_HipOffsetY(0.0f),
        m_Thigh(14.0f),
        m_Shin(14.0f),
        m_FootX(0.0f),
        m_FootY(0.0f),
        m_Planted(false),
        m_VisualFootX(0.0f),
        m_VisualFootY(0.0f),
        m_VisualInitialized(false)
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

    void Limb::EaseVisual(float deltaTime)
    {
        // The very first call has nothing to ease from yet - snap so the
        // leg doesn't slide in from the origin at spawn.
        if (!m_VisualInitialized)
        {
            m_VisualFootX = m_FootX;
            m_VisualFootY = m_FootY;
            m_VisualInitialized = true;
            return;
        }

        const float blend = std::min(1.0f, 26.0f * deltaTime);

        m_VisualFootX += (m_FootX - m_VisualFootX) * blend;
        m_VisualFootY += (m_FootY - m_VisualFootY) * blend;
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
        // and shin, bent toward the facing direction. Drawn from the
        // eased visual foot position, not the raw logical one, so an
        // instantaneous logical reposition (an emergency recovery plant,
        // a landing catch) reads as a quick slide rather than a pop.
        float dx = m_VisualFootX - hipX;
        float dy = m_VisualFootY - hipY;
        float distance = std::sqrt(dx * dx + dy * dy);

        const float maxDistance = m_Thigh + m_Shin - 0.5f;

        float footX = m_VisualFootX;
        float footY = m_VisualFootY;

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

        Uint8 pantsR = 88, pantsG = 92, pantsB = 106;
        Uint8 bootR = 62, bootG = 50, bootB = 40;

        if (!nearSide)
        {
            pantsR = 58; pantsG = 61, pantsB = 72;
            bootR = 44, bootG = 36, bootB = 30;
        }

        pantsR = static_cast<Uint8>(pantsR * tintR / 255);
        pantsG = static_cast<Uint8>(pantsG * tintG / 255);
        pantsB = static_cast<Uint8>(pantsB * tintB / 255);
        bootR = static_cast<Uint8>(bootR * tintR / 255);
        bootG = static_cast<Uint8>(bootG * tintG / 255);
        bootB = static_cast<Uint8>(bootB * tintB / 255);

        // Thigh is beefier than the shin.
        DrawShadedSegment(
            window, hipX, hipY, kneeX, kneeY, 6.0f,
            pantsR, pantsG, pantsB);
        DrawShadedSegment(
            window, kneeX, kneeY, footX, footY - 2.0f, 4.0f,
            pantsR, pantsG, pantsB);

        // Knee pad.
        {
            const Uint8 padR = static_cast<Uint8>(70 * tintR / 255);
            const Uint8 padG = static_cast<Uint8>(78 * tintG / 255);
            const Uint8 padB = static_cast<Uint8>(58 * tintB / 255);

            window.DrawFilledRect(kneeX - 3.0f, kneeY - 3.0f, 6.0f, 6.0f,
                24, 22, 27, 255);
            window.DrawFilledRect(kneeX - 2.0f, kneeY - 2.0f, 4.0f, 4.0f,
                padR, padG, padB, 255);
            window.DrawFilledRect(kneeX - 2.0f, kneeY - 2.0f, 4.0f, 1.0f,
                static_cast<Uint8>(std::min(255, padR + 30)),
                static_cast<Uint8>(std::min(255, padG + 30)),
                static_cast<Uint8>(std::min(255, padB + 30)),
                255);
        }

        // Boot: upper, forward toe cap, dark sole.
        {
            const float dir = facingDir;

            // Outline block.
            window.DrawFilledRect(
                footX - 6.0f + (dir > 0.0f ? 0.0f : -2.0f),
                footY - 5.0f,
                14.0f,
                7.0f,
                24, 22, 27, 255);

            // Upper.
            window.DrawFilledRect(
                footX - 5.0f + (dir > 0.0f ? 0.0f : -1.0f),
                footY - 4.0f,
                10.0f,
                4.0f,
                bootR, bootG, bootB, 255);

            // Toe cap.
            window.DrawFilledRect(
                dir > 0.0f ? footX + 4.0f : footX - 8.0f,
                footY - 2.0f,
                4.0f,
                2.0f,
                bootR, bootG, bootB, 255);

            // Top highlight.
            window.DrawFilledRect(
                footX - 5.0f + (dir > 0.0f ? 0.0f : -1.0f),
                footY - 4.0f,
                10.0f,
                1.0f,
                static_cast<Uint8>(std::min(255, bootR + 28)),
                static_cast<Uint8>(std::min(255, bootG + 28)),
                static_cast<Uint8>(std::min(255, bootB + 28)),
                255);

            // Sole.
            window.DrawFilledRect(
                footX - 5.0f + (dir > 0.0f ? 0.0f : -3.0f),
                footY,
                12.0f,
                2.0f,
                30, 27, 26, 255);
        }
    }
}
