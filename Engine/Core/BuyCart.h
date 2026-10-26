#pragma once

#include "../Combat/Delivery.h"
#include "../Sim/PlayerInput.h"

#include <vector>

namespace Atlas
{
    struct WeaponDef;

    // One reinforcement queued in the cart and the weapons packed into
    // its inventory. No weapons means it lands with the free default
    // rifle.
    struct CartBody
    {
        int Class = 1; // its build (a BodyClass value)
        std::vector<const WeaponDef*> Weapons;
        int Id = 0;    // stable while it is in the cart (for undo)
    };

    // Cortex Command-style order cart: a craft, then bodies with their
    // gear, then loose cargo. Buying a weapon packs it into whichever
    // body is "active" (the most recently added, or one picked in the
    // order list); with no active body it rides as a loose field pickup.
    //
    // Pure bookkeeping - no gold checks, no rendering, no gameplay. The
    // World prices an order itself (World::PriceOrder on ToOrder()), so
    // the cart never keeps its own copy of the price list.
    class BuyCart
    {
    public:
        static constexpr int MaxWeaponsPerBody = 4;
        static constexpr int AutoCraft = -1;

        enum class AddResult
        {
            ToBody,    // packed into the active body
            Loose,     // queued as a loose field pickup
            BodyFull,  // the active body can't carry more
            Duplicate, // the active body already carries one
        };

        void AddBody(int bodyClass);

        AddResult AddWeapon(const WeaponDef* weapon);

        void AddSupplyCrate();
        void AddDrone();

        void RemoveBody(int index);
        void RemoveBodyWeapon(int bodyIndex, int weaponIndex);
        void RemoveLooseWeapon(int index);
        void RemoveSupplyCrate();
        void RemoveDrone();

        // Takes back the most recent addition still in the cart.
        void Undo();

        // -1 deselects (subsequent weapons go loose). Out-of-range
        // indices are ignored.
        void SelectBody(int index);
        void DeselectBody();
        int GetActiveBodyIndex() const;

        // AutoCraft, or a CraftKind value to always fly in.
        void SetCraftChoice(int choice);
        int GetCraftChoice() const;

        // The craft the order would fly on: the chosen one, or on auto
        // the smallest that fits (the biggest if nothing does).
        CraftKind ResolveCraft() const;

        void Clear();
        bool IsEmpty() const;

        const std::vector<CartBody>& GetBodies() const;
        const std::vector<const WeaponDef*>& GetLooseWeapons() const;
        int GetSupplyCrateCount() const;
        int GetDroneCount() const;
        int GetItemCount() const; // weapons, crates and drones, any body

        // The order as the World takes it (the default rifle fills in
        // for bodies without weapons). DropX is left on the buyer.
        OrderRequest ToOrder(const WeaponDef* defaultWeapon) const;

        // Replaces the cart with an order's contents (loading a preset).
        void FromOrder(const OrderRequest& order, const WeaponDef* defaultWeapon);

    private:
        enum class AddKind
        {
            Body,
            BodyWeapon,
            LooseWeapon,
            Crate,
            Drone,
        };

        struct Addition
        {
            AddKind Kind;
            int BodyId;
            const WeaponDef* Weapon;
        };

        int FindBody(int id) const;

        std::vector<CartBody> m_Bodies;
        std::vector<const WeaponDef*> m_LooseWeapons;
        std::vector<Addition> m_History;
        int m_SupplyCrates = 0;
        int m_Drones = 0;
        int m_ActiveBody = -1;
        int m_CraftChoice = AutoCraft;
        int m_NextId = 1;
    };
}
