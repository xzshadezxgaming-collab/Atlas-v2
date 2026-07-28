#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Atlas
{
    class Actor;
    class ParticleSystem;
    class Terrain;

    enum class WeaponKind
    {
        Gun,
        Digger, // energy tool: carves terrain at the aim point
        Shovel, // digs bigger scoops at close range
    };

    struct WeaponDef
    {
        std::string Name = "Gun";
        WeaponKind Kind = WeaponKind::Gun;

        float FireRate = 6.0f;       // shots per second
        int ClipSize = 20;           // 0 = infinite
        float ReloadTime = 1.8f;

        float MuzzleVelocity = 900.0f;
        float SpreadDegrees = 3.0f;
        int Damage = 10;
        float Power = 8.0f;          // terrain penetration budget
        int Pellets = 1;

        float Recoil = 60.0f;        // impulse against the shooter
        float BarrelLength = 14.0f;

        float DigRadius = 9.0f;      // dig tools: bite size
        float DigRange = 110.0f;     // dig tools: reach

        // Dig tools erode rather than delete: each tick can remove
        // material totalling this much strength (dirt is cheap, stone is
        // expensive), so hard rock digs visibly slower.
        float DigPower = 30.0f;

        // Material strength above this can't be dug at all by this tool
        // (a shovel can't dig stone or gold).
        float MaxDigStrength = 100.0f;

        // Multiplies a material's effective strength cost against this
        // tool's DigPower budget - below 1.0 means the tool is cheaper
        // (faster) against that material category, above 1.0 means
        // slower. Only affects speed, not the MaxDigStrength cutoff:
        // a tool that can't touch a material at all stays that way
        // regardless of these multipliers.
        float HardMaterialCost = 1.0f; // applies to Stone and Gold
        float SoftMaterialCost = 1.0f; // applies to Grass and Dirt

        // The tool's effective direction smoothly sweeps back and forth
        // within a cone this many degrees wide, centered on the aim
        // direction, instead of staying rigidly straight (0 = no sweep).
        float ConeAngleDegrees = 0.0f;
        float ConeSweepSpeed = 2.0f; // sweep oscillations per second-ish

        // Dig tools only: damage dealt per fire tick to another actor's
        // limb the tool's impact point is touching (0 = can't hurt
        // actors, only terrain).
        int LimbDamage = 0;

        // Gold cost to order this weapon from the buy menu (equipped to
        // a reinforcement, or as a loose field pickup). Weapons that
        // aren't meant to be purchasable (EnemyGun) just go unused.
        int Cost = 20;
    };

    // Loads weapon definitions from an INI file; returns built-in defaults
    // if the file is missing.
    std::vector<WeaponDef> LoadWeaponDefs(const std::string& filename);
    std::vector<WeaponDef> BuiltInWeaponDefs();

    // An instance of a weapon held by an actor: ammo, cooldown, reload.
    class Weapon
    {
    public:
        Weapon();

        void SetDef(const WeaponDef* def);
        const WeaponDef* GetDef() const;

        void Update(float deltaTime);

        // Attempts to fire from the muzzle toward (dirX, dirY) (normalized).
        // Returns true if a shot happened. For diggers, carves terrain at
        // the aim point instead.
        bool TryFire(
            ParticleSystem& particles,
            Terrain& terrain,
            float muzzleX,
            float muzzleY,
            float dirX,
            float dirY,
            float targetX,
            float targetY,
            Actor* owner);

        void StartReload();
        bool IsReloading() const;
        float GetReloadProgress() const;

        int GetAmmo() const;
        int GetClipSize() const;

        // Instantly tops the clip back up and clears any reload in
        // progress (a supply-crate resupply, not a timed field reload).
        void Refill();

        // Applies this weapon's cone-sweep (if any) to the given aim
        // direction, returning the tool's actual effective direction for
        // this instant. With ConeAngleDegrees == 0 this just returns
        // (aimDirX, aimDirY) unchanged. Shared by the real dig raycast and
        // the beam preview so they can never disagree.
        void GetSweptDirection(
            float aimDirX,
            float aimDirY,
            float& outDirX,
            float& outDirY) const;

    private:
        const WeaponDef* m_Def;

        float m_Cooldown;
        float m_ReloadTimer;
        int m_Ammo;

        float m_SweepPhase;

        std::uint32_t m_RandomState;

        float RandomUnit();
    };
}
