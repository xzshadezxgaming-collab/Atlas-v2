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

        // Close enough to a fixed target (a brain) to stop and shoot.
        constexpr float PointRange = 150.0f;

        // Close enough to the goal to stop marching and idle about.
        constexpr float GoalSlack = 50.0f;
    }

    AIController::AIController()
        : m_WanderDir(1.0f),
        m_WanderTimer(2.0f),
        m_BurstTimer(0.0f),
        m_PauseTimer(0.6f),
        m_AimWobble(0.0f),
        m_WobbleTimer(0.0f),
        m_StuckTimer(0.0f),
        m_JetTimer(0.0f),
        m_RandomState(0xC0FFEEu)
    {
    }

    void AIController::Seed(std::uint32_t seed)
    {
        // Xorshift must never be seeded with zero.
        m_RandomState = (seed * 2654435761u) ^ 0xC0FFEEu;

        if (m_RandomState == 0u)
            m_RandomState = 0xC0FFEEu;
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
        float targetX,
        float targetY,
        const Terrain& terrain) const
    {
        float x = self.GetCenterX();
        float y = self.GetY() + 10.0f;

        const float dx = targetX - x;
        const float dy = targetY - y;
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > EngageRange)
            return false;

        const int steps = static_cast<int>(distance / 4.0f) + 1;

        const float stepX = dx / static_cast<float>(steps);
        const float stepY = dy / static_cast<float>(steps);

        // The last step lands on the target itself, which may be solid
        // (a brain is made of terrain).
        for (int i = 0; i < steps - 1; i++)
        {
            x += stepX;
            y += stepY;

            if (terrain.IsSolid(x, y))
                return false;
        }

        return true;
    }

    void AIController::Engage(
        Actor& self,
        float x,
        float y,
        ParticleSystem& particles,
        Terrain& mutableTerrain,
        float deltaTime)
    {
        // Aim with wobble so soldiers aren't laser-precise.
        m_WobbleTimer -= deltaTime;

        if (m_WobbleTimer <= 0.0f)
        {
            m_WobbleTimer = 0.25f;
            m_AimWobble = RandomUnit() * 26.0f;
        }

        self.SetAim(x, y + m_AimWobble);

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
                x,
                y,
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

    void AIController::Update(
        Actor& self,
        const AIOrders& orders,
        const Terrain& terrain,
        ParticleSystem& particles,
        Terrain& mutableTerrain,
        float deltaTime)
    {
        if (!self.IsAlive())
            return;

        Actor* target = orders.Target;

        const bool seesTarget =
            target && target->IsAlive() &&
            HasLineOfSight(self, target->GetCenterX(), target->GetCenterY(), terrain);

        const float pointDx = orders.PointX - self.GetCenterX();
        const float pointDy = orders.PointY - self.GetCenterY();

        const bool seesPoint =
            !seesTarget &&
            orders.HasPoint &&
            pointDx * pointDx + pointDy * pointDy < EngageRange * EngageRange &&
            HasLineOfSight(self, orders.PointX, orders.PointY, terrain);

        float moveInput = 0.0f;
        bool wantsProgress = false; // moving with purpose (stuck detection applies)

        if (seesTarget)
        {
            const float dx = target->GetCenterX() - self.GetCenterX();

            // Keep a fighting distance: advance if far, hold if close.
            if (std::fabs(dx) > PreferredRange)
            {
                moveInput = dx > 0.0f ? 0.6f : -0.6f;
                wantsProgress = true;
            }

            Engage(self, target->GetCenterX(), target->GetCenterY(),
                particles, mutableTerrain, deltaTime);
        }
        else if (seesPoint)
        {
            if (std::fabs(pointDx) > PointRange)
            {
                moveInput = pointDx > 0.0f ? 0.6f : -0.6f;
                wantsProgress = true;
            }

            Engage(self, orders.PointX, orders.PointY,
                particles, mutableTerrain, deltaTime);
        }
        else
        {
            self.ClearAim();

            const float goalDx = orders.GoalX - self.GetCenterX();

            if (orders.HasGoal && std::fabs(goalDx) > GoalSlack)
            {
                moveInput = goalDx > 0.0f ? 0.7f : -0.7f;
                wantsProgress = true;
            }
            else
            {
                // Amble back and forth (around the goal, if there is
                // one); flip when bumping into walls.
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
                    m_WanderDir = -m_WanderDir;
                }

                moveInput = m_WanderDir * 0.45f;
            }
        }

        // Walked into a wall or a cliff on the way somewhere: jet over.
        if (wantsProgress && std::fabs(self.GetVelocityX()) < 8.0f)
            m_StuckTimer += deltaTime;
        else
            m_StuckTimer = 0.0f;

        if (m_StuckTimer > 0.35f && m_JetTimer <= 0.0f && self.GetFuel() > 0.4f)
        {
            m_JetTimer = 0.55f + (RandomUnit() + 1.0f) * 0.2f;
            m_StuckTimer = 0.0f;
        }

        const bool jet = m_JetTimer > 0.0f;

        if (jet)
            m_JetTimer -= deltaTime;

        self.GetWeapon().Update(deltaTime);
        self.Update(terrain, &particles, deltaTime, moveInput, jet, jet);
    }
}
