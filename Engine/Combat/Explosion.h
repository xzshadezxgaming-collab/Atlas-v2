#pragma once

namespace Atlas
{
    class Actor;
    class ParticleSystem;
    class Terrain;

    // Carves a crater, sprays debris and smoke, and applies distance-based
    // damage and knockback to actors.
    void Explode(
        Terrain& terrain,
        ParticleSystem& particles,
        Actor* const* actors,
        int actorCount,
        float x,
        float y,
        float radius,
        int maxDamage);
}
