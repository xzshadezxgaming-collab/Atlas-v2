#pragma once

#include "../Actors/Ragdoll.h"
#include "../World/Material.h"
#include "../World/Terrain.h"

#include <cstdint>
#include <vector>

namespace Atlas
{
    class Actor;
    class Terrain;
    class Window;

    enum class ParticleType : std::uint8_t
    {
        Bullet,    // damages terrain (energy vs material strength) and actors
        Debris,    // knocked-loose terrain; settles back into the world
        Blood,     // stains the terrain where it lands
        Spark,     // glowing ember, dies on contact
        Smoke,     // drifts up, grows and fades, ignores terrain
        Gib,       // chunk of a dead actor; settles as a stain + debris
        Fire,      // additive fireball puff (explosions), ignores terrain
        Casing,    // ejected brass, bounces off terrain then fades
        Dust,      // soft tan puff (footsteps, landings), ignores terrain
        Shockwave, // expanding blast ring (explosions), ignores terrain
        Flash,     // brief bright light bloom at a point, ignores terrain
        Flame,     // burning fuel: hurts and ignites actors, burns grass
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

        // Who fired it, by stable actor id rather than pointer: the
        // shooter may die and be removed while the round is in flight.
        // Never hits its owner or the owner's team; gold it mines is
        // credited to the owner. 0 / -1 = no owner.
        std::uint32_t OwnerId;
        std::int8_t OwnerTeam;
    };

    // Gold mined by a bullet, credited to its owner by the caller (who
    // can resolve the id - the owner may no longer exist).
    struct GoldCredit
    {
        std::uint32_t OwnerId;
        int Amount;
    };

    // Something round that bullets and flames can hit besides actors
    // (drones). Set before each Update; hits are reported, not applied.
    struct HitCircle
    {
        float X;
        float Y;
        float Radius;
        std::uint32_t Id;
        int Team;
    };

    struct CircleHit
    {
        std::uint32_t Id;
        int Damage;
        bool Fire;
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
            Actor* owner);

        // Same, for shooters that aren't actors (drones).
        void SpawnBullet(
            float x,
            float y,
            float velX,
            float velY,
            int damage,
            float power,
            std::uint32_t ownerId,
            int ownerTeam);

        void SpawnFlame(
            float x,
            float y,
            float velX,
            float velY,
            int damage,
            std::uint32_t ownerId,
            int ownerTeam);

        void SpawnDebris(float x, float y, float velX, float velY, Material material);
        void SpawnBlood(float x, float y, float velX, float velY);
        void SpawnSpark(float x, float y, float velX, float velY);
        void SpawnSmoke(float x, float y, float velX, float velY);
        void SpawnFire(float x, float y, float velX, float velY);
        void SpawnCasing(float x, float y, float velX, float velY);
        void SpawnDust(
            float x,
            float y,
            float velX,
            float velY,
            std::uint8_t r = 172,
            std::uint8_t g = 150,
            std::uint8_t b = 118);
        void SpawnShockwave(float x, float y);
        void SpawnFlash(float x, float y, float radius);
        void SpawnGib(float x, float y, float velX, float velY,
            std::uint8_t r, std::uint8_t g, std::uint8_t b, int size);

        // Corpses and severed limbs (see Ragdoll). The oldest goes when
        // there are too many.
        void SpawnRagdoll(const Ragdoll& ragdoll);

        // A blast throws nearby ragdolls around; one caught right in it
        // is torn apart into gibs.
        void BlastRagdolls(float x, float y, float radius, float strength);

        // Drawn behind the living, so call before the actors.
        void DrawRagdolls(Window& window) const;
        int GetRagdollCount() const;

        // Convenience bursts.
        void BurstBlood(float x, float y, int count, float speed);
        void BurstDebris(float x, float y, int count, float speed, Material material);

        void Update(
            Terrain& terrain,
            float deltaTime,
            Actor* const* actors,
            int actorCount,
            std::vector<GoldCredit>* outGold = nullptr);

        void Draw(Window& window) const;

        void SetHitCircles(const std::vector<HitCircle>& circles);
        const std::vector<CircleHit>& GetCircleHits() const;

        int GetActiveCount() const;
        void Clear();

        // A team's own bullets pass harmlessly off brain pixels inside
        // this box (its brain), so nobody shoots their own brain by
        // accident.
        void SetFriendlyBrainBox(int team, int minX, int minY, int maxX, int maxY);
        bool GetFriendlyBrainBox(int team, Terrain::Box& out) const;

    private:
        struct BrainBox
        {
            bool Set = false;
            int MinX = 0;
            int MinY = 0;
            int MaxX = 0;
            int MaxY = 0;
        };

        BrainBox m_BrainBoxes[2];

        bool IsFriendlyBrain(int team, int x, int y) const;

        void Push(const Particle& particle);

        // Deterministic-ish cheap RNG for spread/bursts.
        float RandomUnit();

        // Tests p against the hit circles; records a hit and returns true.
        bool HitTestCircles(const Particle& p);

        // Bullets hitting a ragdoll: knocks it, splashes blood, stops.
        bool HitRagdolls(const Particle& p);

        std::vector<Particle> m_Particles;
        std::vector<Ragdoll> m_Ragdolls;
        std::vector<HitCircle> m_Circles;
        std::vector<CircleHit> m_CircleHits;
        std::uint32_t m_RandomState;
    };
}
