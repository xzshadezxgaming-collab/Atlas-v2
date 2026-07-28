#include "ParticleSystem.h"

#include "../Actors/Actor.h"
#include "../Core/Window.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr int MaxParticles = 6000;

        constexpr float BulletGravity = 240.0f;
        constexpr float DebrisGravity = 900.0f;

        bool HitsActor(const Actor& actor, float x, float y)
        {
            // Torso box plus the leg zone below it.
            const float left = actor.GetX() - 2.0f;
            const float right = actor.GetX() + actor.GetWidth() + 2.0f;
            const float top = actor.GetY();
            const float bottom = actor.GetY() + actor.GetHeight() + 26.0f;

            return x >= left && x <= right && y >= top && y <= bottom;
        }
    }

    ParticleSystem::ParticleSystem()
        : m_RandomState(0x9E3779B9u)
    {
        m_Particles.reserve(MaxParticles);
    }

    float ParticleSystem::RandomUnit()
    {
        // xorshift32, mapped to [-1, 1].
        m_RandomState ^= m_RandomState << 13;
        m_RandomState ^= m_RandomState >> 17;
        m_RandomState ^= m_RandomState << 5;

        return static_cast<float>(m_RandomState & 0xFFFF) / 32768.0f - 1.0f;
    }

    void ParticleSystem::Push(const Particle& particle)
    {
        if (static_cast<int>(m_Particles.size()) >= MaxParticles)
            return;

        m_Particles.push_back(particle);
    }

    void ParticleSystem::SpawnBullet(
        float x,
        float y,
        float velX,
        float velY,
        int damage,
        float power,
        Actor* owner)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 2.5f;
        p.MaxLife = p.Life;
        p.Gravity = BulletGravity;
        p.Power = power;
        p.Damage = static_cast<std::int16_t>(damage);
        p.R = 255;
        p.G = 232;
        p.B = 128;
        p.Size = 2;
        p.Type = ParticleType::Bullet;
        p.SettleMaterial = Material::Air;
        p.Owner = owner;

        Push(p);
    }

    void ParticleSystem::SpawnDebris(
        float x,
        float y,
        float velX,
        float velY,
        Material material)
    {
        const MaterialInfo& info = GetMaterialInfo(material);

        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 6.0f;
        p.MaxLife = p.Life;
        p.Gravity = DebrisGravity;
        p.R = info.R;
        p.G = info.G;
        p.B = info.B;
        p.Size = 1;
        p.Type = ParticleType::Debris;
        p.SettleMaterial = material;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnBlood(float x, float y, float velX, float velY)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 3.0f;
        p.MaxLife = p.Life;
        p.Gravity = DebrisGravity;
        p.R = 158;
        p.G = 24;
        p.B = 24;
        p.Size = 1;
        p.Type = ParticleType::Blood;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnSpark(float x, float y, float velX, float velY)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 0.25f + RandomUnit() * 0.1f;
        p.MaxLife = p.Life;
        p.Gravity = 400.0f;
        p.R = 255;
        p.G = 210;
        p.B = 90;
        p.Size = 1;
        p.Type = ParticleType::Spark;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnSmoke(float x, float y, float velX, float velY)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 0.7f + RandomUnit() * 0.3f;
        p.MaxLife = p.Life;
        p.Gravity = -160.0f; // drifts upward
        p.R = 130;
        p.G = 128;
        p.B = 124;
        p.Size = 2;
        p.Type = ParticleType::Smoke;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnFire(float x, float y, float velX, float velY)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 0.4f + RandomUnit() * 0.15f;
        p.MaxLife = p.Life;
        p.Gravity = -120.0f; // fire rises
        p.R = 255;
        p.G = 150;
        p.B = 60;
        p.Size = 3;
        p.Type = ParticleType::Fire;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnCasing(float x, float y, float velX, float velY)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 1.4f;
        p.MaxLife = p.Life;
        p.Gravity = DebrisGravity;
        p.Bounces = 2;
        p.R = 214;
        p.G = 178;
        p.B = 86;
        p.Size = 1;
        p.Type = ParticleType::Casing;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::SpawnGib(
        float x,
        float y,
        float velX,
        float velY,
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b,
        int size)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 8.0f;
        p.MaxLife = p.Life;
        p.Gravity = DebrisGravity;
        p.R = r;
        p.G = g;
        p.B = b;
        p.Size = static_cast<std::uint8_t>(std::clamp(size, 1, 4));
        p.Type = ParticleType::Gib;
        p.SettleMaterial = Material::Air;
        p.Owner = nullptr;

        Push(p);
    }

    void ParticleSystem::BurstBlood(float x, float y, int count, float speed)
    {
        for (int i = 0; i < count; i++)
        {
            SpawnBlood(
                x,
                y,
                RandomUnit() * speed,
                RandomUnit() * speed - speed * 0.4f);
        }
    }

    void ParticleSystem::BurstDebris(
        float x,
        float y,
        int count,
        float speed,
        Material material)
    {
        for (int i = 0; i < count; i++)
        {
            SpawnDebris(
                x,
                y,
                RandomUnit() * speed,
                RandomUnit() * speed - speed * 0.5f,
                material);
        }
    }

    void ParticleSystem::Update(
        Terrain& terrain,
        float deltaTime,
        Actor* const* actors,
        int actorCount)
    {
        const float worldWidth = static_cast<float>(terrain.GetWidth());
        const float worldHeight = static_cast<float>(terrain.GetHeight());

        for (std::size_t i = 0; i < m_Particles.size(); )
        {
            Particle& p = m_Particles[i];

            p.Life -= deltaTime;
            p.VelY += p.Gravity * deltaTime;

            bool alive = p.Life > 0.0f;

            if (alive)
            {
                const float deltaX = p.VelX * deltaTime;
                const float deltaY = p.VelY * deltaTime;

                const int steps = std::max(
                    1,
                    static_cast<int>(std::ceil(std::max(
                        std::fabs(deltaX),
                        std::fabs(deltaY)))));

                const float stepX = deltaX / static_cast<float>(steps);
                const float stepY = deltaY / static_cast<float>(steps);

                for (int s = 0; s < steps && alive; s++)
                {
                    const float prevX = p.X;
                    const float prevY = p.Y;

                    p.X += stepX;
                    p.Y += stepY;

                    // Out of the world: cull (smoke may pass above freely).
                    if (p.X < -64.0f || p.X > worldWidth + 64.0f ||
                        p.Y > worldHeight + 64.0f || p.Y < -400.0f)
                    {
                        alive = false;
                        break;
                    }

                    // Actor hits (bullets only).
                    if (p.Type == ParticleType::Bullet && actors)
                    {
                        for (int a = 0; a < actorCount && alive; a++)
                        {
                            Actor* actor = actors[a];

                            if (!actor || actor == p.Owner || !actor->IsAlive())
                                continue;

                            if (HitsActor(*actor, p.X, p.Y))
                            {
                                actor->TakeDamage(
                                    p.Damage,
                                    p.VelX * 0.03f,
                                    p.VelY * 0.03f);

                                BurstBlood(p.X, p.Y, 6, 140.0f);

                                alive = false;
                            }
                        }

                        if (!alive)
                            break;
                    }

                    if (p.Type == ParticleType::Smoke ||
                        p.Type == ParticleType::Fire)
                        continue;

                    if (!terrain.IsSolid(p.X, p.Y))
                        continue;

                    // Terrain contact.
                    const int px = static_cast<int>(std::floor(p.X));
                    const int py = static_cast<int>(std::floor(p.Y));

                    switch (p.Type)
                    {
                    case ParticleType::Bullet:
                    {
                        const Material material = terrain.GetMaterial(px, py);
                        const float strength =
                            GetMaterialInfo(material).Strength;

                        // Outside the world's pixel grid (side walls):
                        // nothing to chew through.
                        if (material == Material::Air)
                        {
                            alive = false;
                            break;
                        }

                        if (p.Power >= strength)
                        {
                            // Punch through the pixel, knock it loose.
                            const int value = GetMaterialInfo(material).Value;

                            terrain.DestroyPixel(px, py);
                            p.Power -= strength;

                            if (value > 0 && p.Owner)
                                p.Owner->AddGold(value);

                            if ((m_RandomState & 3u) == 0u)
                            {
                                SpawnDebris(
                                    prevX,
                                    prevY,
                                    p.VelX * 0.1f + RandomUnit() * 60.0f,
                                    -std::fabs(p.VelY) * 0.1f - 60.0f +
                                        RandomUnit() * 40.0f,
                                    material);
                            }

                            RandomUnit();
                        }
                        else
                        {
                            // Stopped: sparks on hard material.
                            SpawnSpark(prevX, prevY,
                                -p.VelX * 0.08f + RandomUnit() * 90.0f,
                                -std::fabs(p.VelY) * 0.08f - 70.0f);
                            SpawnSpark(prevX, prevY,
                                -p.VelX * 0.05f + RandomUnit() * 90.0f,
                                -60.0f + RandomUnit() * 50.0f);

                            alive = false;
                        }
                        break;
                    }

                    case ParticleType::Debris:
                    {
                        // Settle into the terrain at the last free spot.
                        const int settleX = static_cast<int>(std::floor(prevX));
                        const int settleY = static_cast<int>(std::floor(prevY));

                        if (!terrain.IsSolid(prevX, prevY))
                        {
                            terrain.SetMaterial(
                                settleX, settleY, p.SettleMaterial);
                        }

                        alive = false;
                        break;
                    }

                    case ParticleType::Blood:
                    {
                        terrain.StainPixel(px, py, p.R, p.G, p.B);
                        terrain.StainPixel(px + 1, py, p.R, p.G, p.B);

                        alive = false;
                        break;
                    }

                    case ParticleType::Gib:
                    {
                        // Meat lands: stain and stop.
                        terrain.StainPixel(px, py, 158, 24, 24);

                        const int settleX = static_cast<int>(std::floor(prevX));
                        const int settleY = static_cast<int>(std::floor(prevY));

                        if (!terrain.IsSolid(prevX, prevY) && p.Size >= 2)
                        {
                            terrain.SetMaterial(settleX, settleY, Material::Dirt);
                            terrain.StainPixel(settleX, settleY, p.R, p.G, p.B);
                        }

                        alive = false;
                        break;
                    }

                    case ParticleType::Casing:
                    {
                        if (p.Bounces > 0)
                        {
                            p.Bounces--;
                            p.X = prevX;
                            p.Y = prevY;
                            p.VelY = -std::fabs(p.VelY) * 0.4f;
                            p.VelX *= 0.6f;
                        }
                        else
                        {
                            alive = false;
                        }
                        break;
                    }

                    case ParticleType::Spark:
                    default:
                        alive = false;
                        break;
                    }
                }
            }

            if (alive)
            {
                i++;
            }
            else
            {
                m_Particles[i] = m_Particles.back();
                m_Particles.pop_back();
            }
        }
    }

    void ParticleSystem::Draw(Window& window) const
    {
        // Pass 1: matte particles (smoke behind everything, then solids).
        for (const Particle& p : m_Particles)
        {
            const float age = 1.0f - p.Life / p.MaxLife;

            switch (p.Type)
            {
            case ParticleType::Smoke:
            {
                // Grows and thins out as it rises.
                const float size = 2.0f + age * 9.0f;
                const Uint8 alpha = static_cast<Uint8>(120.0f * (1.0f - age));
                const Uint8 tone = static_cast<Uint8>(120.0f + age * 40.0f);

                window.DrawFilledRect(
                    p.X - size * 0.5f, p.Y - size * 0.5f, size, size,
                    tone, tone, static_cast<Uint8>(tone - 4), alpha);
                break;
            }

            case ParticleType::Debris:
            case ParticleType::Blood:
            case ParticleType::Gib:
            {
                const float size = static_cast<float>(p.Size);

                window.DrawFilledRect(
                    p.X - size * 0.5f, p.Y - size * 0.5f, size, size,
                    p.R, p.G, p.B, 255);
                break;
            }

            case ParticleType::Casing:
            {
                window.DrawFilledRect(
                    p.X - 1.0f, p.Y - 0.5f, 2.0f, 1.5f,
                    p.R, p.G, p.B, 255);
                break;
            }

            default:
                break;
            }
        }

        // Pass 2: glowing particles on top (fire, sparks, tracers).
        for (const Particle& p : m_Particles)
        {
            const float age = 1.0f - p.Life / p.MaxLife;

            switch (p.Type)
            {
            case ParticleType::Fire:
            {
                // Expanding additive fireball fading orange -> red.
                const float radius = 4.0f + age * 14.0f;
                const Uint8 alpha = static_cast<Uint8>(70.0f * (1.0f - age));

                window.DrawGlow(
                    p.X, p.Y, radius,
                    255,
                    static_cast<Uint8>(150.0f * (1.0f - age * 0.6f)),
                    40,
                    alpha);

                if (age < 0.5f)
                {
                    const float core = 3.0f * (1.0f - age);

                    window.DrawFilledRect(
                        p.X - core * 0.5f, p.Y - core * 0.5f, core, core,
                        255, 232, 160, 255);
                }
                break;
            }

            case ParticleType::Spark:
            {
                // White-hot -> amber -> ember red.
                Uint8 r = 255, g = 220, b = 150;

                if (age > 0.35f)
                {
                    r = 255; g = 140; b = 40;
                }

                if (age > 0.7f)
                {
                    r = 150; g = 45; b = 20;
                }

                window.DrawFilledRect(
                    p.X - 1.0f, p.Y - 1.0f, 2.0f, 2.0f, r, g, b, 255);

                if (age < 0.4f)
                    window.DrawGlow(p.X, p.Y, 5.0f, 255, 190, 90, 40);

                break;
            }

            case ParticleType::Bullet:
            {
                // Tracer streak along the velocity, glowing head.
                const float speed = std::sqrt(
                    p.VelX * p.VelX + p.VelY * p.VelY) + 0.001f;

                const float streak = std::min(14.0f, speed * 0.014f);

                const float tailX = p.X - p.VelX / speed * streak;
                const float tailY = p.Y - p.VelY / speed * streak;

                for (int seg = 0; seg < 3; seg++)
                {
                    const float t = static_cast<float>(seg) / 3.0f;

                    window.DrawFilledRect(
                        p.X + (tailX - p.X) * t - 1.0f,
                        p.Y + (tailY - p.Y) * t - 1.0f,
                        2.0f,
                        2.0f,
                        255,
                        static_cast<Uint8>(240 - seg * 50),
                        static_cast<Uint8>(170 - seg * 50),
                        static_cast<Uint8>(255 - seg * 70));
                }

                window.DrawGlow(p.X, p.Y, 6.0f, 255, 210, 110, 55);
                break;
            }

            default:
                break;
            }
        }
    }

    int ParticleSystem::GetActiveCount() const
    {
        return static_cast<int>(m_Particles.size());
    }

    void ParticleSystem::Clear()
    {
        m_Particles.clear();
    }
}
