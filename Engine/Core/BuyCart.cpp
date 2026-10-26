#include "BuyCart.h"

#include "../Sim/World.h"

#include <algorithm>

namespace Atlas
{
    void BuyCart::AddBody(int bodyClass)
    {
        CartBody body;
        body.Class = bodyClass;
        body.Id = m_NextId++;

        m_Bodies.push_back(body);
        m_ActiveBody = static_cast<int>(m_Bodies.size()) - 1;
        m_History.push_back({ AddKind::Body, body.Id, nullptr });
    }

    BuyCart::AddResult BuyCart::AddWeapon(const WeaponDef* weapon)
    {
        if (m_ActiveBody >= 0 &&
            m_ActiveBody < static_cast<int>(m_Bodies.size()))
        {
            CartBody& body = m_Bodies[static_cast<std::size_t>(m_ActiveBody)];

            if (std::find(body.Weapons.begin(), body.Weapons.end(), weapon) != body.Weapons.end())
                return AddResult::Duplicate;

            if (static_cast<int>(body.Weapons.size()) >= MaxWeaponsPerBody)
                return AddResult::BodyFull;

            body.Weapons.push_back(weapon);
            m_History.push_back({ AddKind::BodyWeapon, body.Id, weapon });
            return AddResult::ToBody;
        }

        m_LooseWeapons.push_back(weapon);
        m_History.push_back({ AddKind::LooseWeapon, 0, weapon });
        return AddResult::Loose;
    }

    void BuyCart::AddSupplyCrate()
    {
        m_SupplyCrates++;
        m_History.push_back({ AddKind::Crate, 0, nullptr });
    }

    void BuyCart::AddDrone()
    {
        m_Drones++;
        m_History.push_back({ AddKind::Drone, 0, nullptr });
    }

    void BuyCart::RemoveBody(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_Bodies.size()))
            return;

        m_Bodies.erase(m_Bodies.begin() + index);

        if (m_ActiveBody == index)
            m_ActiveBody = m_Bodies.empty() ? -1 : static_cast<int>(m_Bodies.size()) - 1;
        else if (m_ActiveBody > index)
            m_ActiveBody--;
    }

    void BuyCart::RemoveBodyWeapon(int bodyIndex, int weaponIndex)
    {
        if (bodyIndex < 0 || bodyIndex >= static_cast<int>(m_Bodies.size()))
            return;

        std::vector<const WeaponDef*>& weapons =
            m_Bodies[static_cast<std::size_t>(bodyIndex)].Weapons;

        if (weaponIndex >= 0 && weaponIndex < static_cast<int>(weapons.size()))
            weapons.erase(weapons.begin() + weaponIndex);
    }

    void BuyCart::RemoveLooseWeapon(int index)
    {
        if (index >= 0 && index < static_cast<int>(m_LooseWeapons.size()))
            m_LooseWeapons.erase(m_LooseWeapons.begin() + index);
    }

    void BuyCart::RemoveSupplyCrate()
    {
        m_SupplyCrates = std::max(0, m_SupplyCrates - 1);
    }

    void BuyCart::RemoveDrone()
    {
        m_Drones = std::max(0, m_Drones - 1);
    }

    int BuyCart::FindBody(int id) const
    {
        for (std::size_t i = 0; i < m_Bodies.size(); i++)
        {
            if (m_Bodies[i].Id == id)
                return static_cast<int>(i);
        }

        return -1;
    }

    void BuyCart::Undo()
    {
        // Entries whose item was already removed by hand are skipped.
        while (!m_History.empty())
        {
            const Addition last = m_History.back();
            m_History.pop_back();

            switch (last.Kind)
            {
            case AddKind::Body:
            {
                const int index = FindBody(last.BodyId);

                if (index >= 0)
                {
                    RemoveBody(index);
                    return;
                }
                break;
            }

            case AddKind::BodyWeapon:
            {
                const int index = FindBody(last.BodyId);

                if (index >= 0)
                {
                    std::vector<const WeaponDef*>& weapons =
                        m_Bodies[static_cast<std::size_t>(index)].Weapons;
                    const auto it = std::find(weapons.begin(), weapons.end(), last.Weapon);

                    if (it != weapons.end())
                    {
                        weapons.erase(it);
                        return;
                    }
                }
                break;
            }

            case AddKind::LooseWeapon:
            {
                const auto it = std::find(m_LooseWeapons.rbegin(), m_LooseWeapons.rend(), last.Weapon);

                if (it != m_LooseWeapons.rend())
                {
                    m_LooseWeapons.erase(std::next(it).base());
                    return;
                }
                break;
            }

            case AddKind::Crate:
                if (m_SupplyCrates > 0)
                {
                    m_SupplyCrates--;
                    return;
                }
                break;

            case AddKind::Drone:
                if (m_Drones > 0)
                {
                    m_Drones--;
                    return;
                }
                break;
            }
        }
    }

    void BuyCart::SelectBody(int index)
    {
        if (index >= 0 && index < static_cast<int>(m_Bodies.size()))
            m_ActiveBody = index;
    }

    void BuyCart::DeselectBody()
    {
        m_ActiveBody = -1;
    }

    int BuyCart::GetActiveBodyIndex() const
    {
        return m_ActiveBody;
    }

    void BuyCart::SetCraftChoice(int choice)
    {
        m_CraftChoice = std::clamp(choice, AutoCraft, static_cast<int>(CraftKind::Count) - 1);
    }

    int BuyCart::GetCraftChoice() const
    {
        return m_CraftChoice;
    }

    CraftKind BuyCart::ResolveCraft() const
    {
        if (m_CraftChoice != AutoCraft)
            return static_cast<CraftKind>(m_CraftChoice);

        const int units = World::OrderCargoUnits(ToOrder(nullptr));

        for (int c = 0; c < static_cast<int>(CraftKind::Count); c++)
        {
            if (GetCraftInfo(static_cast<CraftKind>(c)).Capacity >= units)
                return static_cast<CraftKind>(c);
        }

        return static_cast<CraftKind>(static_cast<int>(CraftKind::Count) - 1);
    }

    void BuyCart::Clear()
    {
        m_Bodies.clear();
        m_LooseWeapons.clear();
        m_History.clear();
        m_SupplyCrates = 0;
        m_Drones = 0;
        m_ActiveBody = -1;
    }

    bool BuyCart::IsEmpty() const
    {
        return m_Bodies.empty() && m_LooseWeapons.empty() &&
            m_SupplyCrates == 0 && m_Drones == 0;
    }

    const std::vector<CartBody>& BuyCart::GetBodies() const
    {
        return m_Bodies;
    }

    const std::vector<const WeaponDef*>& BuyCart::GetLooseWeapons() const
    {
        return m_LooseWeapons;
    }

    int BuyCart::GetSupplyCrateCount() const
    {
        return m_SupplyCrates;
    }

    int BuyCart::GetDroneCount() const
    {
        return m_Drones;
    }

    int BuyCart::GetItemCount() const
    {
        int count = static_cast<int>(m_LooseWeapons.size()) + m_SupplyCrates + m_Drones;

        for (const CartBody& body : m_Bodies)
            count += static_cast<int>(body.Weapons.size());

        return count;
    }

    OrderRequest BuyCart::ToOrder(const WeaponDef* defaultWeapon) const
    {
        OrderRequest order;

        // Resolving auto here would recurse; callers that send it fill
        // in ResolveCraft(), and cargo counting doesn't need it.
        order.Craft = m_CraftChoice == AutoCraft
            ? static_cast<std::uint8_t>(0)
            : static_cast<std::uint8_t>(m_CraftChoice);

        for (const CartBody& body : m_Bodies)
        {
            OrderBody entry;
            entry.Class = static_cast<std::uint8_t>(body.Class);
            entry.Weapon = body.Weapons.empty() ? defaultWeapon : body.Weapons.front();

            for (std::size_t i = 1; i < body.Weapons.size(); i++)
                entry.Extras.push_back(body.Weapons[i]);

            order.Bodies.push_back(entry);
        }

        order.LooseWeapons = m_LooseWeapons;
        order.SupplyCrates = m_SupplyCrates;
        order.Drones = m_Drones;

        return order;
    }

    void BuyCart::FromOrder(const OrderRequest& order, const WeaponDef* defaultWeapon)
    {
        Clear();

        for (const OrderBody& body : order.Bodies)
        {
            AddBody(body.Class);

            // The free default only needs listing when gear follows it.
            if (body.Weapon && (body.Weapon != defaultWeapon || !body.Extras.empty()))
                AddWeapon(body.Weapon);

            for (const WeaponDef* extra : body.Extras)
                AddWeapon(extra);
        }

        DeselectBody();

        for (const WeaponDef* weapon : order.LooseWeapons)
            AddWeapon(weapon);

        for (int i = 0; i < order.SupplyCrates; i++)
            AddSupplyCrate();

        for (int i = 0; i < order.Drones; i++)
            AddDrone();

        m_ActiveBody = m_Bodies.empty() ? -1 : static_cast<int>(m_Bodies.size()) - 1;
    }
}
