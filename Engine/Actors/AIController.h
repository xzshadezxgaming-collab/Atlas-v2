#pragma once

#include <cstdint>

namespace Atlas
{
    class Actor;
    class ParticleSystem;
    class Terrain;

    // What a unit's commander wants from it this tick.
    struct AIOrders
    {
        // The nearest hostile body, if any. Fought on sight.
        Actor* Target = nullptr;

        // A fixed thing to shoot at once in view (an enemy brain).
        bool HasPoint = false;
        float PointX = 0.0f;
        float PointY = 0.0f;

        // Where to head when there's nothing to fight. Without a goal
        // the unit wanders.
        bool HasGoal = false;
        float GoalX = 0.0f;
    };

    // Soldier brain: heads for its goal (jetting over whatever blocks
    // the way), and on spotting a target stops, aims (with wobble) and
    // fires in bursts.
    class AIController
    {
    public:
        AIController();

        // Per-unit randomness, so a squad doesn't move in lockstep.
        void Seed(std::uint32_t seed);

        void Update(
            Actor& self,
            const AIOrders& orders,
            const Terrain& terrain,
            ParticleSystem& particles,
            Terrain& mutableTerrain,
            float deltaTime);

    private:
        bool HasLineOfSight(
            const Actor& self,
            float targetX,
            float targetY,
            const Terrain& terrain) const;

        // Aims at (x, y) and runs the burst rhythm.
        void Engage(
            Actor& self,
            float x,
            float y,
            ParticleSystem& particles,
            Terrain& mutableTerrain,
            float deltaTime);

        float m_WanderDir;
        float m_WanderTimer;
        float m_BurstTimer;   // > 0: firing
        float m_PauseTimer;   // > 0: holding fire
        float m_AimWobble;
        float m_WobbleTimer;

        float m_StuckTimer;   // how long progress toward the goal has stalled
        float m_JetTimer;     // > 0: jetting to get over an obstacle

        std::uint32_t m_RandomState;

        float RandomUnit();
    };
}
