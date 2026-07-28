#include "Grenade.h"

#include "Explosion.h"

#include "../Core/Window.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float Gravity = 900.0f;
        constexpr float Bounce = 0.45f;
        constexpr float ExplosionRadius = 46.0f;
        constexpr int ExplosionDamage = 85;
    }

    void GrenadeSystem::Throw(
        float x,
        float y,
        float velX,
        float velY,
        float fuse)
    {
        m_Grenades.push_back({ x, y, velX, velY, fuse, 0.0f });
    }

    void GrenadeSystem::Update(
        Terrain& terrain,
        ParticleSystem& particles,
        Actor* const* actors,
        int actorCount,
        float deltaTime)
    {
        m_ExplodedThisFrame = false;

        for (std::size_t i = 0; i < m_Grenades.size(); )
        {
            Grenade& grenade = m_Grenades[i];

            grenade.Fuse -= deltaTime;

            if (grenade.Fuse <= 0.0f)
            {
                Explode(
                    terrain,
                    particles,
                    actors,
                    actorCount,
                    grenade.X,
                    grenade.Y,
                    ExplosionRadius,
                    ExplosionDamage);

                m_ExplodedThisFrame = true;

                m_Grenades[i] = m_Grenades.back();
                m_Grenades.pop_back();
                continue;
            }

            grenade.VelY += Gravity * deltaTime;

            // A thin smoke trail while airborne makes the arc readable.
            grenade.Trail -= deltaTime;

            if (grenade.Trail <= 0.0f)
            {
                particles.SpawnSmoke(
                    grenade.X, grenade.Y,
                    -grenade.VelX * 0.05f,
                    -grenade.VelY * 0.05f - 8.0f);
                grenade.Trail = 0.055f;
            }

            // Move one pixel at a time and bounce off terrain per axis.
            const float deltaX = grenade.VelX * deltaTime;
            const float deltaY = grenade.VelY * deltaTime;

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(std::max(
                    std::fabs(deltaX),
                    std::fabs(deltaY)))));

            const float stepX = deltaX / static_cast<float>(steps);
            const float stepY = deltaY / static_cast<float>(steps);

            for (int s = 0; s < steps; s++)
            {
                if (terrain.IsSolid(grenade.X + stepX, grenade.Y))
                {
                    grenade.VelX = -grenade.VelX * Bounce;
                }
                else
                {
                    grenade.X += stepX;
                }

                if (terrain.IsSolid(grenade.X, grenade.Y + stepY))
                {
                    grenade.VelY = -grenade.VelY * Bounce;

                    // Ground friction on each bounce.
                    grenade.VelX *= 0.8f;
                }
                else
                {
                    grenade.Y += stepY;
                }
            }

            i++;
        }
    }

    void GrenadeSystem::Draw(Window& window) const
    {
        for (const Grenade& grenade : m_Grenades)
        {
            window.DrawFilledRect(
                grenade.X - 2.0f,
                grenade.Y - 2.0f,
                4.0f,
                4.0f,
                40,
                48,
                40,
                255);

            // Blinking fuse light that blinks faster as time runs out.
            const float blinkRate = grenade.Fuse < 0.8f ? 16.0f : 6.0f;

            if (static_cast<int>(grenade.Fuse * blinkRate) % 2 == 0)
            {
                window.DrawFilledRect(
                    grenade.X - 1.0f,
                    grenade.Y - 3.0f,
                    2.0f,
                    2.0f,
                    255,
                    60,
                    40,
                    255);

                window.DrawGlow(
                    grenade.X, grenade.Y - 2.0f, 5.0f, 255, 70, 40, 90);
            }
        }
    }

    int GrenadeSystem::GetActiveCount() const
    {
        return static_cast<int>(m_Grenades.size());
    }

    void GrenadeSystem::Clear()
    {
        m_Grenades.clear();
    }

    bool GrenadeSystem::ExplodedThisFrame() const
    {
        return m_ExplodedThisFrame;
    }
}
