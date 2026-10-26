#pragma once

#include "ByteStream.h"

#include "../Sim/PlayerInput.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Atlas
{
    class World;

    // Bump whenever the wire format or anything that affects simulation
    // results changes: peers on different versions would desync.
    constexpr std::uint16_t ProtocolVersion = 8;

    constexpr std::uint16_t DefaultPort = 27960;

    enum class MessageType : std::uint8_t
    {
        Hello = 1,   // client -> host: version
        Welcome,     // host -> client: world config, assigned slot
        Reject,      // host -> client: reason, then disconnect
        ClientInput, // client -> host: latest input (+ an order)
        Frames,      // host -> clients: finalized tick frames
    };

    // One player's contribution to a tick.
    struct FrameInput
    {
        std::uint8_t Slot = 0;
        PlayerInput Input;
        bool HasOrder = false;
        OrderRequest Order;
    };

    // Everything every peer needs to advance the World by one tick
    // identically. The host decides frames; everyone (the host too)
    // applies them in order: joins, then settings, then orders, then
    // World::Step with the inputs.
    struct TickFrame
    {
        std::uint32_t Tick = 0;      // World::GetTick() before this step
        std::uint8_t Joins = 0;      // players added before this step
        bool BotsEnabled = true;
        std::vector<FrameInput> Inputs;

        // Host's state hash after tick HashTick, for desync detection.
        bool HasHash = false;
        std::uint32_t HashTick = 0;
        std::uint64_t Hash = 0;
    };

    struct WelcomeInfo
    {
        int Slot = 0;
        int Width = 0;
        int Height = 0;
        unsigned int Seed = 0;
        std::uint8_t Mode = 0; // GameMode
        std::uint8_t Difficulty = 1;
        std::uint8_t LandingZone = 12;
        std::uint8_t Biome = 0;
        std::string WeaponsIni;
        std::uint32_t HistoryFrames = 0; // frames that will follow before live ones
    };

    void WriteInput(ByteWriter& out, const PlayerInput& input);
    PlayerInput ReadInput(ByteReader& in);

    // Weapons travel as indices into the World's weapon list.
    void WriteOrder(ByteWriter& out, const OrderRequest& order, const World& world);
    OrderRequest ReadOrder(ByteReader& in, const World& world);

    void WriteFrame(ByteWriter& out, const TickFrame& frame, const World& world);
    TickFrame ReadFrame(ByteReader& in, const World& world);

    void WriteWelcome(ByteWriter& out, const WelcomeInfo& info);
    WelcomeInfo ReadWelcome(ByteReader& in);

    // Applies a frame to the World (joins, settings, orders, step).
    // Returns false if the frame is not for the World's current tick.
    bool ApplyFrame(World& world, const TickFrame& frame);
}
