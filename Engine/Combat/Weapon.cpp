#include "Weapon.h"

#include "../Actors/Actor.h"
#include "../Core/IniFile.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <cmath>
#include <cstdint>

namespace Atlas
{
    std::vector<WeaponDef> BuiltInWeaponDefs()
    {
        std::vector<WeaponDef> defs;

        {
            WeaponDef smg;
            smg.Name = "SMG";
            smg.FireRate = 9.0f;
            smg.ClipSize = 30;
            smg.ReloadTime = 1.6f;
            smg.MuzzleVelocity = 900.0f;
            smg.SpreadDegrees = 4.0f;
            smg.Damage = 9;
            smg.Power = 7.0f;
            smg.Pellets = 1;
            smg.Recoil = 40.0f;
            smg.BarrelLength = 14.0f;
            defs.push_back(smg);
        }

        {
            WeaponDef shotgun;
            shotgun.Name = "Shotgun";
            shotgun.FireRate = 1.4f;
            shotgun.ClipSize = 6;
            shotgun.ReloadTime = 2.2f;
            shotgun.MuzzleVelocity = 750.0f;
            shotgun.SpreadDegrees = 9.0f;
            shotgun.Damage = 7;
            shotgun.Power = 5.0f;
            shotgun.Pellets = 7;
            shotgun.Recoil = 170.0f;
            shotgun.BarrelLength = 16.0f;
            defs.push_back(shotgun);
        }

        {
            WeaponDef rifle;
            rifle.Name = "Rifle";
            rifle.FireRate = 2.5f;
            rifle.ClipSize = 8;
            rifle.ReloadTime = 1.8f;
            rifle.MuzzleVelocity = 1400.0f;
            rifle.SpreadDegrees = 0.7f;
            rifle.Damage = 30;
            rifle.Power = 22.0f;
            rifle.Pellets = 1;
            rifle.Recoil = 90.0f;
            rifle.BarrelLength = 20.0f;
            defs.push_back(rifle);
        }

        {
            // Continuous grinder: fast small bites, chews stone slowly.
            WeaponDef digger;
            digger.Name = "Digger";
            digger.Kind = WeaponKind::Digger;
            digger.FireRate = 24.0f;
            digger.ClipSize = 0;
            digger.DigRadius = 7.5f;
            digger.DigRange = 115.0f;
            digger.DigPower = 52.0f;
            digger.BarrelLength = 12.0f;
            digger.Recoil = 0.0f;
            defs.push_back(digger);
        }

        {
            // Big slow scoops of soft ground; useless against stone.
            WeaponDef shovel;
            shovel.Name = "Shovel";
            shovel.Kind = WeaponKind::Shovel;
            shovel.FireRate = 2.0f;
            shovel.ClipSize = 0;
            shovel.DigRadius = 15.0f;
            shovel.DigRange = 58.0f;
            shovel.DigPower = 420.0f;
            shovel.MaxDigStrength = 5.0f;
            shovel.BarrelLength = 17.0f;
            shovel.Recoil = 0.0f;
            defs.push_back(shovel);
        }

        {
            WeaponDef enemyGun;
            enemyGun.Name = "EnemyGun";
            enemyGun.FireRate = 5.0f;
            enemyGun.ClipSize = 0;
            enemyGun.MuzzleVelocity = 780.0f;
            enemyGun.SpreadDegrees = 6.0f;
            enemyGun.Damage = 6;
            enemyGun.Power = 5.0f;
            enemyGun.Pellets = 1;
            enemyGun.Recoil = 30.0f;
            enemyGun.BarrelLength = 14.0f;
            defs.push_back(enemyGun);
        }

        return defs;
    }

    std::vector<WeaponDef> LoadWeaponDefs(const std::string& filename)
    {
        std::vector<WeaponDef> defs = BuiltInWeaponDefs();

        IniFile ini;

        if (!ini.Load(filename))
            return defs;

        for (const std::string& section : ini.GetSectionNames())
        {
            // Find an existing def to override, or append a new one.
            WeaponDef* def = nullptr;

            for (WeaponDef& existing : defs)
            {
                if (existing.Name == section)
                {
                    def = &existing;
                    break;
                }
            }

            if (!def)
            {
                defs.push_back(WeaponDef{});
                def = &defs.back();
                def->Name = section;
            }

            std::string currentKind = "Gun";

            if (def->Kind == WeaponKind::Digger)
                currentKind = "Digger";
            else if (def->Kind == WeaponKind::Shovel)
                currentKind = "Shovel";

            const std::string kind =
                ini.GetString(section, "Kind", currentKind);

            if (kind == "Digger")
                def->Kind = WeaponKind::Digger;
            else if (kind == "Shovel")
                def->Kind = WeaponKind::Shovel;
            else
                def->Kind = WeaponKind::Gun;

            def->FireRate = ini.GetFloat(section, "FireRate", def->FireRate);
            def->ClipSize = ini.GetInt(section, "ClipSize", def->ClipSize);
            def->ReloadTime = ini.GetFloat(section, "ReloadTime", def->ReloadTime);
            def->MuzzleVelocity =
                ini.GetFloat(section, "MuzzleVelocity", def->MuzzleVelocity);
            def->SpreadDegrees =
                ini.GetFloat(section, "Spread", def->SpreadDegrees);
            def->Damage = ini.GetInt(section, "Damage", def->Damage);
            def->Power = ini.GetFloat(section, "Power", def->Power);
            def->Pellets = ini.GetInt(section, "Pellets", def->Pellets);
            def->Recoil = ini.GetFloat(section, "Recoil", def->Recoil);
            def->BarrelLength =
                ini.GetFloat(section, "BarrelLength", def->BarrelLength);
            def->DigRadius = ini.GetFloat(section, "DigRadius", def->DigRadius);
            def->DigRange = ini.GetFloat(section, "DigRange", def->DigRange);
            def->DigPower = ini.GetFloat(section, "DigPower", def->DigPower);
            def->MaxDigStrength = ini.GetFloat(
                section, "MaxDigStrength", def->MaxDigStrength);
        }

        return defs;
    }

    Weapon::Weapon()
        : m_Def(nullptr),
        m_Cooldown(0.0f),
        m_ReloadTimer(0.0f),
        m_Ammo(0),
        m_RandomState(0xB5297A4Du)
    {
    }

    float Weapon::RandomUnit()
    {
        m_RandomState ^= m_RandomState << 13;
        m_RandomState ^= m_RandomState >> 17;
        m_RandomState ^= m_RandomState << 5;

        return static_cast<float>(m_RandomState & 0xFFFF) / 32768.0f - 1.0f;
    }

    void Weapon::SetDef(const WeaponDef* def)
    {
        m_Def = def;
        m_Cooldown = 0.0f;
        m_ReloadTimer = 0.0f;
        m_Ammo = def ? def->ClipSize : 0;
    }

    const WeaponDef* Weapon::GetDef() const
    {
        return m_Def;
    }

    void Weapon::Update(float deltaTime)
    {
        if (m_Cooldown > 0.0f)
            m_Cooldown -= deltaTime;

        if (m_ReloadTimer > 0.0f)
        {
            m_ReloadTimer -= deltaTime;

            if (m_ReloadTimer <= 0.0f && m_Def)
                m_Ammo = m_Def->ClipSize;
        }
    }

    bool Weapon::TryFire(
        ParticleSystem& particles,
        Terrain& terrain,
        float muzzleX,
        float muzzleY,
        float dirX,
        float dirY,
        float targetX,
        float targetY,
        Actor* owner)
    {
        if (!m_Def)
            return false;

        if (m_Cooldown > 0.0f || IsReloading())
            return false;

        const bool infiniteAmmo = m_Def->ClipSize == 0;

        if (!infiniteAmmo && m_Ammo <= 0)
        {
            StartReload();
            return false;
        }

        m_Cooldown = 1.0f / m_Def->FireRate;

        if (m_Def->Kind == WeaponKind::Digger ||
            m_Def->Kind == WeaponKind::Shovel)
        {
            // Dig tools work like a raycast: whatever surface is first in
            // front of the tool gets broken. No carving through walls at
            // the cursor. The ray starts back at the hand so a muzzle
            // pressed into a wall still digs the near face.
            const float reachBehind = m_Def->BarrelLength + 4.0f;

            float hitX = 0.0f;
            float hitY = 0.0f;

            if (!terrain.RaycastSolid(
                muzzleX - dirX * reachBehind,
                muzzleY - dirY * reachBehind,
                dirX,
                dirY,
                m_Def->DigRange + reachBehind,
                hitX,
                hitY))
            {
                return false;
            }

            // Erode the struck face: each tick spends a strength budget,
            // nearest pixels first, so soft dirt melts away while stone
            // is ground down slowly. Material harder than the tool's cap
            // doesn't budge at all.
            const float digX = hitX + dirX * m_Def->DigRadius * 0.35f;
            const float digY = hitY + dirY * m_Def->DigRadius * 0.35f;

            float budget = m_Def->DigPower;
            int removed = 0;
            bool struckHard = false;

            const int span = static_cast<int>(m_Def->DigRadius) + 1;

            // Two rings: eat the contact area first, then the fringe.
            for (int pass = 0; pass < 2 && budget > 0.0f; pass++)
            {
                const float passRadius = pass == 0
                    ? m_Def->DigRadius * 0.55f
                    : m_Def->DigRadius;

                const float radiusSquared = passRadius * passRadius;
                const float innerSquared = pass == 0
                    ? -1.0f
                    : m_Def->DigRadius * 0.55f * m_Def->DigRadius * 0.55f;

                for (int dy = -span; dy <= span && budget > 0.0f; dy++)
                {
                    for (int dx = -span; dx <= span && budget > 0.0f; dx++)
                    {
                        const float distSquared =
                            static_cast<float>(dx * dx + dy * dy);

                        if (distSquared > radiusSquared ||
                            distSquared <= innerSquared)
                            continue;

                        const int px = static_cast<int>(digX) + dx;
                        const int py = static_cast<int>(digY) + dy;

                        const Material material = terrain.GetMaterial(px, py);
                        const MaterialInfo& info = GetMaterialInfo(material);

                        if (!info.Solid)
                            continue;

                        if (info.Strength > m_Def->MaxDigStrength ||
                            budget < info.Strength)
                        {
                            struckHard = true;
                            continue;
                        }

                        budget -= info.Strength;
                        terrain.DestroyPixel(px, py);
                        removed++;

                        // Spray some of the spoil back toward the digger.
                        if ((removed & 1) == 0)
                        {
                            particles.SpawnDebris(
                                static_cast<float>(px),
                                static_cast<float>(py),
                                -dirX * 70.0f + RandomUnit() * 80.0f,
                                -dirY * 40.0f - 90.0f + RandomUnit() * 60.0f,
                                material);
                        }
                    }
                }
            }

            // Grinding against rock the tool can't eat: sparks, no dig.
            if (struckHard && removed < 3)
            {
                particles.SpawnSpark(
                    hitX, hitY,
                    -dirX * 60.0f + RandomUnit() * 70.0f,
                    -dirY * 60.0f - 40.0f);
                particles.SpawnSpark(
                    hitX, hitY,
                    -dirX * 30.0f + RandomUnit() * 70.0f,
                    -30.0f + RandomUnit() * 40.0f);
            }

            return removed > 0;
        }

        if (!infiniteAmmo)
        {
            m_Ammo--;

            if (m_Ammo <= 0)
                StartReload();
        }

        const float spreadRadians =
            m_Def->SpreadDegrees * 3.14159265f / 180.0f;

        const float baseAngle = std::atan2(dirY, dirX);

        for (int i = 0; i < m_Def->Pellets; i++)
        {
            const float angle = baseAngle + RandomUnit() * spreadRadians;
            const float speed =
                m_Def->MuzzleVelocity * (1.0f + RandomUnit() * 0.04f);

            particles.SpawnBullet(
                muzzleX,
                muzzleY,
                std::cos(angle) * speed,
                std::sin(angle) * speed,
                m_Def->Damage,
                m_Def->Power,
                owner);
        }

        // Muzzle smoke and an ejected casing.
        particles.SpawnSmoke(
            muzzleX,
            muzzleY,
            dirX * 40.0f + RandomUnit() * 20.0f,
            dirY * 40.0f - 20.0f);

        particles.SpawnCasing(
            muzzleX - dirX * m_Def->BarrelLength,
            muzzleY - dirY * m_Def->BarrelLength - 2.0f,
            -dirX * 40.0f + RandomUnit() * 30.0f,
            -110.0f + RandomUnit() * 40.0f);

        return true;
    }

    void Weapon::StartReload()
    {
        if (!m_Def || m_Def->ClipSize == 0 || IsReloading())
            return;

        if (m_Ammo == m_Def->ClipSize)
            return;

        m_ReloadTimer = m_Def->ReloadTime;
    }

    bool Weapon::IsReloading() const
    {
        return m_ReloadTimer > 0.0f;
    }

    float Weapon::GetReloadProgress() const
    {
        if (!m_Def || !IsReloading())
            return 1.0f;

        return 1.0f - m_ReloadTimer / m_Def->ReloadTime;
    }

    int Weapon::GetAmmo() const
    {
        return m_Ammo;
    }

    int Weapon::GetClipSize() const
    {
        return m_Def ? m_Def->ClipSize : 0;
    }
}
