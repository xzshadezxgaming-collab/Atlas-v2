#pragma once

#include <vector>

namespace Atlas
{
    struct WeaponDef;

    // One reinforcement queued in the cart: always leaves with a weapon
    // (defaults to whatever the caller passes as the "bundled" weapon),
    // which can be replaced by assigning a different one.
    struct CartBody
    {
        const WeaponDef* Weapon;
        int BodyCost;
        int WeaponCost; // cost of the currently-assigned weapon (0 = bundled default)
    };

    struct CartLooseWeapon
    {
        const WeaponDef* Weapon;
        int Cost;
    };

    // Cortex Command-style order cart: bodies and weapons/items queued
    // for one drop-ship order. Buying a weapon equips it to whichever
    // body is currently "active" (most recently added, or explicitly
    // selected); with no active body, weapons are queued as loose field
    // pickups instead. Pure bookkeeping - no gold checks, no rendering,
    // no knowledge of gameplay; the caller decides affordability before
    // calling Add*, and reads the cart back out to spend gold and queue
    // deliveries.
    class BuyCart
    {
    public:
        // Adds a reinforcement with a bundled default weapon (no extra
        // charge for it - only the body's own cost is added) and makes
        // it the active assignment target.
        void AddBody(const WeaponDef* defaultWeapon, int bodyCost);

        // Assigns to the active body if one is selected (replacing
        // whatever weapon it had and adjusting the total by the cost
        // delta), otherwise queues a loose weapon. Returns true if it
        // was assigned to a body, false if it went in as a loose item.
        bool AddWeapon(const WeaponDef* weapon, int cost);

        void AddSupplyCrate(int cost);

        // Removes one supply crate from the order, if any are queued.
        void RemoveSupplyCrate();

        // -1 deselects (subsequent weapons go loose). Out-of-range
        // indices are ignored.
        void SelectBody(int index);
        void DeselectBody();

        void RemoveBody(int index);
        void RemoveLooseWeapon(int index);

        void Clear();

        int GetTotalCost() const;
        int GetActiveBodyIndex() const;
        bool IsEmpty() const;

        const std::vector<CartBody>& GetBodies() const;
        const std::vector<CartLooseWeapon>& GetLooseWeapons() const;
        int GetSupplyCrateCount() const;

    private:
        std::vector<CartBody> m_Bodies;
        std::vector<CartLooseWeapon> m_LooseWeapons;
        int m_SupplyCrateCount = 0;
        int m_SupplyCrateCostEach = 0;
        int m_Total = 0;
        int m_ActiveBody = -1;
    };
}
