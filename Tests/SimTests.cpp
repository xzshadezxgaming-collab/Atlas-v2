// Headless simulation tests. No window, renderer, audio or input device:
// if any of these need one, the World has leaked presentation code.

#include "Sim/World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using namespace Atlas;

namespace
{
    int g_Failures = 0;

    void Check(bool condition, const std::string& what)
    {
        if (!condition)
        {
            std::printf("  FAIL: %s\n", what.c_str());
            g_Failures++;
        }
    }

    WorldConfig MakeConfig(unsigned int seed)
    {
        WorldConfig config;
        config.Width = 2048;
        config.Height = 1024;
        config.Seed = seed;
        config.Weapons = LoadWeaponDefs(std::string()); // built-in defaults
        return config;
    }

    // A busy, reproducible script: walks back and forth, jumps, jets,
    // crouches, sweeps its aim, fires, throws grenades and cycles through
    // every weapon (guns and both dig tools).
    PlayerInput ScriptedInput(const World& world, int slot, std::uint64_t tick)
    {
        const Actor& body = *world.GetPlayer(slot).Body;

        PlayerInput input;

        const std::uint64_t phase = (tick / 240) % 4;
        input.MoveX = (phase == 0 || phase == 1) ? 1.0f : -1.0f;
        if (phase == 3)
            input.MoveX = 0.0f;

        input.Jump = (tick % 150) < 4;
        input.JumpPressed = (tick % 150) == 0;
        input.Jetpack = (tick % 400) > 360;
        input.Crouch = (tick % 500) > 470;

        const float angle = static_cast<float>(tick) * 0.01f;
        input.AimX = body.GetCenterX() + std::cos(angle) * 120.0f;
        input.AimY = body.GetCenterY() + 40.0f + std::sin(angle) * 80.0f;

        input.Fire = (tick % 90) < 60;
        input.ThrowPressed = (tick % 300) == 17;
        input.ReloadPressed = (tick % 700) == 5;

        if (tick % 360 == 0)
            input.SelectSlot = static_cast<std::int8_t>((tick / 360) % std::max(1, body.GetItemCount()));

        return input;
    }

    void RunScripted(World& world, std::uint64_t ticks,
        const std::function<void(std::uint64_t)>& afterTick = nullptr)
    {
        std::vector<PlayerInput> inputs(world.GetPlayerCount());

        static const char* const Extras[] = { "Rocket Launcher", "Flamethrower", "Sniper" };

        for (std::uint64_t t = 0; t < ticks; t++)
        {
            // Hand out the fancier weapons, and put drones up on both
            // sides now and then, so every system gets exercised.
            if (t % 600 == 300)
            {
                for (int p = 0; p < world.GetPlayerCount(); p++)
                    world.GetPlayer(p).Body->AddItem(world.FindWeapon(Extras[(t / 600 + p) % 3]));
            }

            if (t % 2400 == 1200)
            {
                const Actor& body = *world.GetPlayer(0).Body;
                world.AddDrone(body.GetCenterX() - 40.0f, body.GetY() - 80.0f, 0);
                world.AddDrone(body.GetCenterX() + 300.0f, body.GetY() - 120.0f, 1);
            }

            for (int p = 0; p < world.GetPlayerCount(); p++)
                inputs[p] = ScriptedInput(world, p, t + static_cast<std::uint64_t>(p) * 97u);

            world.Step(inputs.data(), static_cast<int>(inputs.size()));

            if (afterTick)
                afterTick(t);
        }
    }

    void TestDeterminism()
    {
        std::printf("determinism: same seed + same inputs -> same state\n");

        World a;
        World b;
        Check(a.Create(MakeConfig(42u)), "world a created");
        Check(b.Create(MakeConfig(42u)), "world b created");
        a.AddPlayer();
        b.AddPlayer();

        Check(a.ComputeStateHash() == b.ComputeStateHash(), "initial hashes match");

        std::vector<std::uint64_t> hashesA;
        RunScripted(a, 120 * 60, [&](std::uint64_t t)
        {
            if (t % 60 == 0)
                hashesA.push_back(a.ComputeStateHash());
        });

        std::size_t index = 0;
        bool allMatch = true;
        std::uint64_t firstMismatch = 0;

        RunScripted(b, 120 * 60, [&](std::uint64_t t)
        {
            if (t % 60 == 0)
            {
                if (allMatch && hashesA[index] != b.ComputeStateHash())
                {
                    allMatch = false;
                    firstMismatch = t;
                }

                index++;
            }
        });

        Check(allMatch, "hashes diverged at tick " + std::to_string(firstMismatch));
        Check(a.GetWave() > 0, "waves spawned during the run");

        std::printf("  60s simulated, wave %d, %d particles, hash %016llx\n",
            a.GetWave(),
            a.GetParticles().GetActiveCount(),
            static_cast<unsigned long long>(a.ComputeStateHash()));
    }

    void TestSeedsDiffer()
    {
        std::printf("seeds: different seeds -> different worlds\n");

        World a;
        World b;
        a.Create(MakeConfig(1u));
        b.Create(MakeConfig(2u));

        Check(a.ComputeStateHash() != b.ComputeStateHash(), "terrain differs by seed");
    }

    void TestMultiplayerSoak()
    {
        // Four players and bots fighting for five simulated minutes.
        // Mostly here to give sanitizers (ASan/UBSan in CI) a long run
        // through every system: deaths mid-flight, respawns, removals.
        std::printf("soak: 4 players, 5 simulated minutes\n");

        World world;
        Check(world.Create(MakeConfig(7u)), "world created");

        for (int i = 0; i < World::MaxPlayers; i++)
            Check(world.AddPlayer() == i, "player slot " + std::to_string(i));

        Check(world.AddPlayer() == -1, "player cap enforced");

        int deaths = 0;
        bool wasAlive[World::MaxPlayers] = { true, true, true, true };

        RunScripted(world, 120 * 60 * 5, [&](std::uint64_t)
        {
            for (int p = 0; p < World::MaxPlayers; p++)
            {
                const bool alive = world.GetPlayer(p).Body->IsAlive();

                if (wasAlive[p] && !alive)
                    deaths++;

                wasAlive[p] = alive;
            }
        });

        Check(world.GetTick() == 120ull * 60ull * 5ull, "tick counter");
        std::printf("  wave %d, %d player deaths\n", world.GetWave(), deaths);
    }

    void TestOrdersAndIds()
    {
        std::printf("orders: priced by the World, delivered as allies\n");

        World world;
        world.Create(MakeConfig(99u));
        world.SetBotsEnabled(false);
        const int slot = world.AddPlayer();
        Actor& player = *world.GetPlayer(slot).Body;

        OrderRequest order;
        order.Bodies.push_back({ world.GetAllyWeapon(), 1 });
        order.SupplyCrates = 1;

        world.AddTeamGold(0, -world.GetTeamGold(0));
        Check(!world.SubmitOrder(slot, order), "unaffordable order refused");
        Check(world.GetTeamGold(0) == 0, "nothing spent on refusal");

        world.AddTeamGold(0, 1000);
        const int price = world.PriceOrder(order);
        Check(price == World::ReinforcementCost + World::SupplyCrateCost,
            "bundled weapon is free");
        Check(world.SubmitOrder(slot, order), "affordable order accepted");
        Check(world.GetTeamGold(0) == 1000 - price, "gold spent once");
        Check(!world.SubmitOrder(slot, OrderRequest{}), "empty order refused");

        for (int t = 0; t < 120 * 30 && world.GetAllies().empty(); t++)
            world.Step(nullptr, 0);

        Check(world.GetAllies().size() == 1, "reinforcement landed");

        if (!world.GetAllies().empty())
        {
            const Actor& ally = *world.GetAllies().front().Body;
            Check(ally.GetId() != 0 && ally.GetId() != player.GetId(), "ally has its own id");
            Check(world.FindActor(ally.GetId()) == &ally, "ids resolve");
        }

        Check(world.FindActor(0) == nullptr, "id 0 never resolves");
        Check(world.FindActor(123456) == nullptr, "unknown ids don't resolve");
    }
}

namespace
{
    void TestBrainAssault()
    {
        std::printf("brain assault: bases, friendly fire, switching, victory\n");

        World world;
        Check(world.Create(MakeConfig(5u)), "world created");
        world.SetBotsEnabled(false);
        const int slot = world.AddPlayer();

        const TeamBase& home = world.GetBase(0);
        const TeamBase& away = world.GetBase(1);
        Check(home.Exists && away.Exists, "both bases built");
        Check(home.BrainAlive && away.BrainAlive && home.BrainPixelsStart > 200,
            "both brains alive");
        Check(home.X < away.X, "players' base on the left");

        Actor& body = *world.GetPlayer(slot).Body;
        Check(std::fabs(body.GetCenterX() - home.X) < 100.0f, "player starts in their bunker");

        // Hosing our own brain with the SMG does nothing to it.
        PlayerInput input;
        input.AimX = home.GetBrainCenterX();
        input.AimY = home.GetBrainCenterY();
        input.Fire = true;

        for (int t = 0; t < 240; t++)
        {
            input.AimX = home.GetBrainCenterX();
            world.Step(&input, 1);
        }

        Check(home.BrainPixels == home.BrainPixelsStart, "own bullets spare own brain");

        // Order a body and hop into it.
        world.AddTeamGold(0, 1000);
        OrderRequest order;
        order.Bodies.push_back({ world.GetAllyWeapon(), 1 });
        Check(world.SubmitOrder(slot, order), "reinforcement ordered");

        for (int t = 0; t < 120 * 30 && world.GetAllies().empty(); t++)
            world.Step(nullptr, 0);

        Check(world.GetAllies().size() == 1, "reinforcement landed");

        if (world.GetAllies().size() == 1)
        {
            const std::uint32_t mine = body.GetId();
            const std::uint32_t theirs = world.GetAllies().front().Body->GetId();

            PlayerInput swap;
            swap.SwitchBodyPressed = true;
            world.Step(&swap, 1);

            Check(world.GetPlayer(slot).Body.get() == &body, "player's Actor object is stable");
            Check(body.GetId() == theirs, "player now drives the ally's body");
            Check(world.GetAllies().size() == 1 &&
                world.GetAllies().front().Body->GetId() == mine,
                "old body carries on as an ally");

            // Losing a body with a teammate around: jump straight into it.
            body.TakeDamage(9999, 0.0f, 0.0f);

            for (int t = 0; t < 240; t++)
                world.Step(nullptr, 0);

            Check(body.IsAlive() && body.GetId() == mine, "took over the survivor on death");
            Check(world.GetAllies().empty(), "the corpse isn't left as an ally");
        }

        // Standing orders reach the team.
        PlayerInput attack;
        attack.AllyOrder = static_cast<std::int8_t>(AllyOrder::Attack);
        world.Step(&attack, 1);
        Check(world.GetAllyOrder() == AllyOrder::Attack, "ally order set");

        // Wreck the enemy brain: the match is won.
        Terrain& terrain = world.GetTerrain();

        for (int y = away.BrainMinY; y <= away.BrainMaxY; y++)
        {
            for (int x = away.BrainMinX; x <= away.BrainMaxX; x++)
            {
                if (x % 4 != 0)
                    terrain.SetMaterial(x, y, Material::Air);
            }
        }

        bool sawDestroyed = false;
        bool sawOver = false;

        for (int t = 0; t < 4; t++)
        {
            world.Step(nullptr, 0);

            for (const SimEvent& event : world.GetEvents())
            {
                sawDestroyed |= event.Type == SimEventType::BrainDestroyed && event.Value == 1;
                sawOver |= event.Type == SimEventType::MatchOver && event.Value == 0;
            }
        }

        Check(!away.BrainAlive, "enemy brain dead");
        Check(world.GetOutcome() == MatchOutcome::Victory, "victory");
        Check(sawDestroyed && sawOver, "brain-destroyed and match-over events");
    }

    void TestEnemiesAdvance()
    {
        std::printf("enemy drops march on the players' base\n");

        World world;
        world.Create(MakeConfig(11u));
        world.AddPlayer();

        const TeamBase& home = world.GetBase(0);
        float closest = 1.0e9f;
        int drops = 0;

        for (int t = 0; t < 120 * 75; t++)
        {
            world.Step(nullptr, 0);

            for (const SimEvent& event : world.GetEvents())
                drops += event.Type == SimEventType::EnemyDropIncoming ? 1 : 0;

            for (const Enemy& enemy : world.GetEnemies())
            {
                if (enemy.Attacker)
                    closest = std::min(closest, std::fabs(enemy.Body->GetCenterX() - home.X));
            }
        }

        Check(drops >= 1, "enemy drop ships came");
        Check(closest < 700.0f, "attackers got within 700px of the players' bunker (closest " +
            std::to_string(static_cast<int>(closest)) + ")");
    }
}

namespace
{
    // Puts the player out in the open past their base, with one of the
    // enemy guards dragged over `gap` pixels to the right. Returns the
    // guard (null if there was none).
    Actor* SetUpDuel(World& world, int slot, float gap)
    {
        world.SetBotsEnabled(false);

        Actor& player = *world.GetPlayer(slot).Body;
        const float x = world.GetBase(0).X + 420.0f;

        // A flat, open arena so the terrain doesn't decide the fight.
        Terrain& terrain = world.GetTerrain();
        const int floorY = terrain.GetSurfaceY(static_cast<int>(x));

        for (int px = static_cast<int>(x) - 60; px <= static_cast<int>(x + gap) + 60; px++)
        {
            for (int py = floorY - 140; py < floorY; py++)
                terrain.SetMaterial(px, py, Material::Air);

            for (int py = floorY; py < floorY + 8; py++)
                terrain.SetMaterial(px, py, Material::Concrete);
        }

        player.Spawn(terrain, x);

        std::vector<Enemy>& enemies = world.GetEnemies();

        if (enemies.empty())
            return nullptr;

        // Only one guard in the duel.
        while (enemies.size() > 1)
            enemies.pop_back();

        Actor& guard = *enemies.front().Body;
        guard.Spawn(world.GetTerrain(), x + gap);
        return &guard;
    }

    int SelectWeapon(World& world, int slot, const std::string& name)
    {
        Actor& body = *world.GetPlayer(slot).Body;
        body.AddItem(world.FindWeapon(name));

        PlayerInput select;
        select.SelectSlot = static_cast<std::int8_t>(body.GetSelectedItem());
        world.Step(&select, 1);
        return body.GetSelectedItem();
    }

    void TestBodiesAndInventory()
    {
        std::printf("bodies: builds, armor, per-body inventories\n");

        {
            Actor heavy;
            heavy.SetBodyClass(BodyClass::Heavy);
            Check(heavy.GetMaxHealth() == 200 && heavy.GetHealth() == 200, "heavy has 200 health");
            heavy.TakeDamage(100, 0.0f, 0.0f);
            Check(heavy.GetHealth() == 130, "heavy armor soaks 30% of a hit");

            Actor scout;
            scout.SetBodyClass(BodyClass::Light);
            Check(scout.GetMaxHealth() == 70, "scout has 70 health");
            scout.Heal(500);
            Check(scout.GetHealth() == 70, "healing stops at the build's max");
        }

        // Scouts run, heavies plod.
        float walked[2] = {};

        for (int build = 0; build < 2; build++)
        {
            World world;
            world.Create(MakeConfig(21u));
            world.SetBotsEnabled(false);
            const int slot = world.AddPlayer();
            Actor& body = *world.GetPlayer(slot).Body;
            body.SetBodyClass(build == 0 ? BodyClass::Light : BodyClass::Heavy);
            body.Spawn(world.GetTerrain(), world.GetBase(0).X + 420.0f);

            for (int t = 0; t < 60; t++)
                world.Step(nullptr, 0);

            const float startX = body.GetCenterX();
            PlayerInput walk;
            walk.MoveX = 1.0f;

            for (int t = 0; t < 48; t++)
                world.Step(&walk, 1);

            walked[build] = body.GetCenterX() - startX;
        }

        Check(walked[0] > walked[1] * 1.3f, "scout outpaces heavy (" +
            std::to_string(static_cast<int>(walked[0])) + " vs " +
            std::to_string(static_cast<int>(walked[1])) + ")");

        // Every body carries its own kit.
        World world;
        world.Create(MakeConfig(22u));
        world.SetBotsEnabled(false);
        const int slot = world.AddPlayer();
        Actor& player = *world.GetPlayer(slot).Body;

        Check(player.GetItemCount() == World::LoadoutSize, "player starts with the standard kit");

        const int sniper = SelectWeapon(world, slot, "Sniper");
        Check(player.GetItemCount() == World::LoadoutSize + 1, "picked-up weapon is added, not swapped");
        Check(player.GetWeapon().GetDef() == world.FindWeapon("Sniper") && sniper == World::LoadoutSize,
            "and held");

        PlayerInput first;
        first.SelectSlot = 0;
        world.Step(&first, 1);
        Check(player.GetSelectedItem() == 0, "switching back to item 1");

        PlayerInput bogus;
        bogus.SelectSlot = 5 + 10;
        world.Step(&bogus, 1);
        Check(player.GetSelectedItem() == 0, "out-of-range slot ignored");

        world.AddTeamGold(0, 1000);
        OrderRequest order;
        order.Bodies.push_back({ world.FindWeapon("Shotgun"), static_cast<std::uint8_t>(BodyClass::Heavy) });
        Check(world.PriceOrder(order) == 170 + world.FindWeapon("Shotgun")->Cost, "heavy body priced by build");
        Check(world.SubmitOrder(slot, order), "heavy ordered");

        OrderRequest bad;
        bad.Bodies.push_back({ nullptr, 9 });
        Check(!world.SubmitOrder(slot, bad), "unknown build refused");

        for (int t = 0; t < 120 * 30 && world.GetAllies().empty(); t++)
            world.Step(nullptr, 0);

        Check(world.GetAllies().size() == 1, "heavy landed");

        if (world.GetAllies().size() == 1)
        {
            const Actor& ally = *world.GetAllies().front().Body;
            Check(ally.GetBodyClass() == BodyClass::Heavy && ally.GetMaxHealth() == 200, "it's a heavy");
            Check(ally.GetWeapon().GetDef() == world.FindWeapon("Shotgun"), "holding its shotgun");
            Check(ally.GetItemCount() == 2, "and packing a shovel");

            PlayerInput swap;
            swap.SwitchBodyPressed = true;
            world.Step(&swap, 1);

            Check(player.GetBodyClass() == BodyClass::Heavy && player.GetItemCount() == 2,
                "switching bodies takes over that body's kit");
            Check(world.GetAllies().front().Body->GetItemCount() == World::LoadoutSize + 1,
                "the old body keeps its own");
        }
    }

    void TestArsenal()
    {
        std::printf("arsenal: rockets, flame, sniper\n");

        // Rocket: flies, hits, blows a hole in the guard.
        {
            World world;
            world.Create(MakeConfig(31u));
            const int slot = world.AddPlayer();
            Actor* guard = SetUpDuel(world, slot, 260.0f);
            Check(guard != nullptr, "a guard to shoot at");

            SelectWeapon(world, slot, "Rocket Launcher");

            bool sawRocket = false;
            bool sawBlast = false;

            for (int t = 0; t < 240 && guard && guard->IsAlive(); t++)
            {
                PlayerInput input;
                input.AimX = guard->GetCenterX();
                input.AimY = guard->GetCenterY();
                input.Fire = t < 4;
                world.Step(&input, 1);

                sawRocket |= world.GetGrenades().GetRocketCount() > 0;


                for (const SimEvent& event : world.GetEvents())
                    sawBlast |= event.Type == SimEventType::Explosion;
            }

            Check(sawRocket, "rocket launched");
            Check(sawBlast, "rocket exploded");
            Check(world.GetEnemies().empty() || world.GetEnemies().front().Body->GetHealth() < 50,
                "guard wrecked by the rocket");
        }

        // Flamethrower: sets the guard alight.
        {
            World world;
            world.Create(MakeConfig(32u));
            const int slot = world.AddPlayer();
            Actor* guard = SetUpDuel(world, slot, 70.0f);

            SelectWeapon(world, slot, "Flamethrower");

            bool burned = false;
            const int before = guard ? guard->GetHealth() : 0;

            for (int t = 0; t < 120 && guard && guard->IsAlive(); t++)
            {
                PlayerInput input;
                input.AimX = guard->GetCenterX();
                input.AimY = guard->GetCenterY();
                input.Fire = true;
                world.Step(&input, 1);

                burned |= guard->IsBurning();
            }

            Check(burned, "guard caught fire");
            Check(!guard || world.GetEnemies().empty() || guard->GetHealth() < before - 20,
                "and got hurt");
        }

        // Sniper: one shot, heavy damage, at range.
        {
            World world;
            world.Create(MakeConfig(33u));
            const int slot = world.AddPlayer();
            Actor* guard = SetUpDuel(world, slot, 380.0f);

            SelectWeapon(world, slot, "Sniper");

            int lowest = guard ? guard->GetHealth() : 0;

            for (int t = 0; t < 120 && guard && !world.GetEnemies().empty(); t++)
            {
                PlayerInput input;
                input.AimX = guard->GetCenterX();
                input.AimY = guard->GetY() + 18.0f;
                input.Fire = t == 0;
                world.Step(&input, 1);

                if (!world.GetEnemies().empty())
                    lowest = std::min(lowest, guard->GetHealth());
                else
                    lowest = 0;
            }

            Check(lowest <= 30, "sniper round lands hard (guard at " + std::to_string(lowest) + ")");
        }
    }

    void TestDrones()
    {
        std::printf("drones: fly, fight, fall\n");

        World world;
        world.Create(MakeConfig(41u));
        const int slot = world.AddPlayer();
        Actor* guard = SetUpDuel(world, slot, 300.0f);
        Actor& player = *world.GetPlayer(slot).Body;

        // A friendly drone hunts the guard.
        world.AddDrone(player.GetCenterX(), player.GetY() - 60.0f, 0);
        Check(world.GetDrones().size() == 1, "drone in the air");

        const int guardStart = guard ? guard->GetHealth() : 0;

        for (int t = 0; t < 120 * 8 && guard && !world.GetEnemies().empty(); t++)
        {
            world.Step(nullptr, 0);

            // Keep the player out of the fight.
            player.Heal(100);
        }

        Check(world.GetEnemies().empty() || guard->GetHealth() < guardStart,
            "drone shot the guard");

        if (!world.GetDrones().empty())
        {
            const Drone& drone = world.GetDrones().front();
            const float ground = static_cast<float>(world.GetTerrain().GetSurfaceY(static_cast<int>(drone.X)));
            Check(drone.Y < ground - 30.0f, "drone hovers above the ground");
        }

        // A hostile drone gets shot down.
        World other;
        other.Create(MakeConfig(42u));
        other.SetBotsEnabled(false);
        const int me = other.AddPlayer();
        other.GetEnemies().clear();

        Actor& shooter = *other.GetPlayer(me).Body;
        shooter.Spawn(other.GetTerrain(), other.GetBase(0).X + 420.0f);

        for (int t = 0; t < 30; t++)
            other.Step(nullptr, 0);

        other.AddDrone(shooter.GetCenterX() + 160.0f, shooter.GetY() - 40.0f, 1);

        bool downed = false;

        for (int t = 0; t < 120 * 10 && !downed; t++)
        {
            PlayerInput input;

            if (!other.GetDrones().empty())
            {
                input.AimX = other.GetDrones().front().X;
                input.AimY = other.GetDrones().front().Y;
            }

            input.Fire = true;
            input.ReloadPressed = t % 200 == 0;
            other.Step(&input, 1);
            shooter.Heal(100);

            downed = other.GetDrones().empty();
        }

        Check(downed, "player shot the enemy drone down");

        // Drones are orderable.
        other.AddTeamGold(0, 1000);
        OrderRequest order;
        order.Drones = 2;
        Check(other.PriceOrder(order) == 2 * World::DroneCost, "drones priced");
        Check(other.SubmitOrder(me, order), "drones ordered");

        for (int t = 0; t < 120 * 30 && other.GetDrones().size() < 2; t++)
            other.Step(nullptr, 0);

        Check(other.GetDrones().size() == 2, "ordered drones arrived");
    }
}

namespace
{
    void TestAiDigs()
    {
        std::printf("ai: tunnels through what it can't jet over\n");

        World world;
        world.Create(MakeConfig(51u));
        world.SetBotsEnabled(false);
        world.AddPlayer();
        world.GetEnemies().clear();

        // Bury a unit in a pocket inside solid dirt, then order an attack
        // (east). It has a shovel; it has to dig out.
        Terrain& terrain = world.GetTerrain();
        const int x = static_cast<int>(world.GetBase(0).X) + 500;
        const int floorY = terrain.GetSurfaceY(x);

        for (int px = x - 40; px <= x + 260; px++)
        {
            for (int py = floorY - 260; py < floorY + 6; py++)
            {
                const bool pocket = px > x - 18 && px < x + 22 && py > floorY - 70 && py < floorY;
                terrain.SetMaterial(px, py, pocket ? Material::Air : Material::Dirt);
            }
        }

        world.AddAlly(static_cast<float>(x), nullptr);
        world.SetAllyOrder(AllyOrder::Attack);

        float furthest = 0.0f;

        for (int t = 0; t < 120 * 25 && !world.GetAllies().empty(); t++)
        {
            world.Step(nullptr, 0);
            furthest = std::max(furthest, world.GetAllies().front().Body->GetCenterX() - static_cast<float>(x));
        }

        Check(furthest > 120.0f, "dug its way 120px+ east (got " + std::to_string(static_cast<int>(furthest)) + ")");
    }
}

int main()
{
    TestDeterminism();
    TestSeedsDiffer();
    TestOrdersAndIds();
    TestBrainAssault();
    TestEnemiesAdvance();
    TestBodiesAndInventory();
    TestArsenal();
    TestDrones();
    TestAiDigs();
    TestMultiplayerSoak();

    if (g_Failures > 0)
    {
        std::printf("%d check(s) failed\n", g_Failures);
        return EXIT_FAILURE;
    }

    std::printf("all checks passed\n");
    return EXIT_SUCCESS;
}
