#pragma once

namespace Atlas
{
    class Actor;
    class ParticleSystem;
    class Terrain;

    // Simple soldier brain: wanders until it has line of sight to the
    // target, then stops, aims (with wobble) and fires in bursts.
    class AIController
    {
    public:
        AIController();

        void Update(
            Actor& self,
            Actor* target,
            const Terrain& terrain,
            ParticleSystem& particles,
            Terrain& mutableTerrain,
            float deltaTime);

    private:
        bool HasLineOfSight(
            const Actor& self,
            const Actor& target,
            const Terrain& terrain) const;

        float m_WanderDir;
        float m_WanderTimer;
        float m_BurstTimer;   // > 0: firing
        float m_PauseTimer;   // > 0: holding fire
        float m_AimWobble;
        float m_WobbleTimer;

        unsigned int m_RandomState;

        float RandomUnit();
    };
}
