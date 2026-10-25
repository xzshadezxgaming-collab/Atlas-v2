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

        constexpr float Pi = 3.14159265f;

        // Boot roll through a step: toe-off at lift-off, toe-up just
        // before the heel strikes (radians, screen space).
        constexpr float StepRoll = 0.55f;

        // Toes droop while the leg hangs in the air.
        constexpr float DangleDroop = 0.4f;

        constexpr Uint8 OutlineR = 24;
        constexpr Uint8 OutlineG = 22;
        constexpr Uint8 OutlineB = 27;

        Uint8 Scale(int value, int tint)
        {
            return static_cast<Uint8>(std::clamp(value, 0, 255) * tint / 255);
        }

        Uint8 Lighten(Uint8 value, int amount)
        {
            return static_cast<Uint8>(std::clamp(value + amount, 0, 255));
        }

        // A run of square stamps from (x0, y0) to (x1, y1) whose size
        // tapers from w0 to w1: a limb segment at pixel-art scale.
        void DrawTaper(
            Window& window,
            float x0,
            float y0,
            float x1,
            float y1,
            float w0,
            float w1,
            Uint8 r,
            Uint8 g,
            Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(length / 1.0f)));

            for (int i = 0; i <= steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float w = std::max(1.0f, w0 + (w1 - w0) * t);

                window.DrawFilledRect(
                    std::round(x0 + dx * t - w * 0.5f),
                    std::round(y0 + dy * t - w * 0.5f),
                    std::round(w),
                    std::round(w),
                    r,
                    g,
                    b,
                    255);
            }
        }

        // Outline, fill, and a highlight strip down the lit (upper,
        // forward) edge, offset along the segment's normal.
        void DrawLimbSegment(
            Window& window,
            float x0,
            float y0,
            float x1,
            float y1,
            float w0,
            float w1,
            float bend,
            Uint8 r,
            Uint8 g,
            Uint8 b)
        {
            DrawTaper(window, x0, y0, x1, y1, w0 + 2.0f, w1 + 2.0f,
                OutlineR, OutlineG, OutlineB);
            DrawTaper(window, x0, y0, x1, y1, w0, w1, r, g, b);

            // Shadow on the trailing side, light on the leading side.
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::max(0.001f, std::sqrt(dx * dx + dy * dy));

            float nx = dy / length;
            float ny = -dx / length;

            // Normal toward the knee's facing side.
            if (nx * bend < 0.0f)
            {
                nx = -nx;
                ny = -ny;
            }

            const float shadeW0 = std::max(1.0f, w0 * 0.34f);
            const float shadeW1 = std::max(1.0f, w1 * 0.34f);
            const float off0 = (w0 - shadeW0) * 0.5f;
            const float off1 = (w1 - shadeW1) * 0.5f;

            DrawTaper(window,
                x0 - nx * off0, y0 - ny * off0,
                x1 - nx * off1, y1 - ny * off1,
                shadeW0, shadeW1,
                static_cast<Uint8>(r * 3 / 4),
                static_cast<Uint8>(g * 3 / 4),
                static_cast<Uint8>(b * 3 / 4));

            DrawTaper(window,
                x0 + nx * off0, y0 + ny * off0,
                x1 + nx * off1, y1 + ny * off1,
                shadeW0, shadeW1,
                Lighten(r, 24), Lighten(g, 24), Lighten(b, 26));
        }

        // A point in the boot's frame (x toward the toe, y down) rotated
        // into the world around the ankle.
        void BootPoint(
            float ankleX,
            float ankleY,
            float dir,
            float cosA,
            float sinA,
            float scale,
            float localX,
            float localY,
            float& outX,
            float& outY)
        {
            const float x = localX * dir * scale;
            const float y = localY * scale;

            outX = ankleX + x * cosA - y * sinA;
            outY = ankleY + x * sinA + y * cosA;
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
        m_Swinging(false),
        m_SwingFromX(0.0f),
        m_SwingFromY(0.0f),
        m_SwingT(0.0f),
        m_SwingDirX(1.0f),
        m_VisualFootX(0.0f),
        m_VisualFootY(0.0f),
        m_SnapOffsetX(0.0f),
        m_SnapOffsetY(0.0f),
        m_LastFootX(0.0f),
        m_LastFootY(0.0f),
        m_VisualPitch(0.0f),
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
        return m_Thigh + m_Shin + AnkleHeight;
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
        m_Swinging = false;
        m_SwingT = 0.0f;
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

    bool Limb::AdvanceSwing(
        float targetX,
        float targetY,
        float duration,
        float lift,
        float deltaTime)
    {
        if (!m_Swinging)
        {
            m_Swinging = true;
            m_SwingFromX = m_FootX;
            m_SwingFromY = m_FootY;
            m_SwingT = 0.0f;
        }

        m_SwingT = std::min(1.0f, m_SwingT + deltaTime / std::max(0.02f, duration));

        if (targetX != m_SwingFromX)
            m_SwingDirX = targetX > m_SwingFromX ? 1.0f : -1.0f;

        // Smoothstep along the ground: the foot accelerates off the
        // ground and decelerates into the next foothold.
        const float t = m_SwingT;
        const float ease = t * t * (3.0f - 2.0f * t);

        // Peak height is measured from the higher of the two footholds,
        // so a step up lifts the toe over the ledge instead of through it.
        const float baseY = m_SwingFromY + (targetY - m_SwingFromY) * ease;
        const float climb = std::max(0.0f, baseY - std::min(m_SwingFromY, targetY));

        // Front-loaded arc: the knee drives up early, the foot comes
        // down onto the foothold rather than skimming into it.
        const float arc = std::sin(Pi * std::pow(t, 0.8f));

        m_FootX = m_SwingFromX + (targetX - m_SwingFromX) * ease;
        m_FootY = baseY - (lift + climb) * arc;

        if (m_SwingT >= 1.0f)
        {
            m_FootX = targetX;
            m_FootY = targetY;
            return true;
        }

        return false;
    }

    float Limb::GetSwingProgress() const
    {
        return m_Swinging ? m_SwingT : 0.0f;
    }

    void Limb::DangleToward(float restX, float restY, float rate, float deltaTime)
    {
        m_Swinging = false;
        m_SwingT = 0.0f;

        const float blend = std::min(1.0f, rate * deltaTime);

        m_FootX += (restX - m_FootX) * blend;
        m_FootY += (restY - m_FootY) * blend;
    }

    void Limb::EaseVisual(float deltaTime)
    {
        // Boot roll: flat on the ground, rolling through a step, toes
        // pointed while hanging (the droop is added at draw time, since
        // which way is "toe" depends on facing).
        const float targetPitch = m_Swinging
            ? m_SwingDirX * StepRoll * std::cos(Pi * m_SwingT)
            : 0.0f;

        // The very first call has nothing to ease from yet - snap so the
        // leg doesn't slide in from the origin at spawn.
        if (!m_VisualInitialized)
        {
            m_LastFootX = m_FootX;
            m_LastFootY = m_FootY;
            m_VisualFootX = m_FootX;
            m_VisualFootY = m_FootY;
            m_VisualPitch = targetPitch;
            m_VisualInitialized = true;
            return;
        }

        // Ordinary motion (a swing, a dangle carried along with the body)
        // is drawn exactly; only a jump bigger than any step could make
        // in one tick counts as a snap, and is smoothed out instead.
        constexpr float SnapDistance = 5.0f;

        const float moveX = m_FootX - m_LastFootX;
        const float moveY = m_FootY - m_LastFootY;

        if (moveX * moveX + moveY * moveY > SnapDistance * SnapDistance)
        {
            m_SnapOffsetX -= moveX;
            m_SnapOffsetY -= moveY;
        }

        m_LastFootX = m_FootX;
        m_LastFootY = m_FootY;

        const float decay = std::min(1.0f, 22.0f * deltaTime);
        m_SnapOffsetX -= m_SnapOffsetX * decay;
        m_SnapOffsetY -= m_SnapOffsetY * decay;

        m_VisualFootX = m_FootX + m_SnapOffsetX;
        m_VisualFootY = m_FootY + m_SnapOffsetY;

        const float pitchBlend = std::min(1.0f, 18.0f * deltaTime);
        m_VisualPitch += (targetPitch - m_VisualPitch) * pitchBlend;
    }

    void Limb::SolveKnee(
        float hipX,
        float hipY,
        float ankleX,
        float ankleY,
        float thigh,
        float shin,
        float bend,
        float& outKneeX,
        float& outKneeY,
        float& outAnkleX,
        float& outAnkleY)
    {
        float dx = ankleX - hipX;
        float dy = ankleY - hipY;
        float distance = std::sqrt(dx * dx + dy * dy);

        const float maxDistance = thigh + shin - 0.25f;
        const float minDistance = std::fabs(thigh - shin) + 0.5f;

        if (distance < 0.001f)
        {
            dx = 0.0f;
            dy = 1.0f;
            distance = 1.0f;
        }

        const float ux = dx / distance;
        const float uy = dy / distance;

        distance = std::clamp(distance, minDistance, maxDistance);

        outAnkleX = hipX + ux * distance;
        outAnkleY = hipY + uy * distance;

        // Law of cosines: how far along the hip-ankle line the knee sits,
        // and how far off it.
        const float along =
            (thigh * thigh - shin * shin + distance * distance) /
            (2.0f * distance);

        const float height = std::sqrt(std::max(0.0f, thigh * thigh - along * along));

        // The knee goes off the hip-ankle line on the facing side: for a
        // leg hanging straight down it comes forward, for a leg reaching
        // ahead it rides up, for a foot trailing behind it drops forward
        // and down - it never folds backwards. Mirrored for facing left.
        const float side = bend < 0.0f ? -1.0f : 1.0f;
        const float px = uy * side;
        const float py = -ux * side;
        const float amount = height * std::min(1.0f, std::fabs(bend));

        outKneeX = hipX + ux * along + px * amount;
        outKneeY = hipY + uy * along + py * amount;
    }

    void Limb::Draw(
        Window& window,
        float hipX,
        float hipY,
        float bend,
        bool nearSide,
        const LegStyle& style,
        unsigned char tintR,
        unsigned char tintG,
        unsigned char tintB) const
    {
        // Drawn from the eased visual foot position, not the raw logical
        // one, so an instantaneous logical reposition (an emergency
        // recovery plant, a landing catch) reads as a quick slide rather
        // than a pop.
        const float facing = bend < 0.0f ? -1.0f : 1.0f;

        float pitch = m_VisualPitch;

        if (!m_Planted && !m_Swinging)
            pitch += DangleDroop * facing;

        const float cosA = std::cos(pitch);
        const float sinA = std::sin(pitch);

        // The ankle sits above the sole, rotated with the boot.
        float ankleX = 0.0f;
        float ankleY = 0.0f;

        {
            // Sole contact point in boot space is (0, AnkleHeight).
            const float sx = -(-AnkleHeight) * sinA;
            const float sy = -AnkleHeight * cosA;

            ankleX = m_VisualFootX + sx;
            ankleY = m_VisualFootY + sy;
        }

        float kneeX = 0.0f;
        float kneeY = 0.0f;

        SolveKnee(hipX, hipY, ankleX, ankleY, m_Thigh, m_Shin, bend,
            kneeX, kneeY, ankleX, ankleY);

        // A deeply folded leg with the foot trailing would put the knee
        // through the floor; kneel on it instead.
        kneeY = std::min(kneeY, std::max(hipY, m_VisualFootY - 2.0f));

        // Far leg is in shadow.
        const int shade = nearSide ? 0 : -26;

        const Uint8 pantsR = Scale(style.PantsR + shade, tintR);
        const Uint8 pantsG = Scale(style.PantsG + shade, tintG);
        const Uint8 pantsB = Scale(style.PantsB + shade, tintB);
        const Uint8 bootR = Scale(style.BootR + shade, tintR);
        const Uint8 bootG = Scale(style.BootG + shade, tintG);
        const Uint8 bootB = Scale(style.BootB + shade, tintB);
        const Uint8 padR = Scale(style.PadR + shade, tintR);
        const Uint8 padG = Scale(style.PadG + shade, tintG);
        const Uint8 padB = Scale(style.PadB + shade, tintB);

        const float kneeWidth = (style.ThighWidth + style.ShinWidth) * 0.5f;

        // Shin first so the thigh overlaps it at the knee, like cloth
        // folding over the joint.
        DrawLimbSegment(window, kneeX, kneeY, ankleX, ankleY,
            style.ShinWidth, style.ShinWidth - 1.0f, bend,
            pantsR, pantsG, pantsB);

        if (style.ShinPlate)
        {
            // Greave down the front of the shin.
            const float sx = ankleX - kneeX;
            const float sy = ankleY - kneeY;
            const float length = std::max(0.001f, std::sqrt(sx * sx + sy * sy));
            float nx = sy / length;
            float ny = -sx / length;

            if (nx * bend < 0.0f)
            {
                nx = -nx;
                ny = -ny;
            }

            const float off = style.ShinWidth * 0.3f;

            DrawTaper(window,
                kneeX + sx * 0.2f + nx * off, kneeY + sy * 0.2f + ny * off,
                kneeX + sx * 0.8f + nx * off, kneeY + sy * 0.8f + ny * off,
                2.0f, 2.0f, padR, padG, padB);
        }

        DrawLimbSegment(window, hipX, hipY, kneeX, kneeY,
            style.ThighWidth, kneeWidth, bend,
            pantsR, pantsG, pantsB);

        // Knee: a pad strapped on the front of the joint, or a plate on
        // armoured legs. Bare knees just show the fold of the cloth.
        if (style.KneeSize > 0.0f)
        {
            const float size = style.KneeSize;
            const float cx = kneeX + facing * std::max(0.0f, kneeWidth * 0.5f - size * 0.35f);
            const float cy = kneeY;

            window.DrawFilledRect(
                std::round(cx - size * 0.5f - 1.0f),
                std::round(cy - size * 0.5f - 1.0f),
                std::round(size + 2.0f),
                std::round(size + 2.0f),
                OutlineR, OutlineG, OutlineB, 255);
            window.DrawFilledRect(
                std::round(cx - size * 0.5f),
                std::round(cy - size * 0.5f),
                std::round(size),
                std::round(size),
                padR, padG, padB, 255);
            window.DrawFilledRect(
                std::round(cx - size * 0.5f),
                std::round(cy - size * 0.5f),
                std::round(size),
                1.0f,
                Lighten(padR, 34), Lighten(padG, 34), Lighten(padB, 34), 255);
        }

        // Boot, built in its own frame (x toward the toe, y down from the
        // ankle) and rolled with the step.
        {
            const float s = style.BootScale;
            const float dir = facing;

            float hx, hy, tx, ty;

            // Upper: heel to toe.
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, -1.0f, 1.0f, hx, hy);
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, 4.0f, 1.5f, tx, ty);

            DrawTaper(window, hx, hy, tx, ty, 6.0f * s, 5.0f * s,
                OutlineR, OutlineG, OutlineB);
            DrawTaper(window, hx, hy, tx, ty, 4.0f * s, 3.0f * s,
                bootR, bootG, bootB);

            // Shaft up to the ankle.
            float ax, ay;
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, -0.5f, -0.5f, ax, ay);
            DrawTaper(window, ax, ay, hx, hy, 3.0f * s, 4.0f * s,
                bootR, bootG, bootB);

            // Lace highlight along the top.
            float lx, ly;
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, 1.0f, 0.0f, lx, ly);
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, 3.5f, 0.6f, tx, ty);
            DrawTaper(window, lx, ly, tx, ty, 1.0f, 1.0f,
                Lighten(bootR, 30), Lighten(bootG, 28), Lighten(bootB, 26));

            // Sole.
            float s0x, s0y, s1x, s1y;
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, -2.0f, AnkleHeight / s - 0.5f, s0x, s0y);
            BootPoint(ankleX, ankleY, dir, cosA, sinA, s, 5.5f, AnkleHeight / s - 0.5f, s1x, s1y);
            DrawTaper(window, s0x, s0y, s1x, s1y, 2.0f, 2.0f,
                Scale(34, tintR), Scale(30, tintG), Scale(30, tintB));
        }
    }
}
