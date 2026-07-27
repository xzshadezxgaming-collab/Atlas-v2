#include "Actor.h"

#include "../Core/Window.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        // Torso hitbox (the legs hang below it).
        constexpr float BodyWidth = 24.0f;
        constexpr float BodyHeight = 40.0f;

        // The body sprite is a little wider than the hitbox.
        constexpr float SpriteWidth = 32.0f;
        constexpr float SpriteHeight = 40.0f;

        // Hip joints, relative to the body's top-left corner.
        constexpr float HipY = 38.0f;
        constexpr float HipXBack = 8.0f;
        constexpr float HipXFront = 16.0f;

        constexpr float ThighLength = 17.0f;
        constexpr float ShinLength = 16.0f;
        constexpr float LegReach = ThighLength + ShinLength;

        // Body-bottom height above the supporting feet when standing.
        constexpr float StandHeight = 24.0f;

        // How far a foot may reach for the next foothold.
        constexpr float MaxStepUp = 18.0f;
        constexpr float MaxStepDown = 36.0f;
        constexpr float StepAhead = 16.0f;
        constexpr float FootLift = 10.0f;

        // A foot may only be planted within this hip distance...
        constexpr float PlantReach = LegReach + 3.0f;

        // ...and loses grip beyond this one. The gap is hysteresis: a
        // fresh plant at full stretch doesn't immediately let go again.
        constexpr float MaxFootDistance = LegReach + 8.0f;

        // Stance stretch at which the swinging leg plants on the nearest
        // foothold instead of finishing its full step.
        constexpr float StanceStrain = LegReach + 1.0f;

        constexpr float WalkSpeed = 170.0f;
        constexpr float Gravity = 1000.0f;
        constexpr float JumpVelocity = -460.0f;

        // Vertical body speed limit while the legs push it up or let it
        // down (climbing is slower than free fall).
        constexpr float ClimbRate = 260.0f;

        // Clearance the torso may ride up while moving horizontally into
        // rising ground (the legs handle real steps).
        constexpr int BodyClearance = 12;

        constexpr float SampleSpacing = 2.0f;
    }

    Actor::Actor()
        : m_X(0.0f),
        m_Y(0.0f),
        m_VelocityX(0.0f),
        m_VelocityY(0.0f),
        m_SwingLeg(0),
        m_FacingDir(1.0f),
        m_Grounded(false)
    {
        m_Legs[0].Configure(HipXBack, HipY, ThighLength, ShinLength);
        m_Legs[1].Configure(HipXFront, HipY, ThighLength, ShinLength);
    }

    bool Actor::LoadBodySprite(
        SDL_Renderer* renderer,
        const std::string& filename)
    {
        return m_BodyTexture.Load(renderer, filename);
    }

    void Actor::Spawn(const Terrain& terrain, float centerX)
    {
        float surfaceY = 0.0f;

        for (int y = 0; y < terrain.GetHeight(); y++)
        {
            if (terrain.IsSolid(centerX, static_cast<float>(y)))
            {
                surfaceY = static_cast<float>(y);
                break;
            }
        }

        m_X = centerX - BodyWidth * 0.5f;
        m_Y = surfaceY - StandHeight - BodyHeight;
        m_VelocityX = 0.0f;
        m_VelocityY = 0.0f;

        for (int i = 0; i < 2; i++)
        {
            float footholdY = surfaceY;
            FindFoothold(terrain, HipWorldX(i), footholdY);
            m_Legs[i].Plant(HipWorldX(i), footholdY);
        }

        m_Grounded = true;
    }

    void Actor::Update(
        const Terrain& terrain,
        float deltaTime,
        float moveInput,
        bool jump)
    {
        m_VelocityX = moveInput * WalkSpeed;

        if (moveInput < 0.0f)
            m_FacingDir = -1.0f;
        else if (moveInput > 0.0f)
            m_FacingDir = 1.0f;

        m_VelocityY += Gravity * deltaTime;

        // A planted foot only counts while the terrain under it still
        // exists (digging can remove it) and it is still within leg reach.
        for (int i = 0; i < 2; i++)
        {
            if (!m_Legs[i].IsPlanted())
                continue;

            if (!m_Legs[i].IsSupported(terrain))
            {
                m_Legs[i].Unplant();
                continue;
            }

            const float dx = m_Legs[i].GetFootX() - HipWorldX(i);
            const float dy = m_Legs[i].GetFootY() - HipWorldY(i);

            if (std::sqrt(dx * dx + dy * dy) > MaxFootDistance)
                m_Legs[i].Unplant();
        }

        m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();

        if (m_Grounded && jump)
        {
            m_VelocityY = JumpVelocity;
            m_Legs[0].Unplant();
            m_Legs[1].Unplant();
            m_Grounded = false;
        }

        if (m_Grounded)
        {
            m_VelocityY = 0.0f;

            MoveHorizontal(terrain, m_VelocityX * deltaTime);

            if (std::fabs(m_VelocityX) > 1.0f)
                UpdateWalkGait(terrain, deltaTime);
            else
                UpdateIdleFeet(terrain, deltaTime);

            // The body rides at stand height above the supporting feet.
            float supportSum = 0.0f;
            int supportCount = 0;

            for (int i = 0; i < 2; i++)
            {
                if (m_Legs[i].IsPlanted())
                {
                    supportSum += m_Legs[i].GetFootY();
                    supportCount++;
                }
            }

            if (supportCount > 0)
            {
                const float supportY = supportSum / supportCount;

                MoveVerticalToward(
                    terrain,
                    supportY - StandHeight - BodyHeight,
                    deltaTime);
            }

            m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();
        }

        if (!m_Grounded)
        {
            MoveAirborne(terrain, deltaTime);
            UpdateDanglingFeet(deltaTime);

            if (m_VelocityY >= 0.0f)
                TryLand(terrain);

            m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();

            if (m_Grounded)
                m_VelocityY = 0.0f;
        }
    }

    void Actor::Draw(Window& window)
    {
        const int nearLeg = m_FacingDir > 0.0f ? 1 : 0;
        const int farLeg = 1 - nearLeg;

        m_Legs[farLeg].Draw(
            window,
            HipWorldX(farLeg),
            HipWorldY(farLeg),
            m_FacingDir,
            false);

        if (m_BodyTexture.GetTexture())
        {
            window.DrawTexture(
                m_BodyTexture.GetTexture(),
                m_X - (SpriteWidth - BodyWidth) * 0.5f,
                m_Y,
                SpriteWidth,
                SpriteHeight,
                m_FacingDir < 0.0f);
        }

        m_Legs[nearLeg].Draw(
            window,
            HipWorldX(nearLeg),
            HipWorldY(nearLeg),
            m_FacingDir,
            true);
    }

    float Actor::GetX() const { return m_X; }
    float Actor::GetY() const { return m_Y; }
    float Actor::GetWidth() const { return BodyWidth; }
    float Actor::GetHeight() const { return BodyHeight; }

    float Actor::GetCenterX() const { return m_X + BodyWidth * 0.5f; }

    float Actor::GetCenterY() const
    {
        // Center of the whole figure, legs included.
        return m_Y + (BodyHeight + StandHeight) * 0.5f;
    }

    float Actor::GetVelocityX() const { return m_VelocityX; }
    float Actor::GetVelocityY() const { return m_VelocityY; }

    bool Actor::IsGrounded() const { return m_Grounded; }

    const Limb& Actor::GetLeg(int index) const
    {
        return m_Legs[index];
    }

    bool Actor::IsBodyBoxFree(const Terrain& terrain, float x, float y) const
    {
        const float left = x + 0.5f;
        const float right = x + BodyWidth - 0.5f;
        const float top = y + 0.5f;
        const float bottom = y + BodyHeight - 0.5f;

        for (float sampleX = left; ; sampleX += SampleSpacing)
        {
            const float clampedX = std::fmin(sampleX, right);

            if (terrain.IsSolid(clampedX, top) ||
                terrain.IsSolid(clampedX, bottom))
            {
                return false;
            }

            if (clampedX >= right)
                break;
        }

        for (float sampleY = top; ; sampleY += SampleSpacing)
        {
            const float clampedY = std::fmin(sampleY, bottom);

            if (terrain.IsSolid(left, clampedY) ||
                terrain.IsSolid(right, clampedY))
            {
                return false;
            }

            if (clampedY >= bottom)
                break;
        }

        return true;
    }

    void Actor::MoveHorizontal(const Terrain& terrain, float delta)
    {
        const int steps = static_cast<int>(std::ceil(std::fabs(delta)));

        if (steps == 0)
            return;

        const float step = delta / static_cast<float>(steps);

        for (int i = 0; i < steps; i++)
        {
            if (IsBodyBoxFree(terrain, m_X + step, m_Y))
            {
                m_X += step;
                continue;
            }

            // Ride up over rising ground within a small clearance; real
            // steps are the legs' job.
            bool lifted = false;

            for (int lift = 1; lift <= BodyClearance; lift++)
            {
                if (IsBodyBoxFree(
                    terrain,
                    m_X + step,
                    m_Y - static_cast<float>(lift)))
                {
                    m_X += step;
                    m_Y -= static_cast<float>(lift);
                    lifted = true;
                    break;
                }
            }

            if (!lifted)
            {
                m_VelocityX = 0.0f;
                break;
            }
        }
    }

    void Actor::MoveVerticalToward(
        const Terrain& terrain,
        float targetY,
        float deltaTime)
    {
        const float maxStep = ClimbRate * deltaTime;
        const float delta = std::clamp(targetY - m_Y, -maxStep, maxStep);

        const int steps = static_cast<int>(std::ceil(std::fabs(delta)));

        if (steps == 0)
            return;

        const float step = delta / static_cast<float>(steps);

        for (int i = 0; i < steps; i++)
        {
            if (!IsBodyBoxFree(terrain, m_X, m_Y + step))
                break;

            m_Y += step;
        }
    }

    void Actor::MoveAirborne(const Terrain& terrain, float deltaTime)
    {
        const float deltaX = m_VelocityX * deltaTime;
        const int stepsX = static_cast<int>(std::ceil(std::fabs(deltaX)));

        if (stepsX > 0)
        {
            const float stepX = deltaX / static_cast<float>(stepsX);

            for (int i = 0; i < stepsX; i++)
            {
                if (!IsBodyBoxFree(terrain, m_X + stepX, m_Y))
                {
                    m_VelocityX = 0.0f;
                    break;
                }

                m_X += stepX;
            }
        }

        const float deltaY = m_VelocityY * deltaTime;
        const int stepsY = static_cast<int>(std::ceil(std::fabs(deltaY)));

        if (stepsY > 0)
        {
            const float stepY = deltaY / static_cast<float>(stepsY);

            for (int i = 0; i < stepsY; i++)
            {
                if (!IsBodyBoxFree(terrain, m_X, m_Y + stepY))
                {
                    // Torso landed on terrain directly (a rim or spike
                    // narrower than the legs' stance): crouch onto it.
                    if (stepY > 0.0f)
                    {
                        const float bottom = m_Y + BodyHeight;

                        m_Legs[0].Plant(HipWorldX(0), bottom + 2.0f);
                        m_Legs[1].Plant(HipWorldX(1), bottom + 2.0f);
                    }

                    m_VelocityY = 0.0f;
                    break;
                }

                m_Y += stepY;
            }
        }
    }

    bool Actor::FindFoothold(
        const Terrain& terrain,
        float probeX,
        float& footholdY) const
    {
        const float bodyBottom = m_Y + BodyHeight;

        const int top = static_cast<int>(bodyBottom - MaxStepUp);
        const int bottom = static_cast<int>(bodyBottom + StandHeight + MaxStepDown);

        for (int y = top; y <= bottom; y++)
        {
            if (terrain.IsSolid(probeX, static_cast<float>(y)) &&
                !terrain.IsSolid(probeX, static_cast<float>(y - 1)))
            {
                footholdY = static_cast<float>(y);
                return true;
            }
        }

        return false;
    }

    bool Actor::FindReachableFoothold(
        const Terrain& terrain,
        int leg,
        float probeX,
        float& footholdY) const
    {
        if (!FindFoothold(terrain, probeX, footholdY))
            return false;

        const float dx = probeX - HipWorldX(leg);
        const float dy = footholdY - HipWorldY(leg);

        return std::sqrt(dx * dx + dy * dy) <= PlantReach;
    }

    void Actor::UpdateWalkGait(const Terrain& terrain, float deltaTime)
    {
        Limb& swing = m_Legs[m_SwingLeg];
        Limb& stance = m_Legs[1 - m_SwingLeg];

        swing.Unplant();

        // If the stance leg is close to full stretch (fast on downhill
        // slopes), stomp the swinging foot straight down under its hip
        // now rather than finishing the full stride and losing footing.
        if (stance.IsPlanted())
        {
            const float stretchX =
                stance.GetFootX() - HipWorldX(1 - m_SwingLeg);
            const float stretchY =
                stance.GetFootY() - HipWorldY(1 - m_SwingLeg);

            const float stretch =
                std::sqrt(stretchX * stretchX + stretchY * stretchY);

            if (stretch > StanceStrain)
            {
                const float hipX = HipWorldX(m_SwingLeg);

                float emergencyY = 0.0f;

                if (FindReachableFoothold(
                    terrain, m_SwingLeg, hipX, emergencyY))
                {
                    swing.Plant(hipX, emergencyY);
                    m_SwingLeg = 1 - m_SwingLeg;
                    return;
                }
            }
        }

        // Reach for a foothold ahead of the swinging leg's hip; if the
        // ground there is out of reach, try progressively closer.
        const float hipX = HipWorldX(m_SwingLeg);

        float footholdY = 0.0f;
        bool found = false;
        float probeX = 0.0f;

        for (float fraction = 1.0f; fraction >= 0.0f; fraction -= 0.5f)
        {
            probeX = hipX + m_FacingDir * StepAhead * fraction;

            if (FindReachableFoothold(terrain, m_SwingLeg, probeX, footholdY))
            {
                found = true;
                break;
            }
        }

        if (found)
        {
            const float swingSpeed = std::max(
                140.0f,
                std::fabs(m_VelocityX) * 2.6f);

            if (swing.SwingToward(
                probeX,
                footholdY,
                swingSpeed,
                FootLift,
                deltaTime))
            {
                swing.Plant(probeX, footholdY);
                m_SwingLeg = 1 - m_SwingLeg;
            }
        }
        else
        {
            // Nothing to stand on ahead (walking off an edge): the foot
            // keeps reaching forward and down until the stance leg gives
            // out and the actor falls.
            swing.DangleToward(
                hipX + m_FacingDir * StepAhead,
                HipWorldY(m_SwingLeg) + swing.GetReach() * 0.85f,
                deltaTime);
        }
    }

    void Actor::UpdateIdleFeet(const Terrain& terrain, float deltaTime)
    {
        for (int i = 0; i < 2; i++)
        {
            if (m_Legs[i].IsPlanted())
                continue;

            const float hipX = HipWorldX(i);

            float footholdY = 0.0f;

            if (FindReachableFoothold(terrain, i, hipX, footholdY))
            {
                if (m_Legs[i].SwingToward(
                    hipX,
                    footholdY,
                    120.0f,
                    FootLift * 0.5f,
                    deltaTime))
                {
                    m_Legs[i].Plant(hipX, footholdY);
                }
            }
            else
            {
                m_Legs[i].DangleToward(
                    hipX,
                    HipWorldY(i) + m_Legs[i].GetReach() * 0.85f,
                    deltaTime);
            }
        }
    }

    void Actor::UpdateDanglingFeet(float deltaTime)
    {
        for (int i = 0; i < 2; i++)
        {
            m_Legs[i].Unplant();

            m_Legs[i].DangleToward(
                HipWorldX(i) + m_FacingDir * 2.0f,
                HipWorldY(i) + m_Legs[i].GetReach() * 0.8f,
                deltaTime);
        }
    }

    void Actor::TryLand(const Terrain& terrain)
    {
        // While falling, a leg catches the ground once a reachable surface
        // comes within range below. Each hip probes a few nearby columns
        // so a slope beside the hip still counts.
        for (int i = 0; i < 2; i++)
        {
            for (float offset : { 0.0f, 6.0f, -6.0f })
            {
                const float probeX = HipWorldX(i) + offset;

                float footholdY = 0.0f;

                if (FindReachableFoothold(terrain, i, probeX, footholdY))
                {
                    m_Legs[i].Plant(probeX, footholdY);
                    break;
                }
            }
        }
    }

    float Actor::HipWorldX(int leg) const
    {
        // Mirror the hips when facing left so the front hip leads.
        const float offset = m_Legs[leg].GetHipOffsetX();

        if (m_FacingDir < 0.0f)
            return m_X + BodyWidth - offset;

        return m_X + offset;
    }

    float Actor::HipWorldY(int leg) const
    {
        return m_Y + m_Legs[leg].GetHipOffsetY();
    }
}
