#pragma once

#include "../World/Material.h"

#include <cstdint>
#include <vector>

namespace Atlas
{
    class Actor;
    class Terrain;
    class Window;

    enum class ParticleType : std::uint8_t
    {
        Bullet,   // damages terrain (energy vs material strength) and actors
        Debris,   // knocked-loose terrain; settles back into the world
        Blood,    // stains the terrain where it lands
        Spark,    // glowing ember, dies on contact
        Smoke,    // drifts up, grows and fades, ignores terrain
        Gib,      // chunk of a dead actor; settles as a stain + debris
        Fire,     // additive fireball puff (explosions), ignores terrain
        Casing,   // ejected brass, bounces off terrain then fades
    };

    struct Particle
    {
        float X;
        float Y;
        float VelX;
        float VelY;

        float Life;        // seconds remaining
        float MaxLife;     // starting life, for fade curves
        float Gravity;     // pixels/s^2
        std::uint8_t Bounces; // casings: bounces remaining

        float Power;       // bullets: terrain-destruction budget
        std::int16_t Damage;

        std::uint8_t R;
        std::uint8_t G;
        std::uint8_t B;

        std::uint8_t Size; // drawn as Size x Size pixels
        ParticleType Type;
        Material SettleMaterial;

        const Actor* Owner; // never collides with its owner
    };

    // One pool for every moving pixel in the game: bullets, debris, blood,
    // sparks, smoke and gibs. Movement steps at most one pixel at a time
    // (DDA) so fast particles never tunnel through thin terrain.
    class ParticleSystem
    {
    public:
        ParticleSystem();

        void SpawnBullet(
            float x,
            float y,
            float velX,
            float velY,
            int damage,
            float power,
            const Actor* owner);

        void SpawnDebris(float x, float y, float velX, float velY, Material material);
        void SpawnBlood(float x, float y, float velX, float velY);
        void SpawnSpark(float x, float y, float velX, float velY);
        void SpawnSmoke(float x, float y, float velX, float velY);
        void SpawnFire(float x, float y, float velX, float velY);
        void SpawnCasing(float x, float y, float velX, float velY);
        void SpawnGib(float x, float y, float velX, float velY,
            std::uint8_t r, std::uint8_t g, std::uint8_t b, int size);

        // Convenience bursts.
        void BurstBlood(float x, float y, int count, float speed);
        void BurstDebris(float x, float y, int count, float speed, Material material);

        void Update(
            Terrain& terrain,
            float deltaTime,
            Actor* const* actors,
            int actorCount);

        void Draw(Window& window) const;

        int GetActiveCount() const;
        void Clear();

    private:
        void Push(const Particle& particle);

        // Deterministic-ish cheap RNG for spread/bursts.
        float RandomUnit();

        std::vector<Particle> m_Particles;
        std::uint32_t m_RandomState;
    };
}
