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
    enum class GameMode : std::uint8_t
    {
        // Each side has a brain in a concrete bunker; destroy theirs,
        // keep yours. Enemy reinforcements drop in near their base and
        // march on yours.
        BrainAssault,

        // Endless waves of enemies, no bases.
        Survival,
    };

    // What a team's AI units do when nobody is driving them.
    enum class AllyOrder : std::uint8_t
    {
        Defend, // hold around the team's brain
        Follow, // stay with the player who gave the order
        Attack, // march on the enemy brain

        Count
    };

    enum class MatchOutcome : std::uint8_t
    {
        InProgress,
        Victory, // the players destroyed the enemy brain
        Defeat,  // the players' brain was destroyed
    };

    // A team's base: a concrete bunker with the team's brain inside, in
    // a glass jar on a pedestal. The brain is made of terrain pixels, so
    // anything that chews terrain can kill it.
    struct TeamBase
    {
        bool Exists = false;

        float X = 0.0f;  // bunker center
        int FloorY = 0;  // top of the bunker floor

        // Bounding box of the brain's pixels.
        int BrainMinX = 0;
        int BrainMinY = 0;
        int BrainMaxX = 0;
        int BrainMaxY = 0;

        int BrainPixelsStart = 0;
        int BrainPixels = 0;
        bool BrainAlive = false;

        float GetBrainCenterX() const { return 0.5f * static_cast<float>(BrainMinX + BrainMaxX); }
        float GetBrainCenterY() const { return 0.5f * static_cast<float>(BrainMinY + BrainMaxY); }

        // 0..1 of the brain left; it dies well before every pixel is gone.
        float GetBrainHealth() const;
    };

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

        GameMode Mode = GameMode::BrainAssault;

        // 0 easy, 1 normal, 2 hard: how hard and how often the enemy
        // drops come, and how much gold you start with.
        int Difficulty = 1;

        // Where the players' bunker goes, in percent of the map width
        // from the west edge (BrainAssault). Clamped to 6..42.
        int LandingZone = 12;
    };

    // Running totals for the end-of-match screen (the players' side).
    struct MatchStats
    {
        int EnemiesKilled = 0;
        int BodiesLost = 0;     // player bodies and AI units
        int GoldMined = 0;
        int BodiesOrdered = 0;
        std::uint64_t EndTick = 0; // tick the match was decided, 0 = still on
    };

    // A player-controlled body plus the per-player bookkeeping that used
    // to live in the game loop's locals.
    struct PlayerSlot
    {
        std::unique_ptr<Actor> Body;

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

        // Attackers march on the players' brain; the rest guard their own.
        bool Attacker = true;

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

    // A small flying gun platform. Hovers above the ground, follows its
    // side's orders and shoots what it can see. Not an Actor: bullets
    // and blasts reach it through hit circles.
    struct Drone
    {
        std::uint32_t Id = 0;
        int Team = 0;

        float X = 0.0f;
        float Y = 0.0f;
        float VelX = 0.0f;
        float VelY = 0.0f;

        int Health = 0;
        float Cooldown = 0.0f;
        float BurstTimer = 0.0f;
        float PauseTimer = 0.0f;
        float HurtFlash = 0.0f;

        // Where its gun points (for drawing).
        float AimX = 1.0f;
        float AimY = 0.0f;
        bool Firing = false;

        float Phase = 0.0f; // rotor and bob animation
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
        static constexpr int MaxEnemies = 12;
        static constexpr int MaxAllies = 8;
        static constexpr int MaxDrones = 10;
        static constexpr int MaxDronesPerTeam = 5;
        static constexpr int TeamCount = 2;
        static constexpr int LoadoutSize = 5;

        static constexpr int SupplyCrateCost = 40;
        static constexpr int ReinforcementCost = 100;
        static constexpr int DroneCost = 80;
        static constexpr int DroneHealth = 60;
        static constexpr int StartingGold = 300;
        static constexpr int KillBounty = 10;

        // Below this share of its pixels, a brain is dead.
        static constexpr float BrainDeathThreshold = 0.35f;

        World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        bool Create(const WorldConfig& config);

        // Adds a player body (team 0, tinted by slot) and returns its
        // slot, or -1 when full.
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

        // Lands an AI unit for the players' side at x (menu backdrops,
        // scenarios). weapon null = the default rifleman.
        void AddAlly(float x, const WeaponDef* weapon = nullptr,
            BodyClass bodyClass = BodyClass::Medium);

        // Puts a drone in the air at (x, y) for a team.
        void AddDrone(float x, float y, int team);

        void SetAllyOrder(AllyOrder order);

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
        const std::vector<Drone>& GetDrones() const;
        const std::vector<HealthPickup>& GetHealthPickups() const;
        const std::vector<WeaponPickup>& GetWeaponPickups() const;

        const std::vector<WeaponDef>& GetWeaponDefs() const;
        const WeaponDef* GetLoadoutWeapon(int slot) const;
        const WeaponDef* GetAllyWeapon() const;
        const WeaponDef* FindWeapon(const std::string& name) const;

        // Weapons by position in GetWeaponDefs(), for the wire.
        // -1 / out of range <-> null.
        int GetWeaponIndex(const WeaponDef* def) const;
        const WeaponDef* GetWeaponByIndex(int index) const;

        int GetWave() const;
        std::uint64_t GetTick() const;
        float GetSpawnX(int playerSlot) const;

        GameMode GetMode() const;
        MatchOutcome GetOutcome() const;
        const TeamBase& GetBase(int team) const;

        // Seconds until the next enemy drop (BrainAssault), or the next
        // wave (Survival).
        float GetWaveTimer() const;

        // Gold is pooled per team: whatever a body mines goes to its
        // team, and orders draw on the team's funds.
        int GetTeamGold(int team) const;
        void AddTeamGold(int team, int amount);

        AllyOrder GetAllyOrder() const;
        const MatchStats& GetStats() const;
        int GetDifficulty() const;

    private:
        // Uniform in [-1, 1). Hand-rolled on top of mt19937 (whose output
        // the standard pins down) because std distributions may differ
        // between standard libraries - and peers must agree.
        float RandomUnit();
        float RandomRange(float min, float max);

        void Emit(const SimEvent& event);
        void EmitSound(Sfx sound, float volume = 1.0f);

        // A sound from somewhere in the world: the client fades and pans
        // it by where it is relative to the view.
        void EmitSoundAt(Sfx sound, float volume, float x, float y);

        void InitActor(Actor& actor, int team);
        void SpawnEnemy(float x, bool attacker,
            BodyClass bodyClass = BodyClass::Medium, const WeaponDef* weapon = nullptr);
        void SpawnDrone(float x, float y, int team);
        void UpdateDrones();
        void DamageDrones(Actor* const* actors, int actorCount);
        void FlushLaunch(Actor& actor);
        void AiShotSound(Actor& actor);
        void GiveStarterKit(Actor& body);
        void BuildBase(int team, float centerX);
        void UpdateBrains();
        void SwitchBody(int slot, bool deadBodyOnly);
        void RespawnPlayer(int slot);
        // Buys and ships up to maxCount bodies (and drones) with the
        // enemy's gold. Returns how many were shipped.
        int SpawnEnemyDrop(int maxCount);
        int AllyIndexNearest(float x, float y) const;
        void SpawnWave(int count);
        void SpawnAlly(float x, const WeaponDef* weapon, BodyClass bodyClass);

        void StepPlayer(int slot, const PlayerInput& input);
        void ApplyTerrainHazards(Actor& actor);
        void ApplyDiggerLimbDamage(Actor& digger);
        Actor* NearestLivingPlayer(float x, float y);
        Actor* NearestOfTeam(int team, float x, float y);
        void CollectGold(Actor& body);

        Terrain m_Terrain;
        ParticleSystem m_Particles;
        GrenadeSystem m_Grenades;
        DeliverySystem m_Deliveries;

        std::vector<PlayerSlot> m_Players;
        std::vector<Enemy> m_Enemies;
        std::vector<Ally> m_Allies;
        std::vector<Drone> m_Drones;
        std::vector<HealthPickup> m_HealthPickups;
        std::vector<WeaponPickup> m_WeaponPickups;

        std::vector<WeaponDef> m_WeaponDefs;
        const WeaponDef* m_Loadout[LoadoutSize];
        const WeaponDef* m_EnemyWeapon;
        const WeaponDef* m_AllyWeapon;
        const WeaponDef* m_Shovel;
        const WeaponDef* m_Rocket;
        const WeaponDef* m_Flamer;
        const WeaponDef* m_Shotgun;
        const WeaponDef* m_Digger;

        std::vector<SimEvent> m_Events;
        std::vector<GoldCredit> m_GoldCredits;

        std::mt19937 m_Rng;

        std::uint32_t m_NextActorId;
        std::uint64_t m_Tick;

        float m_SteamSoundTimer;

        int m_Wave;
        float m_WaveTimer;
        bool m_BotsEnabled;

        GameMode m_Mode;
        MatchOutcome m_Outcome;
        TeamBase m_Bases[TeamCount];
        int m_TeamGold[TeamCount];
        float m_BrainAlarmTimer;

        int m_Difficulty;
        MatchStats m_Stats;

        AllyOrder m_AllyOrder;
        int m_AllyOrderPlayer; // who gave a Follow order
    };
}
