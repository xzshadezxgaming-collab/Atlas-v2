#pragma once

#include "../Sim/PlayerInput.h"
#include "../Sim/World.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Atlas
{
    enum class SessionRole
    {
        Offline,
        Host,
        Client,
    };

    enum class SessionState
    {
        Idle,
        Connecting, // client: waiting for the host's welcome
        Joining,    // client: replaying the match so far
        Running,
        Failed,     // could not start / connection lost; see GetStatusText()
    };

    // Runs the World for one machine in a game: alone, as the host of a
    // multiplayer match, or as a client of one.
    //
    // Multiplayer is deterministic lockstep, the model Factorio and most
    // RTS games use for worlds with too much state to stream: every peer
    // runs the same World and only inputs cross the network. The host
    // is the authority on what happens each tick - it collects everyone's
    // latest input, seals it into a TickFrame, applies it, and sends it
    // to all clients, who apply the same frames in the same order. A
    // tick's destroyed terrain, particles and AI therefore cost no
    // bandwidth at all. The host also sends periodic state hashes so a
    // desync is detected instead of silently diverging.
    //
    // Late joiners receive the full frame history and fast-forward the
    // match from tick 0 before taking control.
    class Session
    {
    public:
        Session();
        ~Session();

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        bool StartOffline(const WorldConfig& config, const std::string& weaponsIni);
        bool StartHost(const WorldConfig& config, const std::string& weaponsIni, std::uint16_t port);
        bool StartClient(const std::string& hostName, std::uint16_t port);

        using TickCallback = std::function<void()>;

        // Advances by up to frameTime seconds of simulation (pass 0 to
        // hold, e.g. during hit-stop). localInput is this machine's input
        // for the frame, edges included; order (may be null) is a buy
        // order to submit. onTick runs after every simulated tick so the
        // caller can consume the World's events.
        //
        // Returns true if localInput's edges and the order were taken -
        // the caller clears its latched edges only then.
        bool Update(
            float frameTime,
            const PlayerInput& localInput,
            const OrderRequest* order,
            const TickCallback& onTick);

        // Host/offline: what the bots toggle should be from the next tick
        // on. Clients follow the host and ignore this.
        void SetBotsEnabled(bool enabled);

        SessionRole GetRole() const;
        SessionState GetState() const;
        const std::string& GetStatusText() const;

        // The World exists once the client has been welcomed (and from
        // the start offline or hosting). The local player can act once
        // the state is Running.
        bool HasWorld() const;
        World& GetWorld();
        int GetLocalSlot() const;

        float GetJoinProgress() const; // 0..1 while Joining
        int GetConnectedClients() const; // host only
        bool HasDesynced() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
