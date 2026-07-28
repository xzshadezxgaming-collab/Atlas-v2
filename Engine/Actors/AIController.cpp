#include "AIController.h"

#include "Actor.h"

#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float EngageRange = 620.0f;
        constexpr float PreferredRange = 340.0f;
    }

    AIController::AIController()
        : m_WanderDir(1.0f),
        m_WanderTimer(2.0f),
        m_BurstTimer(0.0f),
        m_PauseTimer(0.6f),
        m_AimWobble(0.0f),
        m_WobbleTimer(0.0f),
        m_RandomState(0xC0FFEEu)
    {
    }

    float AIController::RandomUnit()
    {
        m_RandomState ^= m_RandomState << 13;
        m_RandomState ^= m_RandomState >> 17;
        m_RandomState ^= m_RandomState << 5;

        return static_cast<float>(m_RandomState & 0xFFFF) / 32768.0f - 1.0f;
    }

    bool AIController::HasLineOfSight(
        const Actor& self,
        const Actor& target,
        const Terrain& terrain) const
    {
        float x = self.GetCenterX();
        float y = self.GetY() + 10.0f;

        const float targetX = target.GetCenterX();
        const float targetY = target.GetCenterY();

        const float dx = targetX - x;
        const float dy = targetY - y;
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > EngageRange)
            return false;

        const int steps = static_cast<int>(distance / 4.0f) + 1;

        const float stepX = dx / static_cast<float>(steps);
        const float stepY = dy / static_cast<float>(steps);

        for (int i = 0; i < steps; i++)
        {
            x += stepX;
            y += stepY;

            if (terrain.IsSolid(x, y))
                return false;
        }

        return true;
    }

    void AIController::Update(
        Actor& self,
        Actor* target,
        const Terrain& terrain,
        ParticleSystem& particles,
        Terrain& mutableTerrain,
        float deltaTime)
    {
        if (!self.IsAlive())
            return;

        const bool targetValid = target && target->IsAlive();

        const bool seesTarget =
            targetValid && HasLineOfSight(self, *target, terrain);

        float moveInput = 0.0f;

        if (seesTarget)
        {
            const float dx = target->GetCenterX() - self.GetCenterX();
            const float distance = std::fabs(dx);

            // Keep a fighting distance: advance if far, hold if close.
            if (distance > PreferredRange)
                moveInput = dx > 0.0f ? 0.6f : -0.6f;

            // Aim with wobble so enemies aren't laser-precise.
            m_WobbleTimer -= deltaTime;

            if (m_WobbleTimer <= 0.0f)
            {
                m_WobbleTimer = 0.25f;
                m_AimWobble = RandomUnit() * 26.0f;
            }

            self.SetAim(
                target->GetCenterX(),
                target->GetCenterY() + m_AimWobble);

            // Burst fire rhythm.
            if (m_BurstTimer > 0.0f)
            {
                m_BurstTimer -= deltaTime;

                if (!self.IsArmDestroyed() && self.GetWeapon().TryFire(
                    particles,
                    mutableTerrain,
                    self.GetMuzzleX(),
                    self.GetMuzzleY(),
                    self.GetAimDirX(),
                    self.GetAimDirY(),
                    target->GetCenterX(),
                    target->GetCenterY(),
                    &self))
                {
                    self.NotifyFired();
                }

                if (m_BurstTimer <= 0.0f)
                    m_PauseTimer = 0.5f + (RandomUnit() + 1.0f) * 0.4f;
            }
            else
            {
                m_PauseTimer -= deltaTime;

                if (m_PauseTimer <= 0.0f)
                    m_BurstTimer = 0.3f + (RandomUnit() + 1.0f) * 0.15f;
            }
        }
        else
        {
            self.ClearAim();

            // Wander: amble back and forth, flip when bumping into walls.
            m_WanderTimer -= deltaTime;

            if (m_WanderTimer <= 0.0f)
            {
                m_WanderTimer = 1.5f + (RandomUnit() + 1.0f) * 1.5f;

                const float roll = RandomUnit();

                if (roll < -0.3f)
                    m_WanderDir = -1.0f;
                else if (roll > 0.3f)
                    m_WanderDir = 1.0f;
                else
                    m_WanderDir = 0.0f;
            }

            if (m_WanderDir != 0.0f &&
                std::fabs(self.GetVelocityX()) < 5.0f &&
                self.IsGrounded())
            {
                // Bumped into something: turn around.
                m_WanderDir = -m_WanderDir;
            }

            moveInput = m_WanderDir * 0.45f;
        }

        self.GetWeapon().Update(deltaTime);
        self.Update(terrain, &particles, deltaTime, moveInput, false, false);
    }
}
