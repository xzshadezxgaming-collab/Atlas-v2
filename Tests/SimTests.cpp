// Headless simulation tests. No window, renderer, audio or input device:
// if any of these need one, the World has leaked presentation code.

#include "Sim/World.h"

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
            input.SelectSlot = static_cast<std::int8_t>((tick / 360) % World::LoadoutSize);

        return input;
    }

    void RunScripted(World& world, std::uint64_t ticks,
        const std::function<void(std::uint64_t)>& afterTick = nullptr)
    {
        std::vector<PlayerInput> inputs(world.GetPlayerCount());

        for (std::uint64_t t = 0; t < ticks; t++)
        {
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
        order.Bodies.push_back(world.GetAllyWeapon());
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
        order.Bodies.push_back(world.GetAllyWeapon());
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

int main()
{
    TestDeterminism();
    TestSeedsDiffer();
    TestOrdersAndIds();
    TestBrainAssault();
    TestEnemiesAdvance();
    TestMultiplayerSoak();

    if (g_Failures > 0)
    {
        std::printf("%d check(s) failed\n", g_Failures);
        return EXIT_FAILURE;
    }

    std::printf("all checks passed\n");
    return EXIT_SUCCESS;
}
