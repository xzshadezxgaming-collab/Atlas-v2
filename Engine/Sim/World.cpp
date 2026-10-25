#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Atlas
{
    namespace
    {
        constexpr float GrenadeCooldownTime = 0.9f;
        constexpr float RespawnTime = 3.0f;

        // Drop ships start this far to the side of the ordering player:
        // half a view width plus a margin, so they visibly fly in from
        // beyond the edge of that player's screen.
        constexpr float ShipSpawnDistance = 640.0f + 220.0f;

        Sfx FireSfxFor(const WeaponDef& def)
        {
            if (def.Kind == WeaponKind::Digger ||
                def.Kind == WeaponKind::Shovel)
                return Sfx::Dig;

            if (def.Name == "Shotgun")
                return Sfx::Shotgun;

            if (def.Name == "Rifle")
                return Sfx::Rifle;

            return Sfx::Shot;
        }

        // FNV-1a, fed field by field.
        struct Hasher
        {
            std::uint64_t Value = 1469598103934665603ull;

            void Bytes(const void* data, std::size_t size)
            {
                const auto* bytes = static_cast<const std::uint8_t*>(data);

                for (std::size_t i = 0; i < size; i++)
                {
                    Value ^= bytes[i];
                    Value *= 1099511628211ull;
                }
            }

            void Int(std::int64_t v) { Bytes(&v, sizeof(v)); }

            void Float(float f)
            {
                std::uint32_t bits = 0;
                std::memcpy(&bits, &f, sizeof(bits));
                Bytes(&bits, sizeof(bits));
            }

            void ActorState(const Actor& actor)
            {
                Int(actor.GetId());
                Int(actor.GetTeam());
                Int(actor.GetHealth());
                Int(actor.GetGold());
                Float(actor.GetX());
                Float(actor.GetY());
                Float(actor.GetVelocityX());
                Float(actor.GetVelocityY());
                Float(actor.GetFuel());
            }
        };
    }

    World::World()
        : m_Loadout{},
        m_EnemyWeapon(nullptr),
        m_AllyWeapon(nullptr),
        m_Rng(1337u),
        m_NextActorId(1),
        m_Tick(0),
        m_Wave(0),
        m_WaveTimer(2.5f),
        m_BotsEnabled(true)
    {
    }

    bool World::Create(const WorldConfig& config)
    {
        if (!m_Terrain.Create(config.Width, config.Height, config.Seed))
            return false;

        m_Rng.seed(config.Seed);

        // Weapons: data-driven with built-in fallbacks. Pointers into
        // this vector are handed out, so it never changes after here.
        m_WeaponDefs = config.Weapons;

        if (m_WeaponDefs.empty())
            m_WeaponDefs = LoadWeaponDefs(std::string());

        m_Loadout[0] = FindWeapon("SMG");
        m_Loadout[1] = FindWeapon("Shotgun");
        m_Loadout[2] = FindWeapon("Rifle");
        m_Loadout[3] = FindWeapon("Digger");
        m_Loadout[4] = FindWeapon("Shovel");

        m_EnemyWeapon = FindWeapon("EnemyGun");
        m_AllyWeapon = FindWeapon("SMG");

        return true;
    }

    float World::RandomUnit()
    {
        // Top 24 bits -> [0, 1) exactly representable, then to [-1, 1).
        const float u01 =
            static_cast<float>(m_Rng() >> 8) * (1.0f / 16777216.0f);
        return u01 * 2.0f - 1.0f;
    }

    float World::RandomRange(float min, float max)
    {
        return min + (RandomUnit() * 0.5f + 0.5f) * (max - min);
    }

    void World::Emit(const SimEvent& event)
    {
        m_Events.push_back(event);
    }

    void World::EmitSound(Sfx sound, float volume)
    {
        SimEvent event;
        event.Type = SimEventType::Sound;
        event.Sound = sound;
        event.Volume = volume;
        Emit(event);
    }

    void World::InitActor(Actor& actor, int team)
    {
        actor.SetId(m_NextActorId++);
        actor.SetTeam(team);
    }

    float World::GetSpawnX(int playerSlot) const
    {
        return static_cast<float>(m_Terrain.GetWidth()) * 0.5f +
            static_cast<float>(playerSlot) * 48.0f;
    }

    int World::AddPlayer()
    {
        if (static_cast<int>(m_Players.size()) >= MaxPlayers)
            return -1;

        const int slot = static_cast<int>(m_Players.size());

        PlayerSlot player;
        player.Body = std::make_unique<Actor>();
        InitActor(*player.Body, 0);

        // Player 1 keeps the sprite's own colors; the others get a tint
        // so co-op players can tell each other apart.
        static constexpr std::uint8_t Tints[MaxPlayers][3] =
        {
            { 255, 255, 255 },
            { 255, 214, 120 },
            { 150, 255, 205 },
            { 215, 165, 255 },
        };

        player.Body->SetTint(Tints[slot][0], Tints[slot][1], Tints[slot][2]);
        player.Body->SetWeaponDef(m_Loadout[0]);
        player.Body->Spawn(m_Terrain, GetSpawnX(slot));

        m_Players.push_back(std::move(player));
        return slot;
    }

    void World::SpawnEnemy(float x)
    {
        Enemy enemy;
        enemy.Body = std::make_unique<Actor>();
        InitActor(*enemy.Body, 1);
        enemy.Body->SetTint(255, 118, 106);
        enemy.Body->SetWeaponDef(m_EnemyWeapon);
        enemy.Body->Spawn(m_Terrain, x);
        m_Enemies.push_back(std::move(enemy));
    }

    void World::SpawnWave(int count)
    {
        const float minX = 150.0f;
        const float maxX = static_cast<float>(m_Terrain.GetWidth() - 150);

        auto tooCloseToAPlayer = [&](float x)
        {
            for (const PlayerSlot& player : m_Players)
            {
                if (std::fabs(x - player.Body->GetCenterX()) < 380.0f)
                    return true;
            }

            return false;
        };

        for (int i = 0;
            i < count && static_cast<int>(m_Enemies.size()) < MaxEnemies;
            i++)
        {
            float x = RandomRange(minX, maxX);

            for (int attempt = 0; attempt < 10 && tooCloseToAPlayer(x); attempt++)
                x = RandomRange(minX, maxX);

            SpawnEnemy(x);
        }
    }

    // weapon: null falls back to the default rifleman loadout (SMG).
    void World::SpawnAlly(float x, const WeaponDef* weapon)
    {
        if (static_cast<int>(m_Allies.size()) >= MaxAllies)
            return;

        Ally ally;
        ally.Body = std::make_unique<Actor>();
        InitActor(*ally.Body, 0);
        ally.Body->SetTint(120, 170, 255);
        ally.Body->SetWeaponDef(weapon ? weapon : m_AllyWeapon);
        ally.Body->Spawn(m_Terrain, x);
        m_Allies.push_back(std::move(ally));
    }

    Actor* World::NearestLivingPlayer(float x, float y)
    {
        Actor* nearest = nullptr;
        float nearestDistSquared = 0.0f;

        for (PlayerSlot& player : m_Players)
        {
            if (!player.Body->IsAlive())
                continue;

            const float dx = player.Body->GetCenterX() - x;
            const float dy = player.Body->GetCenterY() - y;
            const float distSquared = dx * dx + dy * dy;

            if (!nearest || distSquared < nearestDistSquared)
            {
                nearest = player.Body.get();
                nearestDistSquared = distSquared;
            }
        }

        return nearest;
    }

    Actor* World::FindActor(std::uint32_t id)
    {
        if (id == 0u)
            return nullptr;

        for (PlayerSlot& player : m_Players)
        {
            if (player.Body->GetId() == id)
                return player.Body.get();
        }

        for (Enemy& enemy : m_Enemies)
        {
            if (enemy.Body->GetId() == id)
                return enemy.Body.get();
        }

        for (Ally& ally : m_Allies)
        {
            if (ally.Body->GetId() == id)
                return ally.Body.get();
        }

        return nullptr;
    }

    int World::PriceOrder(const OrderRequest& order) const
    {
        int total = 0;

        // A body lands with the default rifleman weapon at no extra
        // charge; any other weapon assigned to it is paid for.
        for (const WeaponDef* weapon : order.Bodies)
        {
            total += ReinforcementCost;

            if (weapon && weapon != m_AllyWeapon)
                total += weapon->Cost;
        }

        for (const WeaponDef* weapon : order.LooseWeapons)
        {
            if (weapon)
                total += weapon->Cost;
        }

        total += order.SupplyCrates * SupplyCrateCost;

        return total;
    }

    bool World::SubmitOrder(int player, const OrderRequest& order)
    {
        if (player < 0 || player >= GetPlayerCount())
            return false;

        Actor& buyer = *m_Players[player].Body;

        if (order.Bodies.empty() &&
            order.LooseWeapons.empty() &&
            order.SupplyCrates <= 0)
            return false;

        const int cost = PriceOrder(order);

        if (buyer.GetGold() < cost)
            return false;

        buyer.AddGold(-cost);

        auto queueDelivery = [&](DeliveryKind kind, const WeaponDef* weapon)
        {
            const bool fromLeft = (m_Rng() % 2u) == 0u;
            const float shipDir = fromLeft ? 1.0f : -1.0f;
            const float shipOrigin = fromLeft
                ? buyer.GetCenterX() - ShipSpawnDistance
                : buyer.GetCenterX() + ShipSpawnDistance;

            m_Deliveries.Order(
                kind, shipOrigin, shipDir, buyer.GetCenterX(), weapon, player);
        };

        for (const WeaponDef* weapon : order.Bodies)
            queueDelivery(DeliveryKind::Reinforcement, weapon);

        for (const WeaponDef* weapon : order.LooseWeapons)
            queueDelivery(DeliveryKind::WeaponPickup, weapon);

        for (int i = 0; i < order.SupplyCrates; i++)
            queueDelivery(DeliveryKind::SupplyCrate, nullptr);

        return true;
    }

    void World::ApplyDiggerLimbDamage(Actor& digger)
    {
        // The digger can also maim: whatever limb its impact point lands
        // on takes LimbDamage. The raycast here mirrors what TryFire just
        // did internally (same swept direction, no time has passed), so
        // the hit point matches.
        const WeaponDef& def = *digger.GetWeapon().GetDef();

        float sweptDirX = 0.0f;
        float sweptDirY = 0.0f;
        digger.GetWeapon().GetSweptDirection(
            digger.GetAimDirX(), digger.GetAimDirY(), sweptDirX, sweptDirY);

        const float reachBehind = def.BarrelLength + 4.0f;

        float hitX = 0.0f;
        float hitY = 0.0f;

        if (!m_Terrain.RaycastSolid(
            digger.GetMuzzleX() - sweptDirX * reachBehind,
            digger.GetMuzzleY() - sweptDirY * reachBehind,
            sweptDirX,
            sweptDirY,
            def.DigRange + reachBehind,
            hitX,
            hitY))
        {
            return;
        }

        for (Enemy& enemy : m_Enemies)
        {
            if (!enemy.Body || !enemy.Body->IsAlive())
                continue;

            BodyPartId hitPart = BodyPartId::None;
            float local01X = 0.0f;
            float local01Y = 0.0f;

            if (enemy.Body->TestLimbHit(hitX, hitY, hitPart, local01X, local01Y))
            {
                enemy.Body->TakeLimbDamage(
                    hitPart,
                    def.LimbDamage,
                    local01X,
                    local01Y,
                    &m_Particles,
                    -sweptDirX * 40.0f,
                    -sweptDirY * 40.0f);
            }
        }
    }

    void World::StepPlayer(int slot, const PlayerInput& input)
    {
        PlayerSlot& state = m_Players[slot];
        Actor& player = *state.Body;

        state.GrenadeCooldown -= FixedTimeStep;
        state.JetSoundTimer -= FixedTimeStep;
        state.ToolSfxTimer -= FixedTimeStep;

        if (!player.IsAlive())
        {
            state.RespawnTimer -= FixedTimeStep;

            if (state.RespawnTimer <= 0.0f)
            {
                player.ResetVitals();
                player.Spawn(m_Terrain, GetSpawnX(slot));
                state.Gibbed = false;
            }

            return;
        }

        // Weapon switching and reload.
        if (input.SelectSlot >= 0 && input.SelectSlot < LoadoutSize)
            state.CurrentSlot = input.SelectSlot;

        if (player.GetWeapon().GetDef() != m_Loadout[state.CurrentSlot])
        {
            player.SetWeaponDef(m_Loadout[state.CurrentSlot]);
            EmitSound(Sfx::Reload, 0.7f);
        }

        if (input.ReloadPressed)
        {
            player.GetWeapon().StartReload();
            EmitSound(Sfx::Reload);
        }

        // Digger plasma-beam visual: tracks the aim every tick while the
        // trigger is held, independent of the tool's slower fire rate.
        {
            const Weapon& held = player.GetWeapon();

            const bool isDigging =
                held.GetDef() &&
                held.GetDef()->Kind == WeaponKind::Digger &&
                input.Fire &&
                !player.IsArmDestroyed();

            if (isDigging)
            {
                const WeaponDef& def = *held.GetDef();
                const float muzzleX = player.GetMuzzleX();
                const float muzzleY = player.GetMuzzleY();

                float dirX = player.GetAimDirX();
                float dirY = player.GetAimDirY();
                held.GetSweptDirection(
                    player.GetAimDirX(), player.GetAimDirY(), dirX, dirY);

                float hitX = 0.0f;
                float hitY = 0.0f;

                if (m_Terrain.RaycastSolid(
                    muzzleX, muzzleY, dirX, dirY, def.DigRange, hitX, hitY))
                {
                    player.SetDigBeam(true, hitX, hitY);
                }
                else
                {
                    player.SetDigBeam(
                        true,
                        muzzleX + dirX * def.DigRange,
                        muzzleY + dirY * def.DigRange);
                }
            }
            else
            {
                player.SetDigBeam(false, 0.0f, 0.0f);
            }
        }

        player.SetAim(input.AimX, input.AimY);

        if (input.JumpPressed && player.IsGrounded())
            EmitSound(Sfx::Jump, 0.5f);

        state.PrevFallSpeed = player.GetVelocityY();
        state.WasGrounded = player.IsGrounded();

        player.Update(
            m_Terrain,
            &m_Particles,
            FixedTimeStep,
            std::clamp(input.MoveX, -1.0f, 1.0f),
            input.Jump,
            input.Jetpack,
            input.Crouch);

        if (!state.WasGrounded &&
            player.IsGrounded() &&
            state.PrevFallSpeed > 320.0f)
        {
            EmitSound(Sfx::Land, 0.6f);

            SimEvent landing;
            landing.Type = SimEventType::HardLanding;
            landing.Player = slot;
            landing.Amount = std::min(1.0f, state.PrevFallSpeed / 900.0f);
            Emit(landing);

            // Landing kicks up a dust ring.
            for (int i = 0; i < 7; i++)
            {
                m_Particles.SpawnDust(
                    player.GetCenterX() + RandomUnit() * 10.0f,
                    player.GetY() + player.GetHeight() + 20.0f,
                    RandomUnit() * 60.0f,
                    -20.0f - (i % 3) * 12.0f);
            }
        }

        // Footstep puffs on each fresh foot plant while actually walking.
        for (int leg = 0; leg < 2; leg++)
        {
            const bool planted = player.GetLeg(leg).IsPlanted();

            if (planted && !state.PrevLegPlanted[leg] &&
                std::fabs(player.GetVelocityX()) > 40.0f &&
                player.IsGrounded() &&
                !player.IsCrouching())
            {
                m_Particles.SpawnDust(
                    player.GetLeg(leg).GetFootX(),
                    player.GetLeg(leg).GetFootY() - 1.0f,
                    -player.GetVelocityX() * 0.08f,
                    -16.0f);
            }

            state.PrevLegPlanted[leg] = planted;
        }

        if (player.IsJetting() && state.JetSoundTimer <= 0.0f)
        {
            EmitSound(Sfx::JetBurst, 0.55f);
            state.JetSoundTimer = 0.15f;
        }

        player.GetWeapon().Update(FixedTimeStep);

        if (input.Fire && !player.IsArmDestroyed())
        {
            if (player.GetWeapon().TryFire(
                m_Particles,
                m_Terrain,
                player.GetMuzzleX(),
                player.GetMuzzleY(),
                player.GetAimDirX(),
                player.GetAimDirY(),
                input.AimX,
                input.AimY,
                &player))
            {
                player.NotifyFired();

                const WeaponDef& def = *player.GetWeapon().GetDef();

                if (def.Kind == WeaponKind::Gun)
                {
                    SimEvent fired;
                    fired.Type = SimEventType::GunFired;
                    fired.Player = slot;
                    Emit(fired);

                    EmitSound(FireSfxFor(def), 0.8f);
                }
                else if (state.ToolSfxTimer <= 0.0f)
                {
                    // Dig tools tick fast; don't machine-gun the sound.
                    EmitSound(FireSfxFor(def), 0.7f);
                    state.ToolSfxTimer = 0.1f;
                }

                if (def.Kind == WeaponKind::Digger && def.LimbDamage > 0)
                    ApplyDiggerLimbDamage(player);
            }
        }

        if (input.ThrowPressed && state.GrenadeCooldown <= 0.0f)
        {
            m_Grenades.Throw(
                player.GetHandX(),
                player.GetHandY(),
                player.GetAimDirX() * 520.0f + player.GetVelocityX() * 0.4f,
                player.GetAimDirY() * 520.0f - 60.0f);

            state.GrenadeCooldown = GrenadeCooldownTime;

            EmitSound(Sfx::Throw, 0.7f);
        }
    }

    void World::Step(const PlayerInput* inputs, int inputCount)
    {
        m_Events.clear();

        // --- Players ---
        for (int slot = 0; slot < GetPlayerCount(); slot++)
        {
            const PlayerInput input =
                (inputs && slot < inputCount) ? inputs[slot] : PlayerInput{};

            StepPlayer(slot, input);
        }

        // --- Actor roster for collisions ---
        Actor* actorPtrs[MaxPlayers + MaxEnemies + MaxAllies];
        int actorCount = 0;

        for (PlayerSlot& player : m_Players)
        {
            if (player.Body->IsAlive())
                actorPtrs[actorCount++] = player.Body.get();
        }

        for (Enemy& enemy : m_Enemies)
            actorPtrs[actorCount++] = enemy.Body.get();

        for (Ally& ally : m_Allies)
            actorPtrs[actorCount++] = ally.Body.get();

        // --- Enemies: each hunts the nearest living player ---
        for (Enemy& enemy : m_Enemies)
        {
            Actor* target = NearestLivingPlayer(
                enemy.Body->GetCenterX(), enemy.Body->GetCenterY());

            enemy.Brain.Update(
                *enemy.Body,
                target,
                m_Terrain,
                m_Particles,
                m_Terrain,
                FixedTimeStep);
        }

        // --- Allies: each fights whichever enemy is nearest ---
        for (Ally& ally : m_Allies)
        {
            Actor* nearestEnemy = nullptr;
            float nearestDistSquared = 0.0f;

            for (Enemy& enemy : m_Enemies)
            {
                if (!enemy.Body->IsAlive())
                    continue;

                const float dx =
                    enemy.Body->GetCenterX() - ally.Body->GetCenterX();
                const float dy =
                    enemy.Body->GetCenterY() - ally.Body->GetCenterY();
                const float distSquared = dx * dx + dy * dy;

                if (!nearestEnemy || distSquared < nearestDistSquared)
                {
                    nearestEnemy = enemy.Body.get();
                    nearestDistSquared = distSquared;
                }
            }

            ally.Brain.Update(
                *ally.Body,
                nearestEnemy,
                m_Terrain,
                m_Particles,
                m_Terrain,
                FixedTimeStep);
        }

        // --- Grenades and particles ---
        m_Grenades.Update(
            m_Terrain, m_Particles, actorPtrs, actorCount, FixedTimeStep);

        if (m_Grenades.ExplodedThisFrame())
        {
            EmitSound(Sfx::Explosion);

            SimEvent blast;
            blast.Type = SimEventType::Explosion;
            Emit(blast);
        }

        m_GoldCredits.clear();
        m_Particles.Update(
            m_Terrain, FixedTimeStep, actorPtrs, actorCount, &m_GoldCredits);

        // Gold mined by bullets goes to whoever fired them - if they
        // still exist.
        for (const GoldCredit& credit : m_GoldCredits)
        {
            if (Actor* owner = FindActor(credit.OwnerId))
                owner->AddGold(credit.Amount);
        }

        // Damage events: compare each enemy's health to last tick's -
        // catches every damage source without any plumbing.
        for (Enemy& enemy : m_Enemies)
        {
            const int health = enemy.Body->GetHealth();

            if (health < enemy.LastHealth)
            {
                // Clamp the reported damage to the health that actually
                // remained (a 9999 both-legs-gone kill shouldn't report
                // "9999").
                const int delta = enemy.LastHealth - std::max(0, health);

                if (delta > 0)
                {
                    SimEvent damage;
                    damage.Type = SimEventType::Damage;
                    damage.X = enemy.Body->GetCenterX() + RandomUnit() * 5.0f;
                    damage.Y = enemy.Body->GetY() - 8.0f;
                    damage.Value = delta;
                    Emit(damage);
                }
            }

            enemy.LastHealth = health;
        }

        // --- Deaths ---
        for (PlayerSlot& player : m_Players)
        {
            if (!player.Body->IsAlive() && !player.Gibbed)
            {
                player.Body->Gib(m_Particles);
                EmitSound(Sfx::Gib);
                player.RespawnTimer = RespawnTime;
                player.Gibbed = true;
            }
        }

        for (std::size_t i = 0; i < m_Enemies.size(); )
        {
            Actor& body = *m_Enemies[i].Body;

            if (!body.IsAlive())
            {
                body.Gib(m_Particles);
                EmitSound(Sfx::Gib, 0.8f);

                SimEvent kill;
                kill.Type = SimEventType::EnemyKilled;
                kill.X = body.GetCenterX();
                kill.Y = body.GetCenterY();
                Emit(kill);

                // Sometimes drop a medkit.
                if (RandomUnit() > 0.2f)
                {
                    m_HealthPickups.push_back(
                        { body.GetCenterX(), body.GetCenterY(), -120.0f });
                }

                m_Enemies.erase(m_Enemies.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                i++;
            }
        }

        for (std::size_t i = 0; i < m_Allies.size(); )
        {
            if (!m_Allies[i].Body->IsAlive())
            {
                m_Allies[i].Body->Gib(m_Particles);
                EmitSound(Sfx::Gib, 0.8f);

                m_Allies.erase(m_Allies.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                i++;
            }
        }

        // Pickups fall until they rest on terrain, then wait for a living
        // player to touch them. Returns the player slot that grabbed it.
        auto settleAndTouch = [&](float& x, float& y, float& velY, float radius)
        {
            velY += 900.0f * FixedTimeStep;

            const float fall = velY * FixedTimeStep;

            if (!m_Terrain.IsSolid(x, y + fall + 4.0f))
                y += fall;
            else
                velY = 0.0f;

            for (int slot = 0; slot < GetPlayerCount(); slot++)
            {
                const Actor& body = *m_Players[slot].Body;

                const float dx = x - body.GetCenterX();
                const float dy = y - body.GetCenterY();

                if (body.IsAlive() && dx * dx + dy * dy < radius * radius)
                    return slot;
            }

            return -1;
        };

        // --- Medkit pickups ---
        for (std::size_t i = 0; i < m_HealthPickups.size(); )
        {
            HealthPickup& pickup = m_HealthPickups[i];

            const int taker =
                settleAndTouch(pickup.X, pickup.Y, pickup.VelY, 34.0f);

            if (taker >= 0)
            {
                m_Players[taker].Body->Heal(30);
                EmitSound(Sfx::Reload, 0.9f);

                m_HealthPickups[i] = m_HealthPickups.back();
                m_HealthPickups.pop_back();
            }
            else
            {
                i++;
            }
        }

        // --- Waves ---
        if (m_BotsEnabled && m_Enemies.empty())
        {
            m_WaveTimer -= FixedTimeStep;

            if (m_WaveTimer <= 0.0f)
            {
                m_Wave++;
                SpawnWave(std::min(1 + m_Wave, MaxEnemies));
                m_WaveTimer = 4.0f;

                SimEvent wave;
                wave.Type = SimEventType::WaveStarted;
                wave.Value = m_Wave;
                Emit(wave);
            }
        }

        // --- Deliveries: the system owns flight/fall physics; the
        // gameplay effect of a landed payload happens here ---
        {
            std::vector<const Delivery*> landed;
            std::vector<const Delivery*> delivered;

            m_Deliveries.Update(m_Terrain, FixedTimeStep, landed, delivered);

            if (!landed.empty())
                EmitSound(Sfx::Land, 0.7f);

            for (const Delivery* delivery : delivered)
            {
                switch (delivery->Kind)
                {
                case DeliveryKind::SupplyCrate:
                    if (delivery->OwnerPlayer >= 0 &&
                        delivery->OwnerPlayer < GetPlayerCount())
                    {
                        Actor& owner = *m_Players[delivery->OwnerPlayer].Body;

                        if (owner.IsAlive())
                        {
                            owner.Heal(50);
                            owner.GetWeapon().Refill();
                        }
                    }
                    break;

                case DeliveryKind::Reinforcement:
                    SpawnAlly(delivery->PayloadX, delivery->Weapon);
                    break;

                case DeliveryKind::WeaponPickup:
                    m_WeaponPickups.push_back({
                        delivery->PayloadX,
                        delivery->PayloadY,
                        0.0f,
                        delivery->Weapon });
                    break;
                }
            }
        }

        // --- Weapon field pickups ---
        for (std::size_t i = 0; i < m_WeaponPickups.size(); )
        {
            WeaponPickup& pickup = m_WeaponPickups[i];

            const int taker =
                settleAndTouch(pickup.X, pickup.Y, pickup.VelY, 30.0f);

            if (taker >= 0 && pickup.Weapon)
            {
                PlayerSlot& player = m_Players[taker];
                player.Body->SetWeaponDef(pickup.Weapon);

                // Keep the loadout slot in sync with what got equipped.
                for (int slot = 0; slot < LoadoutSize; slot++)
                {
                    if (m_Loadout[slot] == pickup.Weapon)
                    {
                        player.CurrentSlot = slot;
                        break;
                    }
                }

                EmitSound(Sfx::Reload, 0.8f);

                m_WeaponPickups[i] = m_WeaponPickups.back();
                m_WeaponPickups.pop_back();
            }
            else
            {
                i++;
            }
        }

        m_Tick++;
    }

    void World::SetBotsEnabled(bool enabled)
    {
        m_BotsEnabled = enabled;
    }

    bool World::IsBotsEnabled() const
    {
        return m_BotsEnabled;
    }

    const std::vector<SimEvent>& World::GetEvents() const
    {
        return m_Events;
    }

    std::uint64_t World::ComputeStateHash() const
    {
        Hasher h;

        h.Int(static_cast<std::int64_t>(m_Tick));
        h.Int(m_Wave);
        h.Float(m_WaveTimer);

        h.Bytes(
            m_Terrain.GetMaterialData(),
            static_cast<std::size_t>(m_Terrain.GetWidth()) *
                static_cast<std::size_t>(m_Terrain.GetHeight()));

        for (const PlayerSlot& player : m_Players)
        {
            h.ActorState(*player.Body);
            h.Int(player.CurrentSlot);
        }

        h.Int(static_cast<std::int64_t>(m_Enemies.size()));

        for (const Enemy& enemy : m_Enemies)
            h.ActorState(*enemy.Body);

        h.Int(static_cast<std::int64_t>(m_Allies.size()));

        for (const Ally& ally : m_Allies)
            h.ActorState(*ally.Body);

        h.Int(m_Particles.GetActiveCount());
        h.Int(m_Grenades.GetActiveCount());
        h.Int(m_Deliveries.GetActiveCount());

        for (const HealthPickup& pickup : m_HealthPickups)
        {
            h.Float(pickup.X);
            h.Float(pickup.Y);
        }

        for (const WeaponPickup& pickup : m_WeaponPickups)
        {
            h.Float(pickup.X);
            h.Float(pickup.Y);
        }

        return h.Value;
    }

    Terrain& World::GetTerrain() { return m_Terrain; }
    const Terrain& World::GetTerrain() const { return m_Terrain; }
    const ParticleSystem& World::GetParticles() const { return m_Particles; }
    const GrenadeSystem& World::GetGrenades() const { return m_Grenades; }
    const DeliverySystem& World::GetDeliveries() const { return m_Deliveries; }

    int World::GetPlayerCount() const
    {
        return static_cast<int>(m_Players.size());
    }

    PlayerSlot& World::GetPlayer(int slot) { return m_Players[slot]; }
    const PlayerSlot& World::GetPlayer(int slot) const { return m_Players[slot]; }

    const std::vector<Enemy>& World::GetEnemies() const { return m_Enemies; }
    std::vector<Enemy>& World::GetEnemies() { return m_Enemies; }
    const std::vector<Ally>& World::GetAllies() const { return m_Allies; }
    std::vector<Ally>& World::GetAllies() { return m_Allies; }

    const std::vector<HealthPickup>& World::GetHealthPickups() const
    {
        return m_HealthPickups;
    }

    const std::vector<WeaponPickup>& World::GetWeaponPickups() const
    {
        return m_WeaponPickups;
    }

    const std::vector<WeaponDef>& World::GetWeaponDefs() const
    {
        return m_WeaponDefs;
    }

    const WeaponDef* World::GetLoadoutWeapon(int slot) const
    {
        if (slot < 0 || slot >= LoadoutSize)
            return nullptr;

        return m_Loadout[slot];
    }

    const WeaponDef* World::GetAllyWeapon() const
    {
        return m_AllyWeapon;
    }

    const WeaponDef* World::FindWeapon(const std::string& name) const
    {
        for (const WeaponDef& def : m_WeaponDefs)
        {
            if (def.Name == name)
                return &def;
        }

        return m_WeaponDefs.empty() ? nullptr : &m_WeaponDefs.front();
    }

    int World::GetWeaponIndex(const WeaponDef* def) const
    {
        if (!def || m_WeaponDefs.empty())
            return -1;

        const std::ptrdiff_t index = def - m_WeaponDefs.data();

        if (index < 0 || index >= static_cast<std::ptrdiff_t>(m_WeaponDefs.size()))
            return -1;

        return static_cast<int>(index);
    }

    const WeaponDef* World::GetWeaponByIndex(int index) const
    {
        if (index < 0 || index >= static_cast<int>(m_WeaponDefs.size()))
            return nullptr;

        return &m_WeaponDefs[static_cast<std::size_t>(index)];
    }

    int World::GetWave() const
    {
        return m_Wave;
    }

    std::uint64_t World::GetTick() const
    {
        return m_Tick;
    }
}
