#include "Protocol.h"

#include "../Sim/World.h"

#include <algorithm>

namespace Atlas
{
    namespace
    {
        enum InputButtons : std::uint8_t
        {
            ButtonJump = 1 << 0,
            ButtonJumpPressed = 1 << 1,
            ButtonJetpack = 1 << 2,
            ButtonCrouch = 1 << 3,
            ButtonFire = 1 << 4,
            ButtonThrow = 1 << 5,
            ButtonReload = 1 << 6,
        };

        // Upper bound on list lengths read off the wire, so a bad packet
        // can't make us allocate wildly.
        constexpr std::uint32_t MaxListLength = 64;

        void WriteWeapon(ByteWriter& out, const WeaponDef* def, const World& world)
        {
            out.I8(static_cast<std::int8_t>(world.GetWeaponIndex(def)));
        }

        const WeaponDef* ReadWeapon(ByteReader& in, const World& world)
        {
            return world.GetWeaponByIndex(in.I8());
        }
    }

    void WriteInput(ByteWriter& out, const PlayerInput& input)
    {
        std::uint8_t buttons = 0;

        if (input.Jump) buttons |= ButtonJump;
        if (input.JumpPressed) buttons |= ButtonJumpPressed;
        if (input.Jetpack) buttons |= ButtonJetpack;
        if (input.Crouch) buttons |= ButtonCrouch;
        if (input.Fire) buttons |= ButtonFire;
        if (input.ThrowPressed) buttons |= ButtonThrow;
        if (input.ReloadPressed) buttons |= ButtonReload;

        out.U8(buttons);
        out.F32(input.MoveX);
        out.F32(input.AimX);
        out.F32(input.AimY);
        out.I8(input.SelectSlot);
    }

    PlayerInput ReadInput(ByteReader& in)
    {
        PlayerInput input;

        const std::uint8_t buttons = in.U8();

        input.Jump = (buttons & ButtonJump) != 0;
        input.JumpPressed = (buttons & ButtonJumpPressed) != 0;
        input.Jetpack = (buttons & ButtonJetpack) != 0;
        input.Crouch = (buttons & ButtonCrouch) != 0;
        input.Fire = (buttons & ButtonFire) != 0;
        input.ThrowPressed = (buttons & ButtonThrow) != 0;
        input.ReloadPressed = (buttons & ButtonReload) != 0;

        input.MoveX = in.F32();
        input.AimX = in.F32();
        input.AimY = in.F32();
        input.SelectSlot = in.I8();

        return input;
    }

    void WriteOrder(ByteWriter& out, const OrderRequest& order, const World& world)
    {
        out.U8(static_cast<std::uint8_t>(order.Bodies.size()));

        for (const WeaponDef* weapon : order.Bodies)
            WriteWeapon(out, weapon, world);

        out.U8(static_cast<std::uint8_t>(order.LooseWeapons.size()));

        for (const WeaponDef* weapon : order.LooseWeapons)
            WriteWeapon(out, weapon, world);

        out.U8(static_cast<std::uint8_t>(order.SupplyCrates));
    }

    OrderRequest ReadOrder(ByteReader& in, const World& world)
    {
        OrderRequest order;

        const std::uint32_t bodies = std::min<std::uint32_t>(in.U8(), MaxListLength);

        for (std::uint32_t i = 0; i < bodies; i++)
            order.Bodies.push_back(ReadWeapon(in, world));

        const std::uint32_t loose = std::min<std::uint32_t>(in.U8(), MaxListLength);

        for (std::uint32_t i = 0; i < loose; i++)
            order.LooseWeapons.push_back(ReadWeapon(in, world));

        order.SupplyCrates = in.U8();

        return order;
    }

    void WriteFrame(ByteWriter& out, const TickFrame& frame, const World& world)
    {
        out.U32(frame.Tick);
        out.U8(frame.Joins);

        std::uint8_t flags = 0;

        if (frame.BotsEnabled) flags |= 1;
        if (frame.HasHash) flags |= 2;

        out.U8(flags);

        if (frame.HasHash)
        {
            out.U32(frame.HashTick);
            out.U64(frame.Hash);
        }

        out.U8(static_cast<std::uint8_t>(frame.Inputs.size()));

        for (const FrameInput& entry : frame.Inputs)
        {
            out.U8(entry.Slot);
            WriteInput(out, entry.Input);
            out.U8(entry.HasOrder ? 1 : 0);

            if (entry.HasOrder)
                WriteOrder(out, entry.Order, world);
        }
    }

    TickFrame ReadFrame(ByteReader& in, const World& world)
    {
        TickFrame frame;

        frame.Tick = in.U32();
        frame.Joins = in.U8();

        const std::uint8_t flags = in.U8();
        frame.BotsEnabled = (flags & 1) != 0;
        frame.HasHash = (flags & 2) != 0;

        if (frame.HasHash)
        {
            frame.HashTick = in.U32();
            frame.Hash = in.U64();
        }

        const std::uint32_t count =
            std::min<std::uint32_t>(in.U8(), World::MaxPlayers);

        for (std::uint32_t i = 0; i < count; i++)
        {
            FrameInput entry;
            entry.Slot = in.U8();
            entry.Input = ReadInput(in);
            entry.HasOrder = in.U8() != 0;

            if (entry.HasOrder)
                entry.Order = ReadOrder(in, world);

            frame.Inputs.push_back(entry);
        }

        return frame;
    }

    void WriteWelcome(ByteWriter& out, const WelcomeInfo& info)
    {
        out.U8(static_cast<std::uint8_t>(info.Slot));
        out.U32(static_cast<std::uint32_t>(info.Width));
        out.U32(static_cast<std::uint32_t>(info.Height));
        out.U32(info.Seed);
        out.String(info.WeaponsIni);
        out.U32(info.HistoryFrames);
    }

    WelcomeInfo ReadWelcome(ByteReader& in)
    {
        WelcomeInfo info;
        info.Slot = in.U8();
        info.Width = static_cast<int>(in.U32());
        info.Height = static_cast<int>(in.U32());
        info.Seed = in.U32();
        info.WeaponsIni = in.String();
        info.HistoryFrames = in.U32();
        return info;
    }

    bool ApplyFrame(World& world, const TickFrame& frame)
    {
        if (frame.Tick != world.GetTick())
            return false;

        for (int i = 0; i < frame.Joins; i++)
            world.AddPlayer();

        world.SetBotsEnabled(frame.BotsEnabled);

        PlayerInput inputs[World::MaxPlayers] = {};

        for (const FrameInput& entry : frame.Inputs)
        {
            if (entry.Slot >= World::MaxPlayers)
                continue;

            if (entry.HasOrder)
                world.SubmitOrder(entry.Slot, entry.Order);

            inputs[entry.Slot] = entry.Input;
        }

        world.Step(inputs, World::MaxPlayers);
        return true;
    }
}
