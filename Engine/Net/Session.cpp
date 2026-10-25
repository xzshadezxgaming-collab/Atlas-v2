#include "Session.h"

#include "Protocol.h"

#ifdef _WIN32
// enet.h pulls in <windows.h>; keep its min/max macros away from std::min.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include <enet/enet.h>

#include <algorithm>
#include <deque>
#include <map>
#include <vector>

namespace Atlas
{
    namespace
    {
        // Hashing the whole world is a few milliseconds, so peers compare
        // state once a second rather than every tick.
        constexpr std::uint32_t HashInterval = 120;

        // History is sent to late joiners in chunks of this many frames.
        constexpr std::size_t HistoryChunkFrames = 2048;

        // Frames a joining client replays per Update while catching up.
        constexpr int JoinReplayFramesPerUpdate = 6000;

        constexpr float ConnectTimeoutSeconds = 10.0f;

        // ENet is initialized once per process, however many sessions
        // exist (the tests run a host and clients side by side).
        int g_EnetUsers = 0;

        bool AcquireEnet()
        {
            if (g_EnetUsers == 0 && enet_initialize() != 0)
                return false;

            g_EnetUsers++;
            return true;
        }

        void ReleaseEnet()
        {
            if (g_EnetUsers > 0 && --g_EnetUsers == 0)
                enet_deinitialize();
        }

        void SendBytes(ENetPeer* peer, const std::vector<std::uint8_t>& data)
        {
            ENetPacket* packet = enet_packet_create(
                data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer, 0, packet);
        }

        // A client as the host sees it: the latest held input, plus
        // edges and orders accumulated since the last tick consumed them.
        struct RemotePlayer
        {
            ENetPeer* Peer = nullptr;
            int Slot = -1;
            bool Welcomed = false;

            PlayerInput Held;
            bool JumpEdge = false;
            bool ThrowEdge = false;
            bool ReloadEdge = false;
            std::int8_t SelectSlot = -1;
            std::deque<OrderRequest> Orders;
        };
    }

    struct Session::Impl
    {
        SessionRole Role = SessionRole::Offline;
        SessionState State = SessionState::Idle;
        std::string Status;

        std::unique_ptr<World> TheWorld;
        std::string WeaponsIni;
        WorldConfig Config;

        int LocalSlot = 0;
        bool BotsEnabled = true;
        float Accumulator = 0.0f;

        bool EnetAcquired = false;
        ENetHost* NetHost = nullptr;

        // Host
        std::vector<RemotePlayer> Remotes;
        // Every sealed frame since tick 0, back to back, for late
        // joiners; HistoryOffsets[i] is where frame i starts.
        std::vector<std::uint8_t> HistoryBytes;
        std::vector<std::size_t> HistoryOffsets;
        std::size_t OutgoingFirst = 0; // first frame not yet broadcast
        int PendingJoins = 0;
        bool HashPending = false;
        std::uint32_t PendingHashTick = 0;
        std::uint64_t PendingHash = 0;

        // Client
        ENetPeer* Server = nullptr;
        std::deque<TickFrame> Queue;
        std::uint32_t HistoryTotal = 0;
        std::uint32_t FramesApplied = 0;
        std::map<std::uint32_t, std::uint64_t> OwnHashes;
        bool Desynced = false;
        float ConnectTimer = 0.0f;

        ~Impl()
        {
            if (Server)
                enet_peer_disconnect_now(Server, 0);

            for (RemotePlayer& remote : Remotes)
                enet_peer_disconnect_now(remote.Peer, 0);

            if (NetHost)
                enet_host_destroy(NetHost);

            if (EnetAcquired)
                ReleaseEnet();
        }

        void Fail(const std::string& reason)
        {
            State = SessionState::Failed;
            Status = reason;
        }

        bool CreateWorld()
        {
            Config.Weapons = LoadWeaponDefsFromText(WeaponsIni);

            TheWorld = std::make_unique<World>();

            if (!TheWorld->Create(Config))
            {
                Fail("Could not create the world");
                return false;
            }

            return true;
        }

        // --- Host / offline ---------------------------------------------

        TickFrame BuildFrame(const PlayerInput* localInput, const OrderRequest* localOrder)
        {
            World& world = *TheWorld;

            TickFrame frame;
            frame.Tick = static_cast<std::uint32_t>(world.GetTick());
            frame.Joins = static_cast<std::uint8_t>(PendingJoins);
            frame.BotsEnabled = BotsEnabled;
            PendingJoins = 0;

            if (HashPending)
            {
                frame.HasHash = true;
                frame.HashTick = PendingHashTick;
                frame.Hash = PendingHash;
                HashPending = false;
            }

            const int playersAfterJoins = world.GetPlayerCount() + frame.Joins;

            if (localInput && LocalSlot < playersAfterJoins)
            {
                FrameInput entry;
                entry.Slot = static_cast<std::uint8_t>(LocalSlot);
                entry.Input = *localInput;

                if (localOrder)
                {
                    entry.HasOrder = true;
                    entry.Order = *localOrder;
                }

                frame.Inputs.push_back(entry);
            }

            for (RemotePlayer& remote : Remotes)
            {
                if (!remote.Welcomed || remote.Slot < 0 || remote.Slot >= playersAfterJoins)
                    continue;

                FrameInput entry;
                entry.Slot = static_cast<std::uint8_t>(remote.Slot);
                entry.Input = remote.Held;
                entry.Input.JumpPressed = remote.JumpEdge;
                entry.Input.ThrowPressed = remote.ThrowEdge;
                entry.Input.ReloadPressed = remote.ReloadEdge;
                entry.Input.SelectSlot = remote.SelectSlot;

                remote.JumpEdge = false;
                remote.ThrowEdge = false;
                remote.ReloadEdge = false;
                remote.SelectSlot = -1;

                if (!remote.Orders.empty())
                {
                    entry.HasOrder = true;
                    entry.Order = remote.Orders.front();
                    remote.Orders.pop_front();
                }

                frame.Inputs.push_back(entry);
            }

            return frame;
        }

        // Seals, applies and (when hosting) records one tick. The frame
        // is applied from its encoded form so the host steps exactly
        // what clients will.
        void RunHostTick(const PlayerInput* localInput, const OrderRequest* localOrder)
        {
            World& world = *TheWorld;

            const TickFrame frame = BuildFrame(localInput, localOrder);

            ByteWriter writer;
            WriteFrame(writer, frame, world);

            ByteReader reader(writer.Data().data(), writer.Data().size());
            const TickFrame sealed = ReadFrame(reader, world);

            ApplyFrame(world, sealed);

            if (Role == SessionRole::Host)
            {
                HistoryOffsets.push_back(HistoryBytes.size());
                HistoryBytes.insert(
                    HistoryBytes.end(), writer.Data().begin(), writer.Data().end());

                if (world.GetTick() % HashInterval == 0)
                {
                    HashPending = true;
                    PendingHashTick = static_cast<std::uint32_t>(world.GetTick());
                    PendingHash = world.ComputeStateHash();
                }
            }
        }

        // Sends history frames [first, first + count) in one message.
        void SendFrames(ENetPeer* peer, std::size_t first, std::size_t count)
        {
            if (count == 0)
                return;

            const std::size_t begin = HistoryOffsets[first];
            const std::size_t end = first + count < HistoryOffsets.size()
                ? HistoryOffsets[first + count]
                : HistoryBytes.size();

            ByteWriter out;
            out.U8(static_cast<std::uint8_t>(MessageType::Frames));
            out.U32(static_cast<std::uint32_t>(count));
            out.Data().insert(
                out.Data().end(),
                HistoryBytes.begin() + static_cast<std::ptrdiff_t>(begin),
                HistoryBytes.begin() + static_cast<std::ptrdiff_t>(end));

            SendBytes(peer, out.Data());
        }

        RemotePlayer* FindRemote(ENetPeer* peer)
        {
            for (RemotePlayer& remote : Remotes)
            {
                if (remote.Peer == peer)
                    return &remote;
            }

            return nullptr;
        }

        void Reject(ENetPeer* peer, const std::string& reason)
        {
            ByteWriter out;
            out.U8(static_cast<std::uint8_t>(MessageType::Reject));
            out.String(reason);
            SendBytes(peer, out.Data());
            enet_peer_disconnect_later(peer, 0);
        }

        void HandleHostMessage(RemotePlayer& remote, ByteReader& in)
        {
            const MessageType type = static_cast<MessageType>(in.U8());

            if (type == MessageType::Hello && !remote.Welcomed)
            {
                const std::uint16_t version = in.U16();

                if (!in.Ok() || version != ProtocolVersion)
                {
                    Reject(remote.Peer, "Version mismatch: host runs protocol " +
                        std::to_string(ProtocolVersion));
                    return;
                }

                const int slot = TheWorld->GetPlayerCount() + PendingJoins;

                if (slot >= World::MaxPlayers)
                {
                    Reject(remote.Peer, "Server is full");
                    return;
                }

                PendingJoins++;
                remote.Slot = slot;
                remote.Welcomed = true;

                WelcomeInfo info;
                info.Slot = slot;
                info.Width = Config.Width;
                info.Height = Config.Height;
                info.Seed = Config.Seed;
                info.Mode = static_cast<std::uint8_t>(Config.Mode);
                info.Difficulty = static_cast<std::uint8_t>(Config.Difficulty);
                info.WeaponsIni = WeaponsIni;
                info.HistoryFrames = static_cast<std::uint32_t>(OutgoingFirst);

                ByteWriter out;
                out.U8(static_cast<std::uint8_t>(MessageType::Welcome));
                WriteWelcome(out, info);
                SendBytes(remote.Peer, out.Data());

                // Everything already broadcast to the others; frames sealed
                // later reach this client with the next broadcast.
                for (std::size_t first = 0; first < OutgoingFirst; first += HistoryChunkFrames)
                {
                    SendFrames(remote.Peer, first,
                        std::min(HistoryChunkFrames, OutgoingFirst - first));
                }

                return;
            }

            if (type == MessageType::ClientInput && remote.Welcomed)
            {
                const PlayerInput input = ReadInput(in);
                const bool hasOrder = in.U8() != 0;
                OrderRequest order;

                if (hasOrder)
                    order = ReadOrder(in, *TheWorld);

                if (!in.Ok())
                    return;

                // Held state is simply the latest; edges and orders
                // accumulate until a tick consumes them, so none are lost
                // when several client frames land between two ticks.
                remote.Held = input;
                remote.JumpEdge = remote.JumpEdge || input.JumpPressed;
                remote.ThrowEdge = remote.ThrowEdge || input.ThrowPressed;
                remote.ReloadEdge = remote.ReloadEdge || input.ReloadPressed;

                if (input.SelectSlot >= 0)
                    remote.SelectSlot = input.SelectSlot;

                if (hasOrder && remote.Orders.size() < 8)
                    remote.Orders.push_back(order);
            }
        }

        void ServiceHost()
        {
            ENetEvent event;

            while (enet_host_service(NetHost, &event, 0) > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    RemotePlayer remote;
                    remote.Peer = event.peer;
                    Remotes.push_back(remote);
                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    if (RemotePlayer* remote = FindRemote(event.peer))
                    {
                        ByteReader in(event.packet->data, event.packet->dataLength);
                        HandleHostMessage(*remote, in);
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                    // The body stays in the world (idle) - a dropped
                    // player's slot is not reused in this version.
                    Remotes.erase(
                        std::remove_if(Remotes.begin(), Remotes.end(),
                            [&](const RemotePlayer& r) { return r.Peer == event.peer; }),
                        Remotes.end());
                    break;

                default:
                    break;
                }
            }
        }

        bool UpdateHostOrOffline(
            float frameTime,
            const PlayerInput& localInput,
            const OrderRequest* order,
            const TickCallback& onTick)
        {
            if (NetHost)
                ServiceHost();

            Accumulator += frameTime;

            bool ticked = false;

            while (Accumulator >= World::FixedTimeStep)
            {
                // Edges and the order belong to the first tick only.
                PlayerInput input = localInput;

                if (ticked)
                {
                    input.JumpPressed = false;
                    input.ThrowPressed = false;
                    input.ReloadPressed = false;
                    input.SelectSlot = -1;
                }

                RunHostTick(&input, ticked ? nullptr : order);

                if (onTick)
                    onTick();

                Accumulator -= World::FixedTimeStep;
                ticked = true;
            }

            if (NetHost)
            {
                const std::size_t sealed = HistoryOffsets.size();

                if (sealed > OutgoingFirst)
                {
                    for (RemotePlayer& remote : Remotes)
                    {
                        if (remote.Welcomed)
                            SendFrames(remote.Peer, OutgoingFirst, sealed - OutgoingFirst);
                    }

                    OutgoingFirst = sealed;
                }

                enet_host_flush(NetHost);
            }

            return ticked;
        }

        // --- Client ---------------------------------------------------

        void HandleClientMessage(ByteReader& in)
        {
            const MessageType type = static_cast<MessageType>(in.U8());

            if (type == MessageType::Welcome && !TheWorld)
            {
                const WelcomeInfo info = ReadWelcome(in);

                if (!in.Ok())
                {
                    Fail("Bad welcome from host");
                    return;
                }

                Config.Width = info.Width;
                Config.Height = info.Height;
                Config.Seed = info.Seed;
                Config.Mode = info.Mode == static_cast<std::uint8_t>(GameMode::Survival)
                    ? GameMode::Survival
                    : GameMode::BrainAssault;
                Config.Difficulty = info.Difficulty;
                WeaponsIni = info.WeaponsIni;
                LocalSlot = info.Slot;
                HistoryTotal = info.HistoryFrames;

                if (CreateWorld())
                {
                    State = SessionState::Joining;
                    Status = "Joining match";
                }

                return;
            }

            if (type == MessageType::Frames && TheWorld)
            {
                const std::uint32_t count = in.U32();

                for (std::uint32_t i = 0; i < count && in.Ok(); i++)
                    Queue.push_back(ReadFrame(in, *TheWorld));

                if (!in.Ok())
                    Fail("Corrupt frame data from host");

                return;
            }

            if (type == MessageType::Reject)
            {
                Fail("Host refused: " + in.String());
            }
        }

        void ServiceClient()
        {
            ENetEvent event;

            while (enet_host_service(NetHost, &event, 0) > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    ByteWriter out;
                    out.U8(static_cast<std::uint8_t>(MessageType::Hello));
                    out.U16(ProtocolVersion);
                    SendBytes(event.peer, out.Data());
                    Status = "Connected, waiting for host";
                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    ByteReader in(event.packet->data, event.packet->dataLength);
                    HandleClientMessage(in);
                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                    Server = nullptr;

                    if (State != SessionState::Failed)
                        Fail(TheWorld ? "Lost connection to host" : "Could not reach host");
                    break;

                default:
                    break;
                }
            }
        }

        bool ApplyClientFrame(const TickFrame& frame, const TickCallback* onTick)
        {
            World& world = *TheWorld;

            if (!ApplyFrame(world, frame))
            {
                Fail("Out of sync with host (unexpected tick)");
                Desynced = true;
                return false;
            }

            FramesApplied++;

            if (onTick && *onTick)
                (*onTick)();

            const std::uint32_t tick = static_cast<std::uint32_t>(world.GetTick());

            if (tick % HashInterval == 0)
                OwnHashes[tick] = world.ComputeStateHash();

            if (frame.HasHash)
            {
                const auto it = OwnHashes.find(frame.HashTick);

                if (it != OwnHashes.end() && it->second != frame.Hash && !Desynced)
                {
                    Desynced = true;
                    Status = "DESYNC at tick " + std::to_string(frame.HashTick);
                }

                OwnHashes.erase(OwnHashes.begin(), OwnHashes.upper_bound(frame.HashTick));
            }

            return true;
        }

        bool UpdateClient(
            float frameTime,
            const PlayerInput& localInput,
            const OrderRequest* order,
            const TickCallback& onTick)
        {
            if (State == SessionState::Failed)
                return true;

            ServiceClient();

            if (State == SessionState::Connecting)
            {
                ConnectTimer += frameTime;

                if (ConnectTimer > ConnectTimeoutSeconds)
                    Fail("Timed out connecting to host");

                return true;
            }

            bool consumed = false;

            if (State == SessionState::Running && Server)
            {
                ByteWriter out;
                out.U8(static_cast<std::uint8_t>(MessageType::ClientInput));
                WriteInput(out, localInput);
                out.U8(order ? 1 : 0);

                if (order)
                    WriteOrder(out, *order, *TheWorld);

                SendBytes(Server, out.Data());
                consumed = true;
            }

            if (State == SessionState::Joining)
            {
                // Fast-forward the match so far, silently.
                for (int i = 0; i < JoinReplayFramesPerUpdate && !Queue.empty(); i++)
                {
                    const TickFrame frame = Queue.front();
                    Queue.pop_front();

                    if (!ApplyClientFrame(frame, nullptr))
                        return true;
                }

                if (FramesApplied >= HistoryTotal &&
                    TheWorld->GetPlayerCount() > LocalSlot)
                {
                    State = SessionState::Running;
                    Status = Desynced ? Status : "Connected";
                    Accumulator = 0.0f;
                }
            }
            else if (State == SessionState::Running)
            {
                // Play frames back at the tick rate, catching up when the
                // queue grows (network bunching, our own hit-stop).
                Accumulator += frameTime;

                int ticks = static_cast<int>(Accumulator / World::FixedTimeStep);
                Accumulator -= static_cast<float>(ticks) * World::FixedTimeStep;

                const int queued = static_cast<int>(Queue.size());

                if (queued > ticks + 4)
                    ticks = queued - 2;

                ticks = std::min(ticks, queued);

                if (Queue.empty())
                    Accumulator = std::min(Accumulator, World::FixedTimeStep);

                for (int i = 0; i < ticks; i++)
                {
                    const TickFrame frame = Queue.front();
                    Queue.pop_front();

                    if (!ApplyClientFrame(frame, &onTick))
                        break;
                }
            }

            if (NetHost)
                enet_host_flush(NetHost);

            return consumed || State != SessionState::Running;
        }
    };

    Session::Session()
        : m_Impl(std::make_unique<Impl>())
    {
    }

    Session::~Session() = default;

    bool Session::StartOffline(const WorldConfig& config, const std::string& weaponsIni)
    {
        Impl& s = *m_Impl;

        s.Role = SessionRole::Offline;
        s.Config = config;
        s.WeaponsIni = weaponsIni;

        if (!s.CreateWorld())
            return false;

        // Tick 0 adds the local player.
        s.PendingJoins = 1;
        s.RunHostTick(nullptr, nullptr);

        s.State = SessionState::Running;
        s.Status = "Offline";
        return true;
    }

    bool Session::StartHost(
        const WorldConfig& config,
        const std::string& weaponsIni,
        std::uint16_t port)
    {
        Impl& s = *m_Impl;

        s.Role = SessionRole::Host;
        s.Config = config;
        s.WeaponsIni = weaponsIni;

        if (!AcquireEnet())
        {
            s.Fail("Networking unavailable");
            return false;
        }

        s.EnetAcquired = true;

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        s.NetHost = enet_host_create(&address, 16, 1, 0, 0);

        if (!s.NetHost)
        {
            s.Fail("Could not open port " + std::to_string(port));
            return false;
        }

        if (!s.CreateWorld())
            return false;

        s.PendingJoins = 1;
        s.RunHostTick(nullptr, nullptr);

        s.State = SessionState::Running;
        s.Status = "Hosting on port " + std::to_string(port);
        return true;
    }

    bool Session::StartClient(const std::string& hostName, std::uint16_t port)
    {
        Impl& s = *m_Impl;

        s.Role = SessionRole::Client;

        if (!AcquireEnet())
        {
            s.Fail("Networking unavailable");
            return false;
        }

        s.EnetAcquired = true;

        s.NetHost = enet_host_create(nullptr, 1, 1, 0, 0);

        if (!s.NetHost)
        {
            s.Fail("Could not create a network socket");
            return false;
        }

        ENetAddress address;

        if (enet_address_set_host(&address, hostName.c_str()) != 0)
        {
            s.Fail("Unknown host " + hostName);
            return false;
        }

        address.port = port;

        s.Server = enet_host_connect(s.NetHost, &address, 1, 0);

        if (!s.Server)
        {
            s.Fail("Could not start connecting");
            return false;
        }

        s.State = SessionState::Connecting;
        s.Status = "Connecting to " + hostName + ":" + std::to_string(port);
        return true;
    }

    bool Session::Update(
        float frameTime,
        const PlayerInput& localInput,
        const OrderRequest* order,
        const TickCallback& onTick)
    {
        Impl& s = *m_Impl;

        if (s.Role == SessionRole::Client)
            return s.UpdateClient(frameTime, localInput, order, onTick);

        if (!s.TheWorld)
            return true;

        return s.UpdateHostOrOffline(frameTime, localInput, order, onTick);
    }

    void Session::SetBotsEnabled(bool enabled)
    {
        m_Impl->BotsEnabled = enabled;
    }

    SessionRole Session::GetRole() const { return m_Impl->Role; }
    SessionState Session::GetState() const { return m_Impl->State; }
    const std::string& Session::GetStatusText() const { return m_Impl->Status; }
    bool Session::HasWorld() const { return m_Impl->TheWorld != nullptr; }
    World& Session::GetWorld() { return *m_Impl->TheWorld; }
    int Session::GetLocalSlot() const { return m_Impl->LocalSlot; }
    bool Session::HasDesynced() const { return m_Impl->Desynced; }

    float Session::GetJoinProgress() const
    {
        const Impl& s = *m_Impl;

        if (s.HistoryTotal == 0)
            return 1.0f;

        return std::min(1.0f,
            static_cast<float>(s.FramesApplied) / static_cast<float>(s.HistoryTotal));
    }

    int Session::GetConnectedClients() const
    {
        int count = 0;

        for (const RemotePlayer& remote : m_Impl->Remotes)
        {
            if (remote.Welcomed)
                count++;
        }

        return count;
    }
}
