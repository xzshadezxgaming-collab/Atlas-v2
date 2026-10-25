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
        SpawnBullet(x, y, velX, velY, damage, power,
            owner ? owner->GetId() : 0u,
            owner ? owner->GetTeam() : -1);
    }

    void ParticleSystem::SpawnBullet(
        float x,
        float y,
        float velX,
        float velY,
        int damage,
        float power,
        std::uint32_t ownerId,
        int ownerTeam)
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
        p.OwnerId = ownerId;
        p.OwnerTeam = static_cast<std::int8_t>(ownerTeam);

        Push(p);
    }

    void ParticleSystem::SpawnFlame(
        float x,
        float y,
        float velX,
        float velY,
        int damage,
        std::uint32_t ownerId,
        int ownerTeam)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 0.5f + RandomUnit() * 0.1f;
        p.MaxLife = p.Life;
        p.Gravity = -110.0f; // hot: rises
        p.Damage = static_cast<std::int16_t>(damage);
        p.R = 255;
        p.G = 150;
        p.B = 50;
        p.Size = 2;
        p.Type = ParticleType::Flame;
        p.SettleMaterial = Material::Air;
        p.OwnerId = ownerId;
        p.OwnerTeam = static_cast<std::int8_t>(ownerTeam);

        Push(p);
    }

    void ParticleSystem::SetHitCircles(const std::vector<HitCircle>& circles)
    {
        m_Circles = circles;
    }

    const std::vector<CircleHit>& ParticleSystem::GetCircleHits() const
    {
        return m_CircleHits;
    }

    bool ParticleSystem::HitTestCircles(const Particle& p)
    {
        for (const HitCircle& circle : m_Circles)
        {
            if (circle.Id == p.OwnerId || (p.OwnerTeam >= 0 && circle.Team == p.OwnerTeam))
                continue;

            const float dx = p.X - circle.X;
            const float dy = p.Y - circle.Y;

            if (dx * dx + dy * dy > circle.Radius * circle.Radius)
                continue;

            m_CircleHits.push_back({ circle.Id, p.Damage, p.Type == ParticleType::Flame });
            return true;
        }

        return false;
    }

    void ParticleSystem::SetFriendlyBrainBox(int team, int minX, int minY, int maxX, int maxY)
    {
        if (team < 0 || team >= 2)
            return;

        m_BrainBoxes[team] = { true, minX, minY, maxX, maxY };
    }

    bool ParticleSystem::GetFriendlyBrainBox(int team, Terrain::Box& out) const
    {
        if (team < 0 || team >= 2 || !m_BrainBoxes[team].Set)
            return false;

        const BrainBox& box = m_BrainBoxes[team];
        out = { box.MinX, box.MinY, box.MaxX, box.MaxY };
        return true;
    }

    bool ParticleSystem::IsFriendlyBrain(int team, int x, int y) const
    {
        if (team < 0 || team >= 2 || !m_BrainBoxes[team].Set)
            return false;

        const BrainBox& box = m_BrainBoxes[team];
        return x >= box.MinX && x <= box.MaxX && y >= box.MinY && y <= box.MaxY;
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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

        Push(p);
    }

    void ParticleSystem::SpawnDust(
        float x,
        float y,
        float velX,
        float velY,
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.VelX = velX;
        p.VelY = velY;
        p.Life = 0.45f + RandomUnit() * 0.15f;
        p.MaxLife = p.Life;
        p.Gravity = -30.0f; // hangs, drifts gently up as it disperses
        p.R = r;
        p.G = g;
        p.B = b;
        p.Size = 2;
        p.Type = ParticleType::Dust;
        p.SettleMaterial = Material::Air;
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

        Push(p);
    }

    void ParticleSystem::SpawnShockwave(float x, float y)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.Life = 0.32f;
        p.MaxLife = p.Life;
        p.R = 255;
        p.G = 228;
        p.B = 190;
        p.Size = 2;
        p.Type = ParticleType::Shockwave;
        p.SettleMaterial = Material::Air;
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

        Push(p);
    }

    void ParticleSystem::SpawnFlash(float x, float y, float radius)
    {
        Particle p{};
        p.X = x;
        p.Y = y;
        p.Life = 0.11f;
        p.MaxLife = p.Life;
        p.Power = radius; // reused as the bloom radius
        p.R = 255;
        p.G = 240;
        p.B = 200;
        p.Size = 1;
        p.Type = ParticleType::Flash;
        p.SettleMaterial = Material::Air;
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        p.OwnerId = 0u;
        p.OwnerTeam = -1;

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
        int actorCount,
        std::vector<GoldCredit>* outGold)
    {
        const float worldWidth = static_cast<float>(terrain.GetWidth());
        const float worldHeight = static_cast<float>(terrain.GetHeight());

        m_CircleHits.clear();

        for (std::size_t r = 0; r < m_Ragdolls.size(); )
        {
            m_Ragdolls[r].Update(terrain, *this, deltaTime);

            if (m_Ragdolls[r].IsExpired())
                m_Ragdolls.erase(m_Ragdolls.begin() + static_cast<std::ptrdiff_t>(r));
            else
                r++;
        }

        for (std::size_t i = 0; i < m_Particles.size(); )
        {
            Particle& p = m_Particles[i];

            p.Life -= deltaTime;
            p.VelY += p.Gravity * deltaTime;

            // Fuel sprays out fast and slows to a lick of flame.
            if (p.Type == ParticleType::Flame)
            {
                const float drag = std::max(0.0f, 1.0f - 3.2f * deltaTime);
                p.VelX *= drag;
                p.VelY *= drag;
            }

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

                    // Actor hits (bullets only), tested against each
                    // actor's independent limb hitboxes rather than one
                    // whole-body box.
                    if ((p.Type == ParticleType::Bullet || p.Type == ParticleType::Flame) &&
                        !m_Circles.empty() && HitTestCircles(p))
                    {
                        if (p.Type == ParticleType::Bullet)
                        {
                            SpawnSpark(p.X, p.Y, -p.VelX * 0.05f + RandomUnit() * 80.0f, -60.0f);
                            SpawnSpark(p.X, p.Y, RandomUnit() * 90.0f, -40.0f + RandomUnit() * 40.0f);
                        }

                        alive = false;
                        break;
                    }

                    if (p.Type == ParticleType::Flame && actors && (s % 2) == 0)
                    {
                        for (int a = 0; a < actorCount && alive; a++)
                        {
                            Actor* actor = actors[a];

                            if (!actor || !actor->IsAlive() ||
                                (p.OwnerId != 0u && actor->GetId() == p.OwnerId) ||
                                (p.OwnerTeam >= 0 && actor->GetTeam() == p.OwnerTeam))
                                continue;

                            if (p.X < actor->GetX() - 2.0f ||
                                p.X > actor->GetX() + actor->GetWidth() + 2.0f ||
                                p.Y < actor->GetY() ||
                                p.Y > actor->GetY() + actor->GetHeight() + 20.0f)
                                continue;

                            actor->TakeDamage(p.Damage, p.VelX * 0.01f, 0.0f);
                            actor->Ignite(2.5f);
                            alive = false;
                        }

                        if (!alive)
                            break;
                    }

                    if (p.Type == ParticleType::Bullet && actors)
                    {
                        for (int a = 0; a < actorCount && alive; a++)
                        {
                            Actor* actor = actors[a];

                            if (!actor || !actor->IsAlive() ||
                                (p.OwnerId != 0u && actor->GetId() == p.OwnerId))
                                continue;

                            // No friendly fire: a bullet only hurts
                            // actors on a different team than its owner.
                            if (p.OwnerTeam >= 0 && actor->GetTeam() == p.OwnerTeam)
                                continue;

                            BodyPartId hitPart = BodyPartId::None;
                            float local01X = 0.0f;
                            float local01Y = 0.0f;

                            if (actor->TestLimbHit(
                                p.X, p.Y, hitPart, local01X, local01Y))
                            {
                                actor->TakeLimbDamage(
                                    hitPart,
                                    p.Damage,
                                    local01X,
                                    local01Y,
                                    this,
                                    p.VelX * 0.03f,
                                    p.VelY * 0.03f);

                                BurstBlood(p.X, p.Y, 6, 140.0f);

                                alive = false;
                            }
                        }

                        if (alive && !m_Ragdolls.empty() && HitRagdolls(p))
                            alive = false;

                        if (!alive)
                            break;
                    }

                    if (p.Type == ParticleType::Smoke ||
                        p.Type == ParticleType::Fire ||
                        p.Type == ParticleType::Dust ||
                        p.Type == ParticleType::Shockwave ||
                        p.Type == ParticleType::Flash)
                        continue;

                    if (p.Type == ParticleType::Flame)
                    {
                        const int fx = static_cast<int>(std::floor(p.X));
                        const int fy = static_cast<int>(std::floor(p.Y));
                        const Material material = terrain.GetMaterial(fx, fy);

                        // Doused.
                        if (IsFluid(material) && material != Material::Lava)
                        {
                            SpawnSmoke(prevX, prevY, RandomUnit() * 10.0f, -30.0f);
                            alive = false;
                            break;
                        }

                        if (!terrain.IsSolid(p.X, p.Y))
                            continue;

                        // Grass burns away; everything else gets scorched.
                        if (material == Material::Grass && (m_RandomState & 3u) == 0u)
                            terrain.DestroyPixel(fx, fy);
                        else if ((m_RandomState & 7u) == 0u)
                            terrain.StainPixel(fx, fy, 40, 32, 28);

                        RandomUnit();
                        alive = false;
                        break;
                    }

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

                        if (material == Material::Brain &&
                            IsFriendlyBrain(p.OwnerTeam, px, py))
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

                            if (value > 0 && p.OwnerId != 0u && outGold)
                                outGold->push_back({ p.OwnerId, value });

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
                            // Stopped: sparks on hard material, plus a
                            // puff of dust in the material's own color.
                            SpawnSpark(prevX, prevY,
                                -p.VelX * 0.08f + RandomUnit() * 90.0f,
                                -std::fabs(p.VelY) * 0.08f - 70.0f);
                            SpawnSpark(prevX, prevY,
                                -p.VelX * 0.05f + RandomUnit() * 90.0f,
                                -60.0f + RandomUnit() * 50.0f);

                            const MaterialInfo& impactInfo =
                                GetMaterialInfo(material);

                            SpawnDust(prevX, prevY,
                                -p.VelX * 0.03f,
                                -22.0f,
                                static_cast<std::uint8_t>(
                                    std::min(255, impactInfo.R + 30)),
                                static_cast<std::uint8_t>(
                                    std::min(255, impactInfo.G + 30)),
                                static_cast<std::uint8_t>(
                                    std::min(255, impactInfo.B + 30)));

                            alive = false;
                        }
                        break;
                    }

                    case ParticleType::Debris:
                    {
                        // Settle into the terrain at the last free spot.
                        const int settleX = static_cast<int>(std::floor(prevX));
                        const int settleY = static_cast<int>(std::floor(prevY));

                        // Some things don't pile back up: brain matter
                        // splatters, glass shatters, concrete crumbles
                        // to rubble.
                        Material settle = p.SettleMaterial;

                        if (settle == Material::Brain)
                        {
                            terrain.StainPixel(px, py, 190, 70, 90);
                            settle = Material::Air;
                        }
                        else if (settle == Material::Glass)
                        {
                            settle = Material::Air;
                        }
                        else if (settle == Material::Concrete)
                        {
                            settle = Material::Gravel;
                        }

                        if (settle != Material::Air && !terrain.IsSolid(prevX, prevY))
                            terrain.SetMaterial(settleX, settleY, settle);

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

            case ParticleType::Dust:
            {
                // Disperses: grows while fading, like a kicked-up puff.
                const float size = 2.0f + age * 7.0f;
                const Uint8 alpha = static_cast<Uint8>(85.0f * (1.0f - age));

                window.DrawFilledRect(
                    p.X - size * 0.5f, p.Y - size * 0.5f, size, size,
                    p.R, p.G, p.B, alpha);
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

            case ParticleType::Flame:
            {
                // Yellow core near the nozzle, swelling into an orange,
                // then red, lick of flame.
                const float radius = 3.0f + age * 9.0f;
                const Uint8 alpha = static_cast<Uint8>(110.0f * (1.0f - age));

                window.DrawGlow(
                    p.X, p.Y, radius,
                    255,
                    static_cast<Uint8>(200.0f * (1.0f - age * 0.75f)),
                    static_cast<Uint8>(70.0f * (1.0f - age)),
                    alpha);

                if (age < 0.6f)
                {
                    const float core = 2.0f + age * 3.0f;

                    window.DrawFilledRect(
                        p.X - core * 0.5f, p.Y - core * 0.5f, core, core,
                        255, static_cast<Uint8>(230 - age * 140.0f), 120,
                        static_cast<Uint8>(230.0f * (1.0f - age)));
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

            case ParticleType::Shockwave:
            {
                // Expanding ring of bright points with a quadratic fade -
                // sells the blast pressure front that the fireball alone
                // doesn't.
                const float radius = 6.0f + age * 175.0f;
                const float fade = (1.0f - age) * (1.0f - age);
                const Uint8 alpha = static_cast<Uint8>(160.0f * fade);

                constexpr int Points = 30;

                for (int i = 0; i < Points; i++)
                {
                    const float angle =
                        static_cast<float>(i) * 6.28318f / Points;

                    window.DrawFilledRect(
                        p.X + std::cos(angle) * radius - 1.5f,
                        p.Y + std::sin(angle) * radius * 0.82f - 1.5f,
                        3.0f, 3.0f,
                        p.R, p.G, p.B, alpha);
                }
                break;
            }

            case ParticleType::Flash:
            {
                // A hard, brief light bloom right at the blast point.
                const float radius = p.Power * (1.0f - age * 0.55f);

                window.DrawGlow(
                    p.X, p.Y, radius,
                    p.R, p.G, p.B,
                    static_cast<Uint8>(115.0f * (1.0f - age)));
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
        m_Ragdolls.clear();
    }

    void ParticleSystem::SpawnRagdoll(const Ragdoll& ragdoll)
    {
        constexpr std::size_t MaxRagdolls = 40;

        if (m_Ragdolls.size() >= MaxRagdolls)
            m_Ragdolls.erase(m_Ragdolls.begin());

        m_Ragdolls.push_back(ragdoll);
    }

    bool ParticleSystem::HitRagdolls(const Particle& p)
    {
        for (Ragdoll& ragdoll : m_Ragdolls)
        {
            float minX, minY, maxX, maxY;
            ragdoll.GetBounds(minX, minY, maxX, maxY);

            if (p.X < minX - 5.0f || p.X > maxX + 5.0f ||
                p.Y < minY - 5.0f || p.Y > maxY + 5.0f)
                continue;

            if (ragdoll.Hit(p.X, p.Y, 6.0f, p.VelX * 0.2f, p.VelY * 0.2f))
            {
                BurstBlood(p.X, p.Y, 4, 110.0f);
                return true;
            }
        }

        return false;
    }

    void ParticleSystem::BlastRagdolls(float x, float y, float radius, float strength)
    {
        for (Ragdoll& ragdoll : m_Ragdolls)
        {
            const float impulse = ragdoll.Push(x, y, radius, strength);

            // Caught in the heart of it: blown to bits.
            if (impulse > strength * 0.7f && !ragdoll.IsExpired())
            {
                const float cx = ragdoll.GetCenterX();
                const float cy = ragdoll.GetCenterY();

                BurstBlood(cx, cy, 24, 240.0f);

                for (int i = 0; i < 7; i++)
                {
                    const float angle = static_cast<float>(i) * 0.9f + RandomUnit();
                    const bool meat = (i % 2) == 0;

                    SpawnGib(cx, cy,
                        std::cos(angle) * (140.0f + RandomUnit() * 60.0f),
                        -160.0f + std::sin(angle) * 120.0f,
                        meat ? 158 : 96, meat ? 24 : 100, meat ? 24 : 80,
                        2 + (i % 2));
                }

                ragdoll.Expire();
            }
        }
    }

    void ParticleSystem::DrawRagdolls(Window& window) const
    {
        for (const Ragdoll& ragdoll : m_Ragdolls)
            ragdoll.Draw(window);
    }

    int ParticleSystem::GetRagdollCount() const
    {
        return static_cast<int>(m_Ragdolls.size());
    }
}
