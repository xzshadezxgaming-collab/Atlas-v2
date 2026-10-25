#pragma once

#include "PlayerInput.h"
#include "SimEvent.h"

#include "../Actors/Actor.h"
#include "../Actors/AIController.h"
#include "../Combat/Delivery.h"
#include "../Combat/Grenade.h"
#include "../Combat/Weapon.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace Atlas
{
    struct WorldConfig
    {
        int Width = 2048;
        int Height = 1024;

        // Fully determines the generated terrain and every random
        // decision the simulation makes. Same seed + same inputs = same
        // game.
        unsigned int Seed = 1337u;

        // Weapon definitions (see LoadWeaponDefs). Empty uses the
        // built-in fallbacks.
        std::vector<WeaponDef> Weapons;
    };

    // A player-controlled body plus the per-player bookkeeping that used
    // to live in the game loop's locals.
    struct PlayerSlot
    {
        std::unique_ptr<Actor> Body;

        int CurrentSlot = 0; // index into the loadout

        float RespawnTimer = 0.0f;
        bool Gibbed = false;

        float GrenadeCooldown = 0.0f;
        float JetSoundTimer = 0.0f;
        float ToolSfxTimer = 0.0f;

        bool WasGrounded = true;
        float PrevFallSpeed = 0.0f;
        bool PrevLegPlanted[2] = { true, true };
    };

    struct Enemy
    {
        std::unique_ptr<Actor> Body;
        AIController Brain;

        // Health seen last tick, to report damage events without
        // plumbing callbacks through every damage path.
        int LastHealth = 100;
    };

    // A purchased reinforcement, once landed: friendly, AI-controlled,
    // fights whatever enemy is nearest.
    struct Ally
    {
        std::unique_ptr<Actor> Body;
        AIController Brain;
    };

    struct HealthPickup
    {
        float X;
        float Y;
        float VelY;
    };

    // A weapon crate ordered without an assigned body: sits where it
    // landed until a player walks over it, then equips (fully loaded).
    struct WeaponPickup
    {
        float X;
        float Y;
        float VelY;
        const WeaponDef* Weapon;
    };

    // The whole game simulation: terrain, actors, projectiles, pickups,
    // waves and deliveries, advanced in fixed steps by Step(). Owns no
    // window, renderer, audio device or input device, so it runs the
    // same in the game client, a headless test, or (later) a dedicated
    // server. Presentation reads state through the const accessors and
    // reacts to the events each Step() reports.
    class World
    {
    public:
        static constexpr float FixedTimeStep = 1.0f / 120.0f;

        static constexpr int MaxPlayers = 4;
        static constexpr int MaxEnemies = 8;
        static constexpr int MaxAllies = 4;
        static constexpr int LoadoutSize = 5;

        static constexpr int SupplyCrateCost = 40;
        static constexpr int ReinforcementCost = 100;

        World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        bool Create(const WorldConfig& config);

        // Adds a player body (team 0) and returns its slot, or -1 when
        // full.
        int AddPlayer();

        // Advances one fixed tick. inputs[i] drives player slot i;
        // missing entries mean "no input". Clears and refills the event
        // list.
        void Step(const PlayerInput* inputs, int inputCount);

        // Spends the player's gold on a drop-ship order. Returns false
        // (and changes nothing) if empty or unaffordable.
        bool SubmitOrder(int player, const OrderRequest& order);

        // What the World would charge for an order.
        int PriceOrder(const OrderRequest& order) const;

        // Turns new enemy waves on/off (existing enemies are unaffected).
        void SetBotsEnabled(bool enabled);
        bool IsBotsEnabled() const;

        const std::vector<SimEvent>& GetEvents() const;

        // Order-independent-of-memory digest of the simulation state, for
        // determinism tests and (later) network desync detection.
        std::uint64_t ComputeStateHash() const;

        // Resolves a stable actor id; null if it no longer exists.
        Actor* FindActor(std::uint32_t id);

        // --- State access (presentation, tests) ---

        Terrain& GetTerrain();
        const Terrain& GetTerrain() const;
        const ParticleSystem& GetParticles() const;
        const GrenadeSystem& GetGrenades() const;
        const DeliverySystem& GetDeliveries() const;

        int GetPlayerCount() const;
        PlayerSlot& GetPlayer(int slot);
        const PlayerSlot& GetPlayer(int slot) const;

        const std::vector<Enemy>& GetEnemies() const;
        std::vector<Enemy>& GetEnemies();
        const std::vector<Ally>& GetAllies() const;
        std::vector<Ally>& GetAllies();
        const std::vector<HealthPickup>& GetHealthPickups() const;
        const std::vector<WeaponPickup>& GetWeaponPickups() const;

        const std::vector<WeaponDef>& GetWeaponDefs() const;
        const WeaponDef* GetLoadoutWeapon(int slot) const;
        const WeaponDef* GetAllyWeapon() const;
        const WeaponDef* FindWeapon(const std::string& name) const;

        int GetWave() const;
        std::uint64_t GetTick() const;
        float GetSpawnX(int playerSlot) const;

    private:
        // Uniform in [-1, 1). Hand-rolled on top of mt19937 (whose output
        // the standard pins down) because std distributions may differ
        // between standard libraries - and peers must agree.
        float RandomUnit();
        float RandomRange(float min, float max);

        void Emit(const SimEvent& event);
        void EmitSound(Sfx sound, float volume = 1.0f);

        void InitActor(Actor& actor, int team);
        void SpawnEnemy(float x);
        void SpawnWave(int count);
        void SpawnAlly(float x, const WeaponDef* weapon);

        void StepPlayer(int slot, const PlayerInput& input);
        void ApplyDiggerLimbDamage(Actor& digger);
        Actor* NearestLivingPlayer(float x, float y);

        Terrain m_Terrain;
        ParticleSystem m_Particles;
        GrenadeSystem m_Grenades;
        DeliverySystem m_Deliveries;

        std::vector<PlayerSlot> m_Players;
        std::vector<Enemy> m_Enemies;
        std::vector<Ally> m_Allies;
        std::vector<HealthPickup> m_HealthPickups;
        std::vector<WeaponPickup> m_WeaponPickups;

        std::vector<WeaponDef> m_WeaponDefs;
        const WeaponDef* m_Loadout[LoadoutSize];
        const WeaponDef* m_EnemyWeapon;
        const WeaponDef* m_AllyWeapon;

        std::vector<SimEvent> m_Events;
        std::vector<GoldCredit> m_GoldCredits;

        std::mt19937 m_Rng;

        std::uint32_t m_NextActorId;
        std::uint64_t m_Tick;

        int m_Wave;
        float m_WaveTimer;
        bool m_BotsEnabled;
    };
}
