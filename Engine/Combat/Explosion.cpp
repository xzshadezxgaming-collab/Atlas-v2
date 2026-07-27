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
        int maxDamage)
    {
        // Carve the crater, collecting some of the destroyed pixels.
        std::vector<Terrain::DestroyedPixel> debris;
        terrain.CarveCircleCollect(x, y, radius, debris, 40);

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

        // Damage and knock back actors by distance.
        for (int i = 0; i < actorCount; i++)
        {
            Actor* actor = actors[i];

            if (!actor || !actor->IsAlive())
                continue;

            const float actorX = actor->GetCenterX();
            const float actorY = actor->GetCenterY();

            float dx = actorX - x;
            float dy = actorY - y;
            const float distance = std::sqrt(dx * dx + dy * dy);

            const float blastReach = radius * 2.2f;

            if (distance > blastReach)
                continue;

            const float falloff = 1.0f - distance / blastReach;

            dx /= distance + 0.001f;
            dy /= distance + 0.001f;

            actor->TakeDamage(
                static_cast<int>(static_cast<float>(maxDamage) * falloff),
                dx * 420.0f * falloff,
                dy * 420.0f * falloff - 140.0f * falloff);
        }
    }
}
