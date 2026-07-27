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
        // (a shovel can't dig stone).
        float MaxDigStrength = 100.0f;
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

    private:
        const WeaponDef* m_Def;

        float m_Cooldown;
        float m_ReloadTimer;
        int m_Ammo;

        std::uint32_t m_RandomState;

        float RandomUnit();
    };
}
