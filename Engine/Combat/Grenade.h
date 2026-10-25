#pragma once

#include <vector>

namespace Atlas
{
    class Actor;
    class ParticleSystem;
    class Terrain;
    class Window;

    struct Grenade
    {
        float X;
        float Y;
        float VelX;
        float VelY;
        float Fuse;  // seconds until detonation
        float Trail; // seconds until the next smoke-trail puff
        int Team;    // thrower's team, -1 = nobody's

        // Rockets fly nearly straight and go off on contact.
        bool Rocket = false;
        float Radius = 0.0f;
        int Damage = 0;
        unsigned int OwnerId = 0;
    };

    // An explosion that happened during the last Update, for anything
    // the blast should reach that isn't an Actor (drones).
    struct Blast
    {
        float X;
        float Y;
        float Radius;
        int Damage;
    };

    class GrenadeSystem
    {
    public:
        void Throw(float x, float y, float velX, float velY, int team = -1, float fuse = 2.2f);

        void Launch(
            float x,
            float y,
            float velX,
            float velY,
            int team,
            unsigned int ownerId,
            float radius,
            int damage);

        void Update(
            Terrain& terrain,
            ParticleSystem& particles,
            Actor* const* actors,
            int actorCount,
            float deltaTime);

        void Draw(Window& window) const;

        int GetActiveCount() const;
        void Clear();

        // Set when any grenade detonated during the last Update (for sound).
        bool ExplodedThisFrame() const;

        const std::vector<Blast>& GetBlasts() const;

        // Extra things a rocket detonates against (drones).
        void SetProximityTargets(const std::vector<Blast>& targets);

        int GetRocketCount() const;
        const std::vector<Grenade>& GetAll() const;

    private:
        std::vector<Grenade> m_Grenades;
        std::vector<Blast> m_Blasts;
        std::vector<Blast> m_Proximity;
        bool m_ExplodedThisFrame = false;
    };
}
