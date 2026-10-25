// Multiplayer over real sockets (loopback): a host and two clients in one
// process, one of them joining mid-match. Every peer must end on the
// same tick with the same state hash.

#include "Net/Session.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <process.h>
#define ATLAS_GETPID _getpid
#else
#include <unistd.h>
#define ATLAS_GETPID getpid
#endif

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

    constexpr float Dt = World::FixedTimeStep;

    // Varied, reproducible input for whichever slot a session controls.
    PlayerInput Script(Session& session, int frame)
    {
        PlayerInput input;

        if (session.GetState() != SessionState::Running)
            return input;

        World& world = session.GetWorld();
        const int slot = session.GetLocalSlot();

        if (slot >= world.GetPlayerCount())
            return input;

        const Actor& body = *world.GetPlayer(slot).Body;
        const int phase = (frame / 200 + slot) % 3;

        input.MoveX = phase == 0 ? 1.0f : (phase == 1 ? -1.0f : 0.0f);
        input.Jump = (frame + slot * 31) % 140 < 3;
        input.JumpPressed = (frame + slot * 31) % 140 == 0;
        input.Jetpack = (frame % 300) > 270;
        input.Fire = (frame + slot * 17) % 80 < 50;
        input.ThrowPressed = (frame + slot * 53) % 260 == 0;

        const float angle = static_cast<float>(frame) * 0.013f + static_cast<float>(slot);
        input.AimX = body.GetCenterX() + std::cos(angle) * 140.0f;
        input.AimY = body.GetCenterY() + 30.0f + std::sin(angle) * 90.0f;

        if (frame % 250 == 0)
            input.SelectSlot = static_cast<std::int8_t>((frame / 250 + slot) % World::LoadoutSize);

        return input;
    }

    bool PumpUntilRunning(Session& host, Session& client, int& frame)
    {
        for (int i = 0; i < 4000; i++)
        {
            host.Update(Dt, Script(host, frame), nullptr, nullptr);
            client.Update(Dt, PlayerInput{}, nullptr, nullptr);
            frame++;

            if (client.GetState() == SessionState::Running)
                return true;

            if (client.GetState() == SessionState::Failed)
            {
                std::printf("  client failed: %s\n", client.GetStatusText().c_str());
                return false;
            }
        }

        return false;
    }
}

int main()
{
    const std::uint16_t port =
        static_cast<std::uint16_t>(37960 + ATLAS_GETPID() % 2000);

    std::printf("lockstep: host + 2 clients on 127.0.0.1:%u\n", port);

    WorldConfig config;
    config.Width = 2048;
    config.Height = 1024;
    config.Seed = 4242u;

    Session host;
    Check(host.StartHost(config, std::string(), port), "host started: " + host.GetStatusText());

    if (g_Failures > 0)
        return EXIT_FAILURE;

    int frame = 0;

    Session early;
    Check(early.StartClient("127.0.0.1", port), "client 1 connecting");
    Check(PumpUntilRunning(host, early, frame), "client 1 joined");
    Check(early.GetLocalSlot() == 1, "client 1 got slot 1");

    // Some shared play.
    for (int i = 0; i < 900; i++, frame++)
    {
        host.Update(Dt, Script(host, frame), nullptr, nullptr);
        early.Update(Dt, Script(early, frame), nullptr, nullptr);
    }

    // A buy order from a client goes through the host like any input
    // and is priced identically everywhere (accepted or refused alike).
    {
        OrderRequest order;
        order.SupplyCrates = 1;
        early.Update(Dt, Script(early, frame), &order, nullptr);
        host.Update(Dt, Script(host, frame), nullptr, nullptr);
        frame++;
    }

    // A late joiner replays the match so far before taking control.
    Session late;
    Check(late.StartClient("127.0.0.1", port), "client 2 connecting");

    for (int i = 0; i < 4000 && late.GetState() != SessionState::Running; i++, frame++)
    {
        host.Update(Dt, Script(host, frame), nullptr, nullptr);
        early.Update(Dt, Script(early, frame), nullptr, nullptr);
        late.Update(Dt, PlayerInput{}, nullptr, nullptr);
    }

    Check(late.GetState() == SessionState::Running, "client 2 joined mid-match: " + late.GetStatusText());
    Check(late.GetLocalSlot() == 2, "client 2 got slot 2");

    for (int i = 0; i < 900; i++, frame++)
    {
        host.Update(Dt, Script(host, frame), nullptr, nullptr);
        early.Update(Dt, Script(early, frame), nullptr, nullptr);
        late.Update(Dt, Script(late, frame), nullptr, nullptr);
    }

    // Host stops; clients drain whatever is in flight.
    const std::uint64_t finalTick = host.GetWorld().GetTick();

    for (int i = 0; i < 4000; i++)
    {
        host.Update(0.0f, PlayerInput{}, nullptr, nullptr);
        early.Update(Dt, PlayerInput{}, nullptr, nullptr);
        late.Update(Dt, PlayerInput{}, nullptr, nullptr);

        if (early.GetWorld().GetTick() == finalTick &&
            late.GetWorld().GetTick() == finalTick)
            break;
    }

    Check(early.GetWorld().GetTick() == finalTick, "client 1 caught up");
    Check(late.GetWorld().GetTick() == finalTick, "client 2 caught up");
    Check(host.GetWorld().GetPlayerCount() == 3, "three players in the match");
    Check(host.GetConnectedClients() == 2, "host sees two clients");

    const std::uint64_t hostHash = host.GetWorld().ComputeStateHash();
    Check(early.GetWorld().ComputeStateHash() == hostHash, "client 1 state matches host");
    Check(late.GetWorld().ComputeStateHash() == hostHash, "client 2 state matches host");
    Check(!early.HasDesynced() && !late.HasDesynced(), "no desync reported");

    // The clients' inputs really drove their bodies on the host.
    for (int slot = 1; slot <= 2; slot++)
    {
        const World& world = host.GetWorld();
        const float moved = std::fabs(
            world.GetPlayer(slot).Body->GetCenterX() - world.GetSpawnX(slot));
        Check(moved > 20.0f || !world.GetPlayer(slot).Body->IsAlive(),
            "player " + std::to_string(slot + 1) + " moved by its client's input");
    }

    std::printf("  tick %llu, wave %d, hash %016llx\n",
        static_cast<unsigned long long>(finalTick),
        host.GetWorld().GetWave(),
        static_cast<unsigned long long>(hostHash));

    // A fifth player is turned away.
    {
        Session c3;
        c3.StartClient("127.0.0.1", port);
        int f = frame;
        PumpUntilRunning(host, c3, f);

        Session refused;
        refused.StartClient("127.0.0.1", port);

        for (int i = 0; i < 2000 && refused.GetState() != SessionState::Failed; i++)
        {
            host.Update(Dt, PlayerInput{}, nullptr, nullptr);
            early.Update(Dt, PlayerInput{}, nullptr, nullptr);
            late.Update(Dt, PlayerInput{}, nullptr, nullptr);
            c3.Update(Dt, PlayerInput{}, nullptr, nullptr);
            refused.Update(Dt, PlayerInput{}, nullptr, nullptr);
        }

        Check(refused.GetState() == SessionState::Failed &&
            refused.GetStatusText().find("full") != std::string::npos,
            "fifth player refused: " + refused.GetStatusText());
    }

    if (g_Failures > 0)
    {
        std::printf("%d check(s) failed\n", g_Failures);
        return EXIT_FAILURE;
    }

    std::printf("all checks passed\n");
    return EXIT_SUCCESS;
}
