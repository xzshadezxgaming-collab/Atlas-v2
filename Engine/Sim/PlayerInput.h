#pragma once

#include <cstdint>
#include <vector>

namespace Atlas
{
    struct WeaponDef;

    // Everything one player can ask of their actor in one simulation
    // tick. The World never reads the keyboard or mouse: a local client
    // fills this from SDL input, and a network peer or a replay supplies
    // it from the wire or a recording, so every player drives their
    // actor through exactly the same path.
    //
    // "Pressed" fields are edges: the client sets them for one tick only.
    struct PlayerInput
    {
        float MoveX = 0.0f;        // -1..1
        bool Jump = false;         // held
        bool JumpPressed = false;  // edge (for the jump sound)
        bool Jetpack = false;      // held
        bool Crouch = false;       // held

        // Aim target in world space.
        float AimX = 0.0f;
        float AimY = 0.0f;

        bool Fire = false;         // held
        bool ThrowPressed = false; // edge
        bool ReloadPressed = false;

        // Loadout slot to switch to, -1 = keep the current one.
        std::int8_t SelectSlot = -1;

        // Edge: take over the next friendly body (Cortex Command's
        // actor switching). The body you leave carries on under AI.
        bool SwitchBodyPressed = false;

        // New standing order for the team's AI units (an AllyOrder
        // value), -1 = no change.
        std::int8_t AllyOrder = -1;
    };

    // A drop-ship order, as sent by a player's buy menu. The World prices
    // it itself (the client's cart total is only a preview).
    struct OrderRequest
    {
        // One reinforcement per entry, carrying the weapon it lands with.
        std::vector<const WeaponDef*> Bodies;

        // Weapons delivered as loose field crates.
        std::vector<const WeaponDef*> LooseWeapons;

        int SupplyCrates = 0;
    };
}
