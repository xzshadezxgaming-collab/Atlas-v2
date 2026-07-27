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
        float Fuse; // seconds until detonation
    };

    class GrenadeSystem
    {
    public:
        void Throw(float x, float y, float velX, float velY, float fuse = 2.2f);

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

    private:
        std::vector<Grenade> m_Grenades;
        bool m_ExplodedThisFrame = false;
    };
}
