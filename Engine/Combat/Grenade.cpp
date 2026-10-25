#include "Grenade.h"

#include "Explosion.h"

#include "../Core/Window.h"
#include "../Graphics/SpriteBank.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include "../Actors/Actor.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float Gravity = 900.0f;
        constexpr float Bounce = 0.45f;
        constexpr float ExplosionRadius = 46.0f;
        constexpr int ExplosionDamage = 85;

        constexpr float RocketLifetime = 3.5f;
        constexpr float RocketGravity = 70.0f;
        constexpr float RocketThrust = 900.0f; // up to RocketMaxSpeed
        constexpr float RocketMaxSpeed = 900.0f;
    }

    void GrenadeSystem::Throw(
        float x,
        float y,
        float velX,
        float velY,
        int team,
        float fuse)
    {
        m_Grenades.push_back({ x, y, velX, velY, fuse, 0.0f, team });
    }

    void GrenadeSystem::Launch(
        float x,
        float y,
        float velX,
        float velY,
        int team,
        unsigned int ownerId,
        float radius,
        int damage)
    {
        Grenade rocket{ x, y, velX, velY, RocketLifetime, 0.0f, team };
        rocket.Rocket = true;
        rocket.Radius = radius;
        rocket.Damage = damage;
        rocket.OwnerId = ownerId;
        m_Grenades.push_back(rocket);
    }

    const std::vector<Blast>& GrenadeSystem::GetBlasts() const
    {
        return m_Blasts;
    }

    void GrenadeSystem::SetProximityTargets(const std::vector<Blast>& targets)
    {
        m_Proximity = targets;
    }

    int GrenadeSystem::GetRocketCount() const
    {
        int count = 0;

        for (const Grenade& grenade : m_Grenades)
            count += grenade.Rocket ? 1 : 0;

        return count;
    }

    const std::vector<Grenade>& GrenadeSystem::GetAll() const
    {
        return m_Grenades;
    }

    void GrenadeSystem::Update(
        Terrain& terrain,
        ParticleSystem& particles,
        Actor* const* actors,
        int actorCount,
        float deltaTime)
    {
        m_ExplodedThisFrame = false;
        m_Blasts.clear();

        for (std::size_t i = 0; i < m_Grenades.size(); )
        {
            Grenade& grenade = m_Grenades[i];

            grenade.Fuse -= deltaTime;

            if (grenade.Rocket && grenade.Fuse > 0.0f)
            {
                // Motor: accelerates along its heading, sags a little.
                const float speed = std::sqrt(
                    grenade.VelX * grenade.VelX + grenade.VelY * grenade.VelY) + 0.001f;

                if (speed < RocketMaxSpeed)
                {
                    const float boost = std::min(RocketMaxSpeed, speed + RocketThrust * deltaTime) / speed;
                    grenade.VelX *= boost;
                    grenade.VelY *= boost;
                }

                grenade.VelY += RocketGravity * deltaTime;

                grenade.Trail -= deltaTime;

                if (grenade.Trail <= 0.0f)
                {
                    particles.SpawnSmoke(
                        grenade.X - grenade.VelX / speed * 6.0f,
                        grenade.Y - grenade.VelY / speed * 6.0f,
                        -grenade.VelX * 0.04f, -grenade.VelY * 0.04f - 10.0f);
                    particles.SpawnSpark(
                        grenade.X - grenade.VelX / speed * 5.0f,
                        grenade.Y - grenade.VelY / speed * 5.0f,
                        -grenade.VelX * 0.2f, -grenade.VelY * 0.2f);
                    grenade.Trail = 0.02f;
                }

                // Fly one pixel at a time; anything solid or any body in
                // the way sets it off.
                const float deltaX = grenade.VelX * deltaTime;
                const float deltaY = grenade.VelY * deltaTime;

                const int steps = std::max(1, static_cast<int>(std::ceil(
                    std::max(std::fabs(deltaX), std::fabs(deltaY)))));

                bool hit = false;

                for (int s = 0; s < steps && !hit; s++)
                {
                    const float nextX = grenade.X + deltaX / static_cast<float>(steps);
                    const float nextY = grenade.Y + deltaY / static_cast<float>(steps);

                    if (terrain.IsSolid(nextX, nextY))
                    {
                        hit = true;
                        break;
                    }

                    grenade.X = nextX;
                    grenade.Y = nextY;

                    // Bodies only get checked every few pixels (cheap,
                    // and rockets are fat anyway).
                    if (s % 3 != 0)
                        continue;

                    for (int a = 0; a < actorCount && !hit; a++)
                    {
                        const Actor* actor = actors[a];

                        if (!actor || !actor->IsAlive() ||
                            actor->GetId() == grenade.OwnerId ||
                            (grenade.Team >= 0 && actor->GetTeam() == grenade.Team))
                            continue;

                        hit = grenade.X >= actor->GetX() - 3.0f &&
                            grenade.X <= actor->GetX() + actor->GetWidth() + 3.0f &&
                            grenade.Y >= actor->GetY() - 3.0f &&
                            grenade.Y <= actor->GetY() + actor->GetHeight() + 22.0f;
                    }

                    for (const Blast& target : m_Proximity)
                    {
                        if (hit)
                            break;

                        // Proximity targets carry their team in Damage.
                        if (grenade.Team >= 0 && target.Damage == grenade.Team)
                            continue;

                        const float dx = grenade.X - target.X;
                        const float dy = grenade.Y - target.Y;
                        hit = dx * dx + dy * dy < target.Radius * target.Radius;
                    }
                }

                if (grenade.X < -50.0f || grenade.Y < -600.0f ||
                    grenade.X > static_cast<float>(terrain.GetWidth()) + 50.0f ||
                    grenade.Y > static_cast<float>(terrain.GetHeight()) + 50.0f)
                {
                    m_Grenades[i] = m_Grenades.back();
                    m_Grenades.pop_back();
                    continue;
                }

                if (!hit)
                {
                    i++;
                    continue;
                }

                grenade.Fuse = 0.0f;
            }

            if (grenade.Fuse <= 0.0f)
            {
                const float radius = grenade.Rocket ? grenade.Radius : ExplosionRadius;
                const int damage = grenade.Rocket ? grenade.Damage : ExplosionDamage;

                Explode(
                    terrain,
                    particles,
                    actors,
                    actorCount,
                    grenade.X,
                    grenade.Y,
                    radius,
                    damage,
                    grenade.Team);

                m_Blasts.push_back({ grenade.X, grenade.Y, radius, damage });
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
            if (grenade.Rocket)
            {
                const float speed = std::sqrt(
                    grenade.VelX * grenade.VelX + grenade.VelY * grenade.VelY) + 0.001f;
                const float dirX = grenade.VelX / speed;
                const float dirY = grenade.VelY / speed;

                // Olive body, red nose, white-hot exhaust.
                if (const SpriteFrame* sprite = SpriteBank::Find("rocket"))
                {
                    window.DrawSprite(*sprite, grenade.X - dirX * 3.0f, grenade.Y - dirY * 3.0f,
                        std::atan2(dirY, dirX) * 57.2957795f);
                }
                else
                {
                    for (int seg = 0; seg < 7; seg++)
                    {
                        const float t = static_cast<float>(seg);
                        const bool nose = seg >= 5;

                        window.DrawFilledRect(
                            grenade.X - dirX * (6.0f - t) - 1.5f,
                            grenade.Y - dirY * (6.0f - t) - 1.5f,
                            3.0f, 3.0f,
                            nose ? 190 : 86, nose ? 50 : 96, nose ? 40 : 70, 255);
                    }
                }

                window.DrawGlow(grenade.X - dirX * 8.0f, grenade.Y - dirY * 8.0f,
                    10.0f, 255, 170, 70, 120);
                window.DrawFilledRect(
                    grenade.X - dirX * 8.0f - 1.5f, grenade.Y - dirY * 8.0f - 1.5f,
                    3.0f, 3.0f, 255, 240, 200, 255);
                continue;
            }

            if (const SpriteFrame* sprite = SpriteBank::Find("grenade"))
            {
                // Tumbles with its horizontal speed.
                window.DrawSprite(*sprite, grenade.X, grenade.Y,
                    std::fmod(grenade.X * 9.0f, 360.0f));
            }
            else
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
            }

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
