#include "World.h"

#include "../Combat/Explosion.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Atlas
{
    namespace
    {
        constexpr float GrenadeCooldownTime = 0.9f;
        constexpr float RespawnTime = 3.0f;

        // A dead player whose team has other bodies jumps into one after
        // this long instead of waiting out the respawn.
        constexpr float SwitchOnDeathDelay = 1.0f;

        // Bunker geometry, in pixels.
        constexpr int BunkerHalfInner = 100;
        constexpr int BunkerInnerHeight = 84;
        constexpr int BunkerWall = 8;
        constexpr int BunkerDoorHeight = 70;
        constexpr int BaseFlatHalf = 170; // ground levelled around a bunker
        constexpr int BaseRamp = 70;      // and blended back to the terrain

        // Where players stand up inside their bunker (clear of the jar).
        constexpr float PlayerSpawnOffsets[4] = { -52.0f, 52.0f, -78.0f, 78.0f };

        constexpr float FirstEnemyDropTime = 20.0f;

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

            if (def.Kind == WeaponKind::Launcher)
                return Sfx::Rocket;

            if (def.Kind == WeaponKind::Flamer)
                return Sfx::Flame;

            if (def.LaserSight)
                return Sfx::Sniper;

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
                Int(static_cast<std::int64_t>(actor.GetBodyClass()));
                Int(actor.GetItemCount());
                Int(actor.GetSelectedItem());
                Int(actor.GetWeapon().GetAmmo());
                Int(actor.IsBurning() ? 1 : 0);
            }
        };
    }

    World::World()
        : m_Loadout{},
        m_EnemyWeapon(nullptr),
        m_AllyWeapon(nullptr),
        m_Shovel(nullptr),
        m_Rocket(nullptr),
        m_Flamer(nullptr),
        m_Shotgun(nullptr),
        m_Digger(nullptr),
        m_Rng(1337u),
        m_NextActorId(1),
        m_Tick(0),
        m_SteamSoundTimer(0.0f),
        m_Wave(0),
        m_WaveTimer(2.5f),
        m_BotsEnabled(true),
        m_Mode(GameMode::BrainAssault),
        m_Outcome(MatchOutcome::InProgress),
        m_TeamGold{},
        m_BrainAlarmTimer(0.0f),
        m_Difficulty(1),
        m_AllyOrder(AllyOrder::Defend),
        m_AllyOrderPlayer(0)
    {
    }

    float TeamBase::GetBrainHealth() const
    {
        if (!BrainAlive || BrainPixelsStart <= 0)
            return 0.0f;

        const float dead = World::BrainDeathThreshold * static_cast<float>(BrainPixelsStart);
        const float span = static_cast<float>(BrainPixelsStart) - dead;

        return std::clamp((static_cast<float>(BrainPixels) - dead) / span, 0.0f, 1.0f);
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
        m_Shovel = FindWeapon("Shovel");
        m_Rocket = FindWeapon("Rocket Launcher");
        m_Flamer = FindWeapon("Flamethrower");
        m_Shotgun = FindWeapon("Shotgun");
        m_Digger = FindWeapon("Digger");

        m_Mode = config.Mode;
        m_Difficulty = std::clamp(config.Difficulty, 0, 2);

        if (m_Mode == GameMode::BrainAssault)
        {
            const float width = static_cast<float>(m_Terrain.GetWidth());

            const float landing = static_cast<float>(std::clamp(config.LandingZone, 6, 42)) / 100.0f;
            BuildBase(0, std::floor(width * landing));
            BuildBase(1, std::floor(width * 0.88f));

            static constexpr int GoldByDifficulty[3] = { 500, StartingGold, 200 };
            m_TeamGold[0] = GoldByDifficulty[m_Difficulty];
            m_TeamGold[1] = StartingGold;

            // A few guards at the enemy base from the start.
            const TeamBase& enemyBase = m_Bases[1];
            SpawnEnemy(enemyBase.X - 150.0f, false);
            SpawnEnemy(enemyBase.X - 190.0f, false);
            SpawnEnemy(enemyBase.X + 60.0f, false);

            m_WaveTimer = FirstEnemyDropTime + (m_Difficulty == 0 ? 10.0f : 0.0f);
        }

        return true;
    }

    void World::BuildBase(int team, float centerX)
    {
        TeamBase& base = m_Bases[team];

        const int cx = static_cast<int>(centerX);
        const int width = m_Terrain.GetWidth();
        const int height = m_Terrain.GetHeight();

        auto isBedrock = [&](int x, int y)
        {
            return m_Terrain.GetMaterial(x, y) == Material::Bedrock;
        };

        const int floorY = m_Terrain.GetSurfaceY(cx) + 4;

        // Level the ground around the bunker, blending back into the
        // natural surface on both sides. Loose sand and caves under the
        // footprint become packed dirt so the base can't slump.
        for (int x = cx - BaseFlatHalf - BaseRamp; x <= cx + BaseFlatHalf + BaseRamp; x++)
        {
            if (x < 0 || x >= width)
                continue;

            const int natural = m_Terrain.GetSurfaceY(x);
            const int fromCenter = std::abs(x - cx);

            int target = floorY;

            if (fromCenter > BaseFlatHalf)
            {
                const float t = static_cast<float>(fromCenter - BaseFlatHalf) /
                    static_cast<float>(BaseRamp);
                const float smooth = t * t * (3.0f - 2.0f * t);
                target = static_cast<int>(std::lround(
                    static_cast<float>(floorY) + (static_cast<float>(natural - floorY)) * smooth));
            }

            for (int y = 0; y < target; y++)
            {
                if (!isBedrock(x, y))
                    m_Terrain.SetMaterial(x, y, Material::Air);
            }

            const int fillDepth = fromCenter > BaseFlatHalf ? 24 : 48;

            for (int y = target; y < std::min(height, target + fillDepth); y++)
            {
                const Material here = m_Terrain.GetMaterial(x, y);

                if (here == Material::Bedrock)
                    break;

                if (GetMaterialInfo(here).Behavior != MaterialBehavior::Static)
                    m_Terrain.SetMaterial(x, y, y == target ? Material::Grass : Material::Dirt);
            }
        }

        // The shell: concrete walls, roof and floor, with a doorway on
        // each side. The inside shows a concrete wall behind it.
        const int innerLeft = cx - BunkerHalfInner;
        const int innerRight = cx + BunkerHalfInner; // exclusive
        const int innerTop = floorY - BunkerInnerHeight;
        const int outerLeft = innerLeft - BunkerWall;
        const int outerRight = innerRight + BunkerWall;
        const int outerTop = innerTop - BunkerWall;
        const int outerBottom = floorY + BunkerWall;

        for (int y = outerTop; y < outerBottom; y++)
        {
            for (int x = outerLeft; x < outerRight; x++)
            {
                if (isBedrock(x, y))
                    continue;

                const bool inside =
                    x >= innerLeft && x < innerRight && y >= innerTop && y < floorY;

                const bool doorway =
                    (x < innerLeft || x >= innerRight) &&
                    y >= floorY - BunkerDoorHeight && y < floorY;

                if (inside || doorway)
                {
                    m_Terrain.SetMaterial(x, y, Material::Air);
                    m_Terrain.SetBackwall(x, y, Material::Concrete);
                }
                else
                {
                    m_Terrain.SetMaterial(x, y, Material::Concrete);
                }
            }
        }

        // The brain: a pedestal, a glass jar full of water, and the
        // brain itself floating in it.
        const int pedestalTop = floorY - 10;

        for (int y = pedestalTop; y < floorY; y++)
        {
            for (int x = cx - 14; x < cx + 14; x++)
                m_Terrain.SetMaterial(x, y, Material::Concrete);
        }

        const int jarLeft = cx - 16;
        const int jarRight = cx + 16; // exclusive
        const int jarTop = pedestalTop - 38;

        for (int y = jarTop; y < pedestalTop; y++)
        {
            for (int x = jarLeft; x < jarRight; x++)
            {
                const bool rim =
                    x < jarLeft + 2 || x >= jarRight - 2 || y < jarTop + 2 || y >= pedestalTop - 2;

                m_Terrain.SetMaterial(x, y, rim ? Material::Glass : Material::Water);
            }
        }

        const float brainCX = static_cast<float>(cx) - 0.5f;
        const float brainCY = static_cast<float>(jarTop + 19);
        const float brainRX = 11.5f;
        const float brainRY = 9.5f;

        base.BrainMinX = cx - 12;
        base.BrainMaxX = cx + 11;
        base.BrainMinY = static_cast<int>(brainCY - brainRY);
        base.BrainMaxY = static_cast<int>(brainCY + brainRY);
        base.BrainPixelsStart = 0;

        for (int y = base.BrainMinY; y <= base.BrainMaxY; y++)
        {
            for (int x = base.BrainMinX; x <= base.BrainMaxX; x++)
            {
                const float ex = (static_cast<float>(x) - brainCX) / brainRX;
                const float ey = (static_cast<float>(y) - brainCY) / brainRY;

                // Two lobes: a notch along the top middle.
                const bool notch = std::abs(x - cx) <= 0 && ey < -0.35f;

                if (ex * ex + ey * ey <= 1.0f && !notch)
                {
                    m_Terrain.SetMaterial(x, y, Material::Brain);
                    base.BrainPixelsStart++;
                }
            }
        }

        m_Particles.SetFriendlyBrainBox(
            team, base.BrainMinX, base.BrainMinY, base.BrainMaxX, base.BrainMaxY);

        base.Exists = true;
        base.X = centerX;
        base.FloorY = floorY;
        base.BrainPixels = base.BrainPixelsStart;
        base.BrainAlive = base.BrainPixelsStart > 0;
    }

    void World::UpdateBrains()
    {
        m_BrainAlarmTimer -= FixedTimeStep;

        for (int team = 0; team < TeamCount; team++)
        {
            TeamBase& base = m_Bases[team];

            if (!base.Exists || !base.BrainAlive)
                continue;

            int pixels = 0;

            for (int y = base.BrainMinY; y <= base.BrainMaxY; y++)
            {
                for (int x = base.BrainMinX; x <= base.BrainMaxX; x++)
                {
                    if (m_Terrain.GetMaterial(x, y) == Material::Brain)
                        pixels++;
                }
            }

            if (pixels < base.BrainPixels && m_BrainAlarmTimer <= 0.0f)
            {
                SimEvent alarm;
                alarm.Type = SimEventType::BrainUnderAttack;
                alarm.Value = team;
                alarm.X = base.GetBrainCenterX();
                alarm.Y = base.GetBrainCenterY();
                Emit(alarm);

                m_BrainAlarmTimer = 4.0f;
            }

            base.BrainPixels = pixels;

            if (static_cast<float>(pixels) >
                BrainDeathThreshold * static_cast<float>(base.BrainPixelsStart))
                continue;

            // Dead: what's left of it bursts.
            base.BrainAlive = false;

            for (int y = base.BrainMinY; y <= base.BrainMaxY; y++)
            {
                for (int x = base.BrainMinX; x <= base.BrainMaxX; x++)
                {
                    if (m_Terrain.GetMaterial(x, y) != Material::Brain)
                        continue;

                    m_Terrain.SetMaterial(x, y, Material::Air);

                    if ((x + y) % 3 == 0)
                    {
                        m_Particles.SpawnBlood(
                            static_cast<float>(x), static_cast<float>(y),
                            RandomUnit() * 160.0f, -80.0f + RandomUnit() * 120.0f);
                    }
                }
            }

            EmitSound(Sfx::Gib);
            EmitSound(Sfx::Explosion, 0.8f);

            SimEvent destroyed;
            destroyed.Type = SimEventType::BrainDestroyed;
            destroyed.Value = team;
            destroyed.X = base.GetBrainCenterX();
            destroyed.Y = base.GetBrainCenterY();
            Emit(destroyed);

            if (m_Outcome == MatchOutcome::InProgress)
            {
                m_Outcome = team == 0 ? MatchOutcome::Defeat : MatchOutcome::Victory;
                m_Stats.EndTick = m_Tick;

                SimEvent over;
                over.Type = SimEventType::MatchOver;
                over.Value = team == 0 ? 1 : 0;
                Emit(over);
            }
        }
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

    void World::EmitSoundAt(Sfx sound, float volume, float x, float y)
    {
        SimEvent event;
        event.Type = SimEventType::Sound;
        event.Sound = sound;
        event.Volume = volume;
        event.Positional = true;
        event.X = x;
        event.Y = y;
        Emit(event);
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
        if (m_Bases[0].Exists)
            return m_Bases[0].X + PlayerSpawnOffsets[playerSlot % MaxPlayers];

        return static_cast<float>(m_Terrain.GetWidth()) * 0.5f +
            static_cast<float>(playerSlot) * 48.0f;
    }

    void World::RespawnPlayer(int slot)
    {
        PlayerSlot& player = m_Players[slot];

        // A fresh standard body with the standard kit.
        player.Body->SetBodyClass(BodyClass::Medium);
        player.Body->ResetVitals();
        GiveStarterKit(*player.Body);

        // Inside the bunker when there is one (search from below its
        // roof, not from the sky). The ground there may have been blown
        // up, so look sideways, then from the sky, for room to stand.
        const int bunkerSearch = m_Bases[0].Exists
            ? m_Bases[0].FloorY - BunkerInnerHeight + 4
            : 0;

        const float spawnX = GetSpawnX(slot);
        bool placed = false;

        for (int pass = 0; pass < 2 && !placed; pass++)
        {
            const int searchFrom = pass == 0 ? bunkerSearch : 0;

            for (int attempt = 0; attempt < 12 && !placed; attempt++)
            {
                const float offset = static_cast<float>((attempt + 1) / 2) * 18.0f *
                    (attempt % 2 == 0 ? 1.0f : -1.0f);

                player.Body->Spawn(m_Terrain, spawnX + offset, searchFrom);
                placed = player.Body->HasRoom(m_Terrain);
            }
        }

        if (!placed)
            player.Body->Spawn(m_Terrain, spawnX);
        player.Gibbed = false;
        player.WasGrounded = true;
        player.PrevFallSpeed = 0.0f;
    }

    int World::AllyIndexNearest(float x, float y) const
    {
        int best = -1;
        float bestDistSquared = 0.0f;

        for (int i = 0; i < static_cast<int>(m_Allies.size()); i++)
        {
            const Actor& body = *m_Allies[static_cast<std::size_t>(i)].Body;

            if (!body.IsAlive())
                continue;

            const float dx = body.GetCenterX() - x;
            const float dy = body.GetCenterY() - y;
            const float distSquared = dx * dx + dy * dy;

            if (best < 0 || distSquared < bestDistSquared)
            {
                best = i;
                bestDistSquared = distSquared;
            }
        }

        return best;
    }

    void World::SwitchBody(int slot, bool fromDeadBody)
    {
        PlayerSlot& player = m_Players[slot];
        Actor& body = *player.Body;

        // From a corpse: the nearest survivor. Otherwise cycle through
        // the team in order.
        const int index = fromDeadBody
            ? AllyIndexNearest(body.GetCenterX(), body.GetCenterY())
            : (m_Allies.empty() ? -1 : 0);

        if (index < 0)
            return;

        // Bodies trade places by value, so the player's Actor object (and
        // every reference to it) stays put while what it holds changes.
        Ally& ally = m_Allies[static_cast<std::size_t>(index)];
        std::swap(body, *ally.Body);

        if (fromDeadBody)
        {
            // The ally entry now holds the corpse; it's already been
            // gibbed, so just drop it.
            m_Allies.erase(m_Allies.begin() + index);
        }
        else
        {
            // The body left behind gets a fresh AI and goes to the back
            // of the line, so repeated switching visits everyone.
            Ally left = std::move(ally);
            left.Brain = AIController();
            left.Brain.Seed(left.Body->GetId());
            left.Body->SelectBestWeapon();

            m_Allies.erase(m_Allies.begin() + index);
            m_Allies.push_back(std::move(left));
        }

        player.Gibbed = false;
        player.RespawnTimer = 0.0f;
        player.WasGrounded = body.IsGrounded();
        player.PrevFallSpeed = 0.0f;
        player.PrevLegPlanted[0] = true;
        player.PrevLegPlanted[1] = true;

        SimEvent switched;
        switched.Type = SimEventType::BodySwitched;
        switched.Player = slot;
        switched.X = body.GetCenterX();
        switched.Y = body.GetCenterY();
        Emit(switched);

        EmitSoundAt(Sfx::Reload, 0.6f, body.GetCenterX(), body.GetCenterY());
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

        m_Players.push_back(std::move(player));
        RespawnPlayer(slot);
        return slot;
    }

    void World::GiveStarterKit(Actor& body)
    {
        body.SetWeaponDef(m_Loadout[0]);

        for (int i = 1; i < LoadoutSize; i++)
            body.AddItem(m_Loadout[i]);

        body.SelectItem(0);
    }

    void World::SpawnEnemy(float x, bool attacker, BodyClass bodyClass, const WeaponDef* weapon)
    {
        if (static_cast<int>(m_Enemies.size()) >= MaxEnemies)
            return;

        Enemy enemy;
        enemy.Body = std::make_unique<Actor>();
        InitActor(*enemy.Body, 1);
        enemy.Body->SetTint(255, 118, 106);
        enemy.Body->SetBodyClass(bodyClass);
        enemy.Body->SetWeaponDef(weapon ? weapon : m_EnemyWeapon);

        // Attackers bring a digger to tunnel through what they can't
        // jet over.
        if (attacker)
        {
            enemy.Body->AddItem(m_Digger);
            enemy.Body->SelectItem(0);
        }

        enemy.LastHealth = enemy.Body->GetHealth();

        // Inside the enemy bunker counts as "from below its roof".
        const TeamBase& base = m_Bases[1];
        const bool inBunker = base.Exists &&
            std::fabs(x - base.X) < static_cast<float>(BunkerHalfInner);

        enemy.Body->Spawn(m_Terrain, x,
            inBunker ? base.FloorY - BunkerInnerHeight + 4 : 0);

        enemy.Brain.Seed(enemy.Body->GetId());
        enemy.Attacker = attacker;
        m_Enemies.push_back(std::move(enemy));
    }

    int World::SpawnEnemyDrop(int maxCount)
    {
        const TeamBase& base = m_Bases[1];
        const float width = static_cast<float>(m_Terrain.GetWidth());

        // The enemy commander shops like a player: each body costs its
        // build plus its weapon, paid from the team's gold. What it
        // wants gets nastier as the match goes on; what it gets depends
        // on what it can afford.
        int& gold = m_TeamGold[1];
        int count = 0;

        auto priceOf = [&](BodyClass build, const WeaponDef* weapon)
        {
            return GetBodyClassInfo(build).Cost + (weapon ? weapon->Cost : 0);
        };

        for (int i = 0; i < maxCount; i++)
        {
            BodyClass build = BodyClass::Medium;
            const WeaponDef* weapon = nullptr;

            if (m_Wave >= 2 && i % 3 == 2)
            {
                build = BodyClass::Heavy;

                if (m_Wave >= 3 + (m_Difficulty == 0 ? 2 : 0))
                    weapon = m_Rocket;
            }
            else if (m_Wave >= 2 && i % 4 == 1)
            {
                build = BodyClass::Light;
                weapon = (m_Wave >= 4 && (m_Wave + i) % 2 == 0) ? m_Flamer : m_Shotgun;
            }

            // Can't afford the fancy one: settle for a plain rifleman.
            if (priceOf(build, weapon) > gold)
            {
                build = BodyClass::Medium;
                weapon = nullptr;
            }

            if (priceOf(build, weapon) > gold)
                break;

            gold -= priceOf(build, weapon);

            // Land on the near side of their bunker, spread out a bit.
            const float targetX = std::clamp(
                base.X - 220.0f - static_cast<float>(count) * 36.0f + RandomUnit() * 14.0f,
                40.0f, width - 40.0f);

            m_Deliveries.Order(
                DeliveryKind::Reinforcement,
                width + 160.0f + static_cast<float>(count) * 90.0f,
                -1.0f,
                targetX,
                weapon,
                -1,
                1,
                static_cast<int>(build));

            count++;
        }

        // Air support from the middle of the match on, with what's left.
        const int wantDrones = m_Wave >= 3 ? std::min(1 + (m_Wave - 3) / 3 + (m_Difficulty == 2 ? 1 : 0), 3) : 0;
        int drones = 0;

        for (int i = 0; i < wantDrones && gold >= DroneCost; i++)
        {
            gold -= DroneCost;

            m_Deliveries.Order(
                DeliveryKind::Drone,
                width + 240.0f + static_cast<float>(count + i) * 90.0f,
                -1.0f,
                std::clamp(base.X - 160.0f - static_cast<float>(i) * 50.0f, 40.0f, width - 40.0f),
                nullptr,
                -1,
                1);

            drones++;
        }

        if (count + drones > 0)
        {
            SimEvent incoming;
            incoming.Type = SimEventType::EnemyDropIncoming;
            incoming.Value = count;
            incoming.X = base.X;
            Emit(incoming);
        }

        return count + drones;
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

            SpawnEnemy(x, true);
        }
    }

    // weapon: null falls back to the default rifleman loadout (SMG).
    // Every ordered body also packs a shovel, for whoever drives it.
    void World::SpawnAlly(float x, const WeaponDef* weapon, BodyClass bodyClass)
    {
        if (static_cast<int>(m_Allies.size()) >= MaxAllies)
            return;

        Ally ally;
        ally.Body = std::make_unique<Actor>();
        InitActor(*ally.Body, 0);
        ally.Body->SetTint(120, 170, 255);
        ally.Body->SetBodyClass(bodyClass);
        ally.Body->SetWeaponDef(weapon ? weapon : m_AllyWeapon);
        ally.Body->AddItem(m_Shovel);
        ally.Body->SelectBestWeapon();
        ally.Body->Spawn(m_Terrain, x);
        ally.Brain.Seed(ally.Body->GetId());
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
        for (const OrderBody& body : order.Bodies)
        {
            total += GetBodyClassInfo(static_cast<BodyClass>(body.Class)).Cost;

            if (body.Weapon && body.Weapon != m_AllyWeapon)
                total += body.Weapon->Cost;
        }

        total += std::max(0, order.Drones) * DroneCost;

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

        const Actor& buyer = *m_Players[player].Body;

        if (order.Bodies.empty() &&
            order.LooseWeapons.empty() &&
            order.SupplyCrates <= 0 &&
            order.Drones <= 0)
            return false;

        for (const OrderBody& body : order.Bodies)
        {
            if (body.Class >= static_cast<std::uint8_t>(BodyClass::Count))
                return false;
        }

        // No paying for drones there's no room to fly.
        int ownDrones = 0;

        for (const Drone& drone : m_Drones)
            ownDrones += drone.Team == 0 ? 1 : 0;

        if (order.Drones < 0 || ownDrones + order.Drones > MaxDronesPerTeam)
            return false;

        const int cost = PriceOrder(order);

        if (m_TeamGold[0] < cost)
            return false;

        m_TeamGold[0] -= cost;

        // Drops land on the buyer, or at home if they're between bodies.
        const float dropX = buyer.IsAlive() || !m_Bases[0].Exists
            ? buyer.GetCenterX()
            : m_Bases[0].X + 150.0f;

        auto queueDelivery = [&](DeliveryKind kind, const WeaponDef* weapon, int bodyClass = 1)
        {
            const bool fromLeft = (m_Rng() % 2u) == 0u;
            const float shipDir = fromLeft ? 1.0f : -1.0f;
            const float shipOrigin = fromLeft
                ? dropX - ShipSpawnDistance
                : dropX + ShipSpawnDistance;

            m_Deliveries.Order(
                kind, shipOrigin, shipDir, dropX, weapon, player, 0, bodyClass);
        };

        for (const OrderBody& body : order.Bodies)
            queueDelivery(DeliveryKind::Reinforcement, body.Weapon, body.Class);

        for (int i = 0; i < order.Drones; i++)
            queueDelivery(DeliveryKind::Drone, nullptr);

        m_Stats.BodiesOrdered += static_cast<int>(order.Bodies.size());

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

        // Standing orders for the team's AI, from whoever gives them.
        if (input.AllyOrder >= 0 &&
            input.AllyOrder < static_cast<std::int8_t>(AllyOrder::Count))
        {
            m_AllyOrder = static_cast<AllyOrder>(input.AllyOrder);
            m_AllyOrderPlayer = slot;
            EmitSound(Sfx::Reload, 0.5f);
        }

        if (!player.IsAlive())
        {
            state.RespawnTimer -= FixedTimeStep;

            // With the brain gone there's nothing to come back to.
            if (m_Outcome == MatchOutcome::Defeat)
                return;

            if (!m_Allies.empty() &&
                state.RespawnTimer <= RespawnTime - SwitchOnDeathDelay)
            {
                SwitchBody(slot, true);
            }
            else if (state.RespawnTimer <= 0.0f)
            {
                RespawnPlayer(slot);
            }

            return;
        }

        if (input.SwitchBodyPressed)
            SwitchBody(slot, false);

        // Switching between the items this body carries, and reload.
        if (input.SelectSlot >= 0 &&
            input.SelectSlot < player.GetItemCount() &&
            input.SelectSlot != player.GetSelectedItem())
        {
            player.SelectItem(input.SelectSlot);
            EmitSoundAt(Sfx::Reload, 0.7f, player.GetCenterX(), player.GetCenterY());
        }

        if (input.ReloadPressed)
        {
            player.GetWeapon().StartReload();
            EmitSoundAt(Sfx::Reload, 1.0f, player.GetCenterX(), player.GetCenterY());
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
            EmitSoundAt(Sfx::Jump, 0.5f, player.GetCenterX(), player.GetCenterY());

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
            EmitSoundAt(Sfx::Land, 0.6f, player.GetCenterX(), player.GetCenterY());

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
            EmitSoundAt(Sfx::JetBurst, 0.55f, player.GetCenterX(), player.GetCenterY());
            state.JetSoundTimer = 0.15f;
        }

        player.GetWeapon().Update(FixedTimeStep);
        player.TakeShotsFired(); // the player's own shots are voiced below

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

                FlushLaunch(player);

                if (def.Kind == WeaponKind::Gun || def.Kind == WeaponKind::Launcher)
                {
                    SimEvent fired;
                    fired.Type = SimEventType::GunFired;
                    fired.Player = slot;
                    Emit(fired);

                    // (Rockets are voiced by FlushLaunch.)
                    if (def.Kind != WeaponKind::Launcher)
                        EmitSoundAt(FireSfxFor(def), 0.8f, player.GetCenterX(), player.GetCenterY());
                }
                else if (state.ToolSfxTimer <= 0.0f)
                {
                    // Dig tools and flamers tick fast; don't machine-gun
                    // the sound.
                    EmitSoundAt(FireSfxFor(def), 0.7f, player.GetCenterX(), player.GetCenterY());
                    state.ToolSfxTimer = def.Kind == WeaponKind::Flamer ? 0.18f : 0.1f;
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
                player.GetAimDirY() * 520.0f - 60.0f,
                player.GetTeam());

            state.GrenadeCooldown = GrenadeCooldownTime;

            EmitSoundAt(Sfx::Throw, 0.7f, player.GetCenterX(), player.GetCenterY());
        }
    }

    void World::ApplyTerrainHazards(Actor& actor)
    {
        if (!actor.IsAlive())
            return;

        const float cx = actor.GetCenterX();
        const float feetY = actor.GetY() + actor.GetHeight() - 2.0f;

        auto sample = [&](float x, float y)
        {
            return m_Terrain.GetMaterial(
                static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
        };

        const Material atCenter = sample(cx, actor.GetCenterY());
        const Material atFeet = sample(cx, feetY);
        const Material atHead = sample(cx, actor.GetY() + 4.0f);

        // Lava burns whatever touches it - fast.
        if (atFeet == Material::Lava || atCenter == Material::Lava || atHead == Material::Lava)
        {
            if (m_Tick % 3u == 0u)
                actor.TakeDamage(2, 0.0f, -30.0f);

            if (m_Tick % 4u == 0u)
            {
                m_Particles.SpawnFire(
                    cx + RandomUnit() * 6.0f, feetY - 4.0f,
                    RandomUnit() * 20.0f, -60.0f - RandomUnit() * 30.0f);
                m_Particles.SpawnSmoke(cx, feetY - 10.0f, RandomUnit() * 15.0f, -40.0f);
            }
        }

        // Water: wading slows you, swimming keeps you from sinking fast.
        const int wet =
            (IsFluid(atFeet) ? 1 : 0) + (IsFluid(atCenter) ? 1 : 0) + (IsFluid(atHead) ? 1 : 0);

        if (wet > 0)
            actor.ApplySubmersion(wet >= 2 ? 0.93f : 0.97f, wet >= 2 ? 70.0f : 400.0f);

        // Lava sets you alight; water puts you out.
        if (atFeet == Material::Lava || atCenter == Material::Lava)
            actor.Ignite(2.0f);

        const bool doused =
            (IsFluid(atCenter) && atCenter != Material::Lava) ||
            (IsFluid(atFeet) && atFeet != Material::Lava);

        if (doused && actor.IsBurning())
        {
            actor.Extinguish();
            m_Particles.SpawnSmoke(cx, actor.GetCenterY(), RandomUnit() * 20.0f, -50.0f);
        }

        if (actor.IsBurning())
        {
            if (m_Tick % 8u == 0u)
                actor.TakeDamage(1, 0.0f, 0.0f);

            if (m_Tick % 6u == 0u)
                m_Particles.SpawnSmoke(cx + RandomUnit() * 6.0f, actor.GetY(), RandomUnit() * 10.0f, -45.0f);
        }
    }

    void World::AiShotSound(Actor& actor)
    {
        if (actor.TakeShotsFired() <= 0 || !actor.GetWeapon().GetDef())
            return;

        const WeaponDef& def = *actor.GetWeapon().GetDef();

        // Rockets have their own launch sound; flame only now and then.
        if (def.Kind == WeaponKind::Launcher ||
            (def.Kind == WeaponKind::Flamer && m_Tick % 20u != 0u))
            return;

        EmitSoundAt(FireSfxFor(def), 0.55f, actor.GetCenterX(), actor.GetCenterY());
    }

    void World::FlushLaunch(Actor& actor)
    {
        PendingLaunch launch;

        if (!actor.GetWeapon().TakeLaunch(launch))
            return;

        m_Grenades.Launch(
            launch.X, launch.Y, launch.VelX, launch.VelY,
            actor.GetTeam(), actor.GetId(), launch.Radius, launch.Damage);

        EmitSoundAt(Sfx::Rocket, 0.8f, launch.X, launch.Y);
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

        // --- Enemies: fight the nearest of the players' side; attackers
        // march on the players' brain, guards hold their own base ---
        const TeamBase& home = m_Bases[0];
        const TeamBase& enemyBase = m_Bases[1];

        for (std::size_t i = 0; i < m_Enemies.size(); i++)
        {
            Enemy& enemy = m_Enemies[i];
            const float x = enemy.Body->GetCenterX();
            const float y = enemy.Body->GetCenterY();

            AIOrders orders;

            if (m_Mode == GameMode::Survival)
            {
                orders.Target = NearestLivingPlayer(x, y);
            }
            else
            {
                orders.Target = NearestOfTeam(0, x, y);

                if (enemy.Attacker && home.BrainAlive)
                {
                    orders.HasGoal = true;
                    orders.GoalX = home.GetBrainCenterX();
                    orders.HasPoint = true;
                    orders.PointX = home.GetBrainCenterX();
                    orders.PointY = home.GetBrainCenterY();
                }
                else if (enemyBase.Exists)
                {
                    orders.HasGoal = true;
                    orders.GoalX = enemyBase.X - 150.0f - static_cast<float>(i % 3) * 40.0f;
                }
            }

            enemy.Brain.Update(
                *enemy.Body, orders, m_Terrain, m_Particles, m_Terrain, FixedTimeStep);
            FlushLaunch(*enemy.Body);
            AiShotSound(*enemy.Body);
        }

        // --- Allies: fight the nearest enemy, otherwise follow the
        // team's standing order ---
        for (std::size_t i = 0; i < m_Allies.size(); i++)
        {
            Ally& ally = m_Allies[i];
            const float x = ally.Body->GetCenterX();
            const float y = ally.Body->GetCenterY();
            const float spread = static_cast<float>(i) * 26.0f;

            AIOrders orders;
            orders.Target = NearestOfTeam(1, x, y);

            AllyOrder order = m_AllyOrder;

            // Without a base there's nothing to defend: stick with people.
            if (order == AllyOrder::Defend && !home.Exists)
                order = AllyOrder::Follow;

            if (order == AllyOrder::Attack && !enemyBase.BrainAlive)
                order = AllyOrder::Defend;

            switch (order)
            {
            case AllyOrder::Defend:
                orders.HasGoal = true;
                orders.GoalX = home.X + 160.0f + spread;
                break;

            case AllyOrder::Follow:
            {
                const int leader = std::clamp(m_AllyOrderPlayer, 0, GetPlayerCount() - 1);

                if (GetPlayerCount() > 0 && m_Players[static_cast<std::size_t>(leader)].Body->IsAlive())
                {
                    const Actor& body = *m_Players[static_cast<std::size_t>(leader)].Body;
                    orders.HasGoal = true;
                    orders.GoalX = body.GetCenterX() - body.GetFacingDir() * (40.0f + spread);
                }
                break;
            }

            case AllyOrder::Attack:
                orders.HasGoal = true;
                orders.GoalX = enemyBase.GetBrainCenterX();
                orders.HasPoint = true;
                orders.PointX = enemyBase.GetBrainCenterX();
                orders.PointY = enemyBase.GetBrainCenterY();
                break;

            default:
                break;
            }

            ally.Brain.Update(
                *ally.Body, orders, m_Terrain, m_Particles, m_Terrain, FixedTimeStep);
            FlushLaunch(*ally.Body);
            AiShotSound(*ally.Body);
        }

        // --- Drones ---
        UpdateDrones();

        // --- Lava and water ---
        for (int i = 0; i < actorCount; i++)
            ApplyTerrainHazards(*actorPtrs[i]);

        // --- Grenades and particles ---
        {
            // Rockets go off near drones too (team rides in Damage).
            std::vector<Blast> proximity;

            for (const Drone& drone : m_Drones)
                proximity.push_back({ drone.X, drone.Y, 9.0f, drone.Team });

            m_Grenades.SetProximityTargets(proximity);
        }

        m_Grenades.Update(
            m_Terrain, m_Particles, actorPtrs, actorCount, FixedTimeStep);

        if (m_Grenades.ExplodedThisFrame())
        {
            for (const Blast& at : m_Grenades.GetBlasts())
                EmitSoundAt(Sfx::Explosion, 1.0f, at.X, at.Y);

            SimEvent blast;
            blast.Type = SimEventType::Explosion;

            if (!m_Grenades.GetBlasts().empty())
            {
                blast.X = m_Grenades.GetBlasts().front().X;
                blast.Y = m_Grenades.GetBlasts().front().Y;
            }

            Emit(blast);
        }

        {
            std::vector<HitCircle> circles;

            for (const Drone& drone : m_Drones)
                circles.push_back({ drone.X, drone.Y, 8.0f, drone.Id, drone.Team });

            m_Particles.SetHitCircles(circles);
        }

        m_GoldCredits.clear();
        m_Particles.Update(
            m_Terrain, FixedTimeStep, actorPtrs, actorCount, &m_GoldCredits);

        DamageDrones(actorPtrs, actorCount);

        // Gold mined by bullets goes to whoever fired them - if they
        // still exist.
        for (const GoldCredit& credit : m_GoldCredits)
        {
            if (Actor* owner = FindActor(credit.OwnerId))
                owner->AddGold(credit.Amount);
        }

        // ...and from every body into its team's funds.
        for (PlayerSlot& player : m_Players)
            CollectGold(*player.Body);

        for (Enemy& enemy : m_Enemies)
            CollectGold(*enemy.Body);

        for (Ally& ally : m_Allies)
            CollectGold(*ally.Body);

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
                m_Stats.BodiesLost++;
                m_TeamGold[1] += KillBounty;
                player.Body->Gib(m_Particles);
                EmitSoundAt(Sfx::Gib, 1.0f, player.Body->GetCenterX(), player.Body->GetCenterY());
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
                EmitSoundAt(Sfx::Gib, 0.8f, body.GetCenterX(), body.GetCenterY());

                m_TeamGold[0] += KillBounty;
                m_Stats.EnemiesKilled++;

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
                m_Stats.BodiesLost++;
                m_TeamGold[1] += KillBounty;
                m_Allies[i].Body->Gib(m_Particles);
                EmitSoundAt(Sfx::Gib, 0.8f, m_Allies[i].Body->GetCenterX(), m_Allies[i].Body->GetCenterY());

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
                EmitSoundAt(Sfx::Reload, 0.9f, pickup.X, pickup.Y);

                m_HealthPickups[i] = m_HealthPickups.back();
                m_HealthPickups.pop_back();
            }
            else
            {
                i++;
            }
        }

        UpdateBrains();

        // --- Enemy reinforcements: drop ships to their base on a timer,
        // bigger as the match goes on ---
        if (m_Mode == GameMode::BrainAssault &&
            m_BotsEnabled &&
            m_Outcome == MatchOutcome::InProgress &&
            enemyBase.BrainAlive)
        {
            m_WaveTimer -= FixedTimeStep;

            // Their income: a steady trickle, more on harder settings.
            static constexpr int IncomeTicks[3] = { 22, 15, 10 }; // ticks per gold
            if (m_Tick % static_cast<std::uint64_t>(IncomeTicks[m_Difficulty]) == 0u)
                m_TeamGold[1]++;

            const int room = MaxEnemies - static_cast<int>(m_Enemies.size());

            if (m_WaveTimer <= 0.0f && (room <= 0 || m_TeamGold[1] < GetBodyClassInfo(BodyClass::Medium).Cost))
            {
                // Full up or broke: look again shortly.
                m_WaveTimer = 4.0f;
            }
            else if (m_WaveTimer <= 0.0f)
            {
                m_Wave++;

                const int size = 2 + m_Wave / 2 + (m_Difficulty - 1);
                SpawnEnemyDrop(std::min(room, std::clamp(size, 1, 4 + m_Difficulty * 2)));

                static constexpr float Interval[3] = { 40.0f, 34.0f, 28.0f };
                static constexpr float MinInterval[3] = { 26.0f, 18.0f, 13.0f };

                m_WaveTimer = std::max(MinInterval[m_Difficulty],
                    Interval[m_Difficulty] - static_cast<float>(m_Wave));

                SimEvent wave;
                wave.Type = SimEventType::WaveStarted;
                wave.Value = m_Wave;
                Emit(wave);
            }
        }

        // --- Waves ---
        if (m_Mode == GameMode::Survival && m_BotsEnabled && m_Enemies.empty())
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
                EmitSoundAt(Sfx::Land, 0.7f, landed.front()->PayloadX, landed.front()->PayloadY);

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
                            owner.RefillAll();
                        }
                    }
                    break;

                case DeliveryKind::Reinforcement:
                    if (delivery->Team == 1)
                    {
                        SpawnEnemy(delivery->PayloadX, true,
                            static_cast<BodyClass>(delivery->BodyClass), delivery->Weapon);
                    }
                    else
                    {
                        SpawnAlly(delivery->PayloadX, delivery->Weapon,
                            static_cast<BodyClass>(delivery->BodyClass));
                    }
                    break;

                case DeliveryKind::Drone:
                    SpawnDrone(delivery->PayloadX, delivery->PayloadY - 24.0f, delivery->Team);
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
                m_Players[taker].Body->AddItem(pickup.Weapon);

                EmitSoundAt(Sfx::Reload, 0.8f, pickup.X, pickup.Y);

                m_WeaponPickups[i] = m_WeaponPickups.back();
                m_WeaponPickups.pop_back();
            }
            else
            {
                i++;
            }
        }

        // --- Living terrain: sand falls, water and lava flow ---
        m_Terrain.Simulate();

        m_SteamSoundTimer -= FixedTimeStep;

        for (const Terrain::DestroyedPixel& steam : m_Terrain.TakeReactions())
        {
            m_Particles.SpawnSmoke(steam.X, steam.Y, RandomUnit() * 20.0f, -50.0f);

            if (m_SteamSoundTimer <= 0.0f)
            {
                EmitSoundAt(Sfx::JetBurst, 0.3f, steam.X, steam.Y);
                m_SteamSoundTimer = 0.25f;
            }
        }

        m_Tick++;
    }

    void World::SpawnDrone(float x, float y, int team)
    {
        int ofTeam = 0;

        for (const Drone& other : m_Drones)
            ofTeam += other.Team == team ? 1 : 0;

        if (static_cast<int>(m_Drones.size()) >= MaxDrones || ofTeam >= MaxDronesPerTeam)
            return;

        Drone drone;
        drone.Id = m_NextActorId++;
        drone.Team = team;
        drone.X = x;
        drone.Y = y;
        drone.Health = DroneHealth;
        drone.PauseTimer = 0.4f;
        drone.Phase = static_cast<float>(drone.Id % 17u) * 0.37f;
        m_Drones.push_back(drone);
    }

    void World::UpdateDrones()
    {
        constexpr float HoverHeight = 85.0f;
        constexpr float SightRange = 480.0f;
        constexpr float MaxSpeedX = 190.0f;
        constexpr float MaxSpeedY = 160.0f;
        constexpr float GunVelocity = 820.0f;

        const float dt = FixedTimeStep;
        const float width = static_cast<float>(m_Terrain.GetWidth());
        const TeamBase& home = m_Bases[0];
        const TeamBase& enemyBase = m_Bases[1];

        auto clearLine = [&](float x0, float y0, float x1, float y1)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const int steps = static_cast<int>(std::sqrt(dx * dx + dy * dy) / 4.0f) + 1;

            for (int i = 1; i < steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);

                if (m_Terrain.IsSolid(x0 + dx * t, y0 + dy * t))
                    return false;
            }

            return true;
        };

        for (std::size_t i = 0; i < m_Drones.size(); i++)
        {
            Drone& drone = m_Drones[i];

            drone.Phase += dt;
            drone.Cooldown -= dt;
            drone.HurtFlash = std::max(0.0f, drone.HurtFlash - dt);
            drone.Firing = false;

            const int foe = 1 - drone.Team;

            // --- Where it wants to be ---
            float goalX = drone.X;

            if (drone.Team == 1)
            {
                if (home.BrainAlive)
                    goalX = home.GetBrainCenterX() + 140.0f + static_cast<float>(i % 3) * 30.0f;
                else if (Actor* prey = NearestOfTeam(0, drone.X, drone.Y))
                    goalX = prey->GetCenterX();
            }
            else
            {
                const float spread = static_cast<float>(i % 4) * 30.0f;

                switch (m_AllyOrder)
                {
                case AllyOrder::Attack:
                    goalX = enemyBase.BrainAlive
                        ? enemyBase.GetBrainCenterX() - 160.0f - spread
                        : home.X + 120.0f + spread;
                    break;

                case AllyOrder::Follow:
                {
                    const int leader = std::clamp(m_AllyOrderPlayer, 0, std::max(0, GetPlayerCount() - 1));

                    if (GetPlayerCount() > 0 && m_Players[static_cast<std::size_t>(leader)].Body->IsAlive())
                        goalX = m_Players[static_cast<std::size_t>(leader)].Body->GetCenterX() - 30.0f + spread;
                    break;
                }

                default:
                    goalX = home.Exists ? home.X + 120.0f + spread : drone.X;
                    break;
                }
            }

            // --- What it's shooting at: the nearest visible hostile ---
            float targetX = 0.0f;
            float targetY = 0.0f;
            bool hasTarget = false;
            float bestDistSquared = SightRange * SightRange;

            if (Actor* body = NearestOfTeam(foe, drone.X, drone.Y))
            {
                const float dx = body->GetCenterX() - drone.X;
                const float dy = body->GetCenterY() - drone.Y;
                const float distSquared = dx * dx + dy * dy;

                if (distSquared < bestDistSquared &&
                    clearLine(drone.X, drone.Y, body->GetCenterX(), body->GetCenterY()))
                {
                    hasTarget = true;
                    targetX = body->GetCenterX();
                    targetY = body->GetCenterY() - 4.0f;
                    bestDistSquared = distSquared;
                }
            }

            for (const Drone& other : m_Drones)
            {
                if (other.Team == drone.Team || other.Health <= 0)
                    continue;

                const float dx = other.X - drone.X;
                const float dy = other.Y - drone.Y;
                const float distSquared = dx * dx + dy * dy;

                if (distSquared < bestDistSquared && clearLine(drone.X, drone.Y, other.X, other.Y))
                {
                    hasTarget = true;
                    targetX = other.X;
                    targetY = other.Y;
                    bestDistSquared = distSquared;
                }
            }

            if (hasTarget)
            {
                // Hold a firing distance off the target.
                const float dx = targetX - drone.X;

                if (std::fabs(dx) > 230.0f)
                    goalX = targetX - (dx > 0.0f ? 200.0f : -200.0f);
                else if (std::fabs(dx) < 110.0f)
                    goalX = drone.X - (dx > 0.0f ? 60.0f : -60.0f);
                else
                    goalX = drone.X;
            }

            goalX += std::sin(drone.Phase * 0.7f) * 30.0f;
            goalX = std::clamp(goalX, 30.0f, width - 30.0f);

            // Hover a set height over whatever is below.
            float groundY = drone.Y + 400.0f;

            for (float probe = drone.Y + 6.0f; probe < drone.Y + 400.0f; probe += 3.0f)
            {
                if (m_Terrain.IsSolid(drone.X, probe))
                {
                    groundY = probe;
                    break;
                }
            }

            const float goalY = groundY - HoverHeight + std::sin(drone.Phase * 2.1f) * 6.0f;

            const float accelX = std::clamp((goalX - drone.X) * 2.5f, -420.0f, 420.0f);
            const float accelY = std::clamp((goalY - drone.Y) * 4.0f, -520.0f, 520.0f);

            const float damping = std::max(0.0f, 1.0f - 1.8f * dt);

            drone.VelX = std::clamp((drone.VelX + accelX * dt) * damping, -MaxSpeedX, MaxSpeedX);
            drone.VelY = std::clamp((drone.VelY + accelY * dt) * damping, -MaxSpeedY, MaxSpeedY);

            // Move a pixel at a time, sliding along terrain.
            const int steps = std::max(1, static_cast<int>(std::ceil(std::max(
                std::fabs(drone.VelX * dt), std::fabs(drone.VelY * dt)))));

            const float stepX = drone.VelX * dt / static_cast<float>(steps);
            const float stepY = drone.VelY * dt / static_cast<float>(steps);

            for (int step = 0; step < steps; step++)
            {
                if (!m_Terrain.IsSolid(drone.X + stepX + (stepX > 0.0f ? 6.0f : -6.0f), drone.Y))
                    drone.X += stepX;
                else
                    drone.VelX *= -0.3f;

                if (!m_Terrain.IsSolid(drone.X, drone.Y + stepY + (stepY > 0.0f ? 5.0f : -5.0f)))
                    drone.Y += stepY;
                else
                    drone.VelY *= -0.3f;
            }

            // Buried by falling sand: climb out.
            if (m_Terrain.IsSolid(drone.X, drone.Y))
                drone.Y -= 1.0f;

            drone.X = std::clamp(drone.X, 10.0f, width - 10.0f);
            drone.Y = std::max(drone.Y, -300.0f);

            // --- Shooting: aimed bursts ---
            if (!hasTarget)
            {
                drone.BurstTimer = 0.0f;
                continue;
            }

            const float aimDx = targetX - drone.X;
            const float aimDy = targetY - drone.Y;
            const float aimLength = std::sqrt(aimDx * aimDx + aimDy * aimDy) + 0.001f;

            drone.AimX = aimDx / aimLength;
            drone.AimY = aimDy / aimLength;

            if (drone.BurstTimer > 0.0f)
            {
                drone.BurstTimer -= dt;
                drone.Firing = true;

                if (drone.Cooldown <= 0.0f)
                {
                    drone.Cooldown = 1.0f / 6.0f;

                    const float spread = RandomUnit() * 0.06f;
                    const float dirX = drone.AimX - drone.AimY * spread;
                    const float dirY = drone.AimY + drone.AimX * spread;

                    m_Particles.SpawnBullet(
                        drone.X + dirX * 9.0f, drone.Y + 3.0f + dirY * 9.0f,
                        dirX * GunVelocity, dirY * GunVelocity,
                        6, 6.0f, drone.Id, drone.Team);
                    EmitSoundAt(Sfx::Shot, 0.4f, drone.X, drone.Y);
                }

                if (drone.BurstTimer <= 0.0f)
                    drone.PauseTimer = 0.6f + (RandomUnit() + 1.0f) * 0.3f;
            }
            else
            {
                drone.PauseTimer -= dt;

                if (drone.PauseTimer <= 0.0f)
                    drone.BurstTimer = 0.45f;
            }
        }
    }

    void World::DamageDrones(Actor* const* actors, int actorCount)
    {
        auto find = [&](std::uint32_t id) -> Drone*
        {
            for (Drone& drone : m_Drones)
            {
                if (drone.Id == id)
                    return &drone;
            }

            return nullptr;
        };

        for (const CircleHit& hit : m_Particles.GetCircleHits())
        {
            if (Drone* drone = find(hit.Id))
            {
                drone->Health -= hit.Damage + (hit.Fire ? 2 : 0);
                drone->HurtFlash = 0.1f;
            }
        }

        for (const Blast& blast : m_Grenades.GetBlasts())
        {
            for (Drone& drone : m_Drones)
            {
                float dx = drone.X - blast.X;
                float dy = drone.Y - blast.Y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                const float reach = blast.Radius * 2.2f;

                if (distance > reach)
                    continue;

                const float falloff = 1.0f - distance / reach;
                dx /= distance + 0.001f;
                dy /= distance + 0.001f;

                drone.Health -= static_cast<int>(static_cast<float>(blast.Damage) * falloff);
                drone.VelX += dx * 300.0f * falloff;
                drone.VelY += dy * 300.0f * falloff;
                drone.HurtFlash = 0.1f;
            }
        }

        for (std::size_t i = 0; i < m_Drones.size(); )
        {
            const Drone drone = m_Drones[i];

            if (drone.Health > 0)
            {
                i++;
                continue;
            }

            m_Drones.erase(m_Drones.begin() + static_cast<std::ptrdiff_t>(i));

            // A small pop: it was carrying a battery, not a warhead.
            Explode(m_Terrain, m_Particles, actors, actorCount,
                drone.X, drone.Y, 10.0f, 20, drone.Team);

            for (int k = 0; k < 6; k++)
            {
                m_Particles.SpawnGib(drone.X, drone.Y,
                    RandomUnit() * 160.0f, -120.0f + RandomUnit() * 80.0f,
                    70, 74, 84, 2);
            }

            EmitSoundAt(Sfx::DroneDown, 0.8f, drone.X, drone.Y);

            if (drone.Team == 1)
            {
                m_TeamGold[0] += KillBounty;
                m_Stats.EnemiesKilled++;

                SimEvent kill;
                kill.Type = SimEventType::EnemyKilled;
                kill.X = drone.X;
                kill.Y = drone.Y;
                Emit(kill);
            }
        }
    }

    void World::AddAlly(float x, const WeaponDef* weapon, BodyClass bodyClass)
    {
        SpawnAlly(x, weapon, bodyClass);
    }

    void World::AddDrone(float x, float y, int team)
    {
        SpawnDrone(x, y, team);
    }

    void World::SetAllyOrder(AllyOrder order)
    {
        m_AllyOrder = order;
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
        h.Int(static_cast<std::int64_t>(m_Outcome));
        h.Int(static_cast<std::int64_t>(m_AllyOrder));
        h.Int(m_AllyOrderPlayer);
        h.Int(m_Difficulty);
        h.Int(m_Stats.EnemiesKilled);
        h.Int(m_Stats.BodiesLost);

        for (int team = 0; team < TeamCount; team++)
        {
            h.Int(m_TeamGold[team]);
            h.Int(m_Bases[team].BrainPixels);
            h.Int(m_Bases[team].BrainAlive ? 1 : 0);
        }

        h.Bytes(
            m_Terrain.GetMaterialData(),
            static_cast<std::size_t>(m_Terrain.GetWidth()) *
                static_cast<std::size_t>(m_Terrain.GetHeight()));

        for (const PlayerSlot& player : m_Players)
            h.ActorState(*player.Body);

        h.Int(static_cast<std::int64_t>(m_Drones.size()));

        for (const Drone& drone : m_Drones)
        {
            h.Int(drone.Id);
            h.Int(drone.Health);
            h.Float(drone.X);
            h.Float(drone.Y);
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
    const std::vector<Drone>& World::GetDrones() const { return m_Drones; }
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

    GameMode World::GetMode() const { return m_Mode; }
    MatchOutcome World::GetOutcome() const { return m_Outcome; }
    const TeamBase& World::GetBase(int team) const { return m_Bases[std::clamp(team, 0, TeamCount - 1)]; }
    float World::GetWaveTimer() const { return m_WaveTimer; }
    AllyOrder World::GetAllyOrder() const { return m_AllyOrder; }
    const MatchStats& World::GetStats() const { return m_Stats; }
    int World::GetDifficulty() const { return m_Difficulty; }

    int World::GetTeamGold(int team) const
    {
        return m_TeamGold[std::clamp(team, 0, TeamCount - 1)];
    }

    void World::AddTeamGold(int team, int amount)
    {
        m_TeamGold[std::clamp(team, 0, TeamCount - 1)] += amount;
    }

    void World::CollectGold(Actor& body)
    {
        const int gold = body.GetGold();

        if (gold == 0)
            return;

        m_TeamGold[std::clamp(body.GetTeam(), 0, TeamCount - 1)] += gold;

        if (body.GetTeam() == 0 && gold > 0)
            m_Stats.GoldMined += gold;
        body.AddGold(-gold);
    }

    Actor* World::NearestOfTeam(int team, float x, float y)
    {
        Actor* nearest = nullptr;
        float nearestDistSquared = 0.0f;

        auto consider = [&](Actor& body)
        {
            if (!body.IsAlive() || body.GetTeam() != team)
                return;

            const float dx = body.GetCenterX() - x;
            const float dy = body.GetCenterY() - y;
            const float distSquared = dx * dx + dy * dy;

            if (!nearest || distSquared < nearestDistSquared)
            {
                nearest = &body;
                nearestDistSquared = distSquared;
            }
        };

        for (PlayerSlot& player : m_Players)
            consider(*player.Body);

        for (Enemy& enemy : m_Enemies)
            consider(*enemy.Body);

        for (Ally& ally : m_Allies)
            consider(*ally.Body);

        return nearest;
    }

    std::uint64_t World::GetTick() const
    {
        return m_Tick;
    }
}
