#include "Explosion.h"

#include "../Actors/Actor.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <cmath>
#include <vector>

namespace Atlas
{
    void Explode(
        Terrain& terrain,
        ParticleSystem& particles,
        Actor* const* actors,
        int actorCount,
        float x,
        float y,
        float radius,
        int maxDamage,
        int team)
    {
        // Carve the crater, collecting some of the destroyed pixels.
        Terrain::Box spare;
        const bool hasSpare = particles.GetFriendlyBrainBox(team, spare);

        std::vector<Terrain::DestroyedPixel> debris;
        terrain.CarveCircleCollect(x, y, radius, debris, 40, hasSpare ? &spare : nullptr);

        // Debris flies outward from the blast center.
        for (const Terrain::DestroyedPixel& pixel : debris)
        {
            float dx = pixel.X - x;
            float dy = pixel.Y - y;
            const float distance = std::sqrt(dx * dx + dy * dy) + 0.001f;

            dx /= distance;
            dy /= distance;

            const float speed = 180.0f + (radius - distance) * 6.0f;

            particles.SpawnDebris(
                pixel.X,
                pixel.Y,
                dx * speed,
                dy * speed - 120.0f,
                pixel.Mat);
        }

        // Pressure front and a hard light pop at the blast point.
        particles.SpawnShockwave(x, y);
        particles.SpawnFlash(x, y, radius * 1.7f);

        // Fireball puffs.
        for (int i = 0; i < 16; i++)
        {
            const float angle = static_cast<float>(i) * 0.3927f;
            const float speed = 30.0f + (i % 4) * 26.0f;

            particles.SpawnFire(
                x + std::cos(angle) * 6.0f,
                y + std::sin(angle) * 6.0f,
                std::cos(angle) * speed,
                std::sin(angle) * speed - 30.0f);
        }

        // Scorch the crater rim.
        for (int i = 0; i < 64; i++)
        {
            const float angle = static_cast<float>(i) * 0.0982f;
            const float ringRadius = radius + 1.0f + (i % 3);

            terrain.StainPixel(
                static_cast<int>(x + std::cos(angle) * ringRadius),
                static_cast<int>(y + std::sin(angle) * ringRadius),
                34, 28, 24);
        }

        // Flash sparks and a smoke plume.
        for (int i = 0; i < 14; i++)
        {
            const float angle = static_cast<float>(i) * 0.4488f;

            particles.SpawnSpark(
                x,
                y,
                std::cos(angle) * (220.0f + (i % 3) * 90.0f),
                std::sin(angle) * (220.0f + (i % 3) * 90.0f) - 80.0f);
        }

        for (int i = 0; i < 10; i++)
        {
            const float angle = static_cast<float>(i) * 0.6283f;

            particles.SpawnSmoke(
                x,
                y,
                std::cos(angle) * 60.0f,
                std::sin(angle) * 40.0f - 60.0f);
        }

        // Damage and knock back actors by distance. Each limb takes its own
        // share too, so a close blast can tear arms and legs off.
        const float blastReach = radius * 2.2f;

        for (int i = 0; i < actorCount; i++)
        {
            Actor* actor = actors[i];

            if (!actor || !actor->IsAlive())
                continue;

            actor->TakeBlast(x, y, blastReach, maxDamage, &particles);
        }

        // Fling the dead about: corpses and loose limbs caught close enough
        // burst apart.
        particles.BlastRagdolls(x, y, radius * 2.5f,
            420.0f + static_cast<float>(maxDamage) * 4.0f);
    }
}
