#include "BuyCart.h"

namespace Atlas
{
    void BuyCart::AddBody(const WeaponDef* defaultWeapon, int bodyCost)
    {
        m_Bodies.push_back({ defaultWeapon, bodyCost, 0 });
        m_ActiveBody = static_cast<int>(m_Bodies.size()) - 1;
        m_Total += bodyCost;
    }

    bool BuyCart::AddWeapon(const WeaponDef* weapon, int cost)
    {
        if (m_ActiveBody >= 0 &&
            m_ActiveBody < static_cast<int>(m_Bodies.size()))
        {
            CartBody& body = m_Bodies[m_ActiveBody];
            m_Total -= body.WeaponCost;
            body.Weapon = weapon;
            body.WeaponCost = cost;
            m_Total += cost;
            return true;
        }

        m_LooseWeapons.push_back({ weapon, cost });
        m_Total += cost;
        return false;
    }

    void BuyCart::AddSupplyCrate(int cost)
    {
        m_SupplyCrateCount++;
        m_SupplyCrateCostEach = cost;
        m_Total += cost;
    }

    void BuyCart::RemoveSupplyCrate()
    {
        if (m_SupplyCrateCount <= 0)
            return;

        m_SupplyCrateCount--;
        m_Total -= m_SupplyCrateCostEach;
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

    void BuyCart::RemoveBody(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_Bodies.size()))
            return;

        const CartBody& body = m_Bodies[index];
        m_Total -= body.BodyCost + body.WeaponCost;

        m_Bodies.erase(m_Bodies.begin() + index);

        if (m_ActiveBody == index)
            m_ActiveBody = -1;
        else if (m_ActiveBody > index)
            m_ActiveBody--;
    }

    void BuyCart::RemoveLooseWeapon(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_LooseWeapons.size()))
            return;

        m_Total -= m_LooseWeapons[index].Cost;
        m_LooseWeapons.erase(m_LooseWeapons.begin() + index);
    }

    void BuyCart::Clear()
    {
        m_Bodies.clear();
        m_LooseWeapons.clear();
        m_SupplyCrateCount = 0;
        m_SupplyCrateCostEach = 0;
        m_Total = 0;
        m_ActiveBody = -1;
    }

    int BuyCart::GetTotalCost() const
    {
        return m_Total;
    }

    int BuyCart::GetActiveBodyIndex() const
    {
        return m_ActiveBody;
    }

    bool BuyCart::IsEmpty() const
    {
        return m_Bodies.empty() && m_LooseWeapons.empty() &&
            m_SupplyCrateCount == 0;
    }

    const std::vector<CartBody>& BuyCart::GetBodies() const
    {
        return m_Bodies;
    }

    const std::vector<CartLooseWeapon>& BuyCart::GetLooseWeapons() const
    {
        return m_LooseWeapons;
    }

    int BuyCart::GetSupplyCrateCount() const
    {
        return m_SupplyCrateCount;
    }
}
