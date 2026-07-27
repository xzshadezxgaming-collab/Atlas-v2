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
            WeaponDef digger;
            digger.Name = "Digger";
            digger.Kind = WeaponKind::Digger;
            digger.FireRate = 14.0f;
            digger.ClipSize = 0;
            digger.DigRadius = 9.0f;
            digger.DigRange = 110.0f;
            digger.BarrelLength = 12.0f;
            digger.Recoil = 0.0f;
            defs.push_back(digger);
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

            const std::string kind =
                ini.GetString(section, "Kind", def->Kind == WeaponKind::Digger
                    ? "Digger"
                    : "Gun");

            def->Kind = (kind == "Digger") ? WeaponKind::Digger : WeaponKind::Gun;

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

        if (m_Def->Kind == WeaponKind::Digger)
        {
            // Carve at the aim point, clamped to the digger's range.
            float digX = targetX;
            float digY = targetY;

            const float dx = targetX - muzzleX;
            const float dy = targetY - muzzleY;
            const float distance = std::sqrt(dx * dx + dy * dy);

            if (distance > m_Def->DigRange)
            {
                digX = muzzleX + dx / distance * m_Def->DigRange;
                digY = muzzleY + dy / distance * m_Def->DigRange;
            }

            std::vector<Terrain::DestroyedPixel> debris;
            terrain.CarveCircleCollect(
                digX, digY, m_Def->DigRadius, debris, 6);

            for (const Terrain::DestroyedPixel& pixel : debris)
            {
                particles.SpawnDebris(
                    pixel.X,
                    pixel.Y,
                    RandomUnit() * 80.0f,
                    -90.0f + RandomUnit() * 60.0f,
                    pixel.Mat);
            }

            return !debris.empty();
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
