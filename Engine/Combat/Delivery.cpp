#include "Delivery.h"

#include "../Core/Window.h"
#include "../Graphics/SpriteBank.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float DeliveryAltitude = 24.0f;
        constexpr float CraftBrake = 520.0f;      // px/s^2 when braking to hover
        constexpr float MinApproachSpeed = 40.0f;
        constexpr float HoverSettle = 0.35f;      // before the first drop
        constexpr float ReleaseInterval = 0.28f;  // between drops
        constexpr float CargoSpread = 9.0f;
        constexpr float ShipExitMargin = 400.0f;
        constexpr float ChuteDeployDelay = 0.35f;
        constexpr float ChuteFallSpeed = 70.0f;
        constexpr float FreeFallGravity = 640.0f;
        constexpr float DeliverAfter = 1.1f; // seconds sitting landed
        constexpr float RemoveAfter = 2.6f;  // seconds landed before cleanup

        void DrawThinLine(
            Window& window,
            float x0, float y0,
            float x1, float y1,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);
            const int steps = std::max(1, static_cast<int>(length / 2.0f));

            for (int i = 0; i <= steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    x0 + dx * t - 0.75f, y0 + dy * t - 0.75f,
                    1.5f, 1.5f, r, g, b, a);
            }
        }

        // Procedural drop-ship hull: a sleek wedge with a glowing tail
        // engine, nose pointed the direction it's flying.
        void DrawDropShipHull(Window& window, float x, float y, float dir, int team,
            float scale, float thrust)
        {
            if (const SpriteFrame* sprite = SpriteBank::Find(team == 0 ? "ship_team0" : "ship_team1"))
            {
                window.DrawSprite(*sprite, x, y, 0.0f, dir < 0.0f, false, scale);

                const float tail = x - dir * 25.0f * scale;
                window.DrawGlow(tail, y + 1.0f, 14.0f * scale * (0.5f + 0.5f * thrust),
                    255, 170, 90, static_cast<Uint8>(60.0f + 100.0f * thrust));
                window.DrawFilledRect(tail - dir * scale - 2.0f, y - 1.0f, 4.0f, 4.0f,
                    255, 220, 160, 255);
                return;
            }

            (void)scale;
            (void)thrust;

            window.DrawFilledRect(x - 22.0f, y - 6.0f, 44.0f, 12.0f,
                40, 42, 50, 255);

            // Team stripe along the spine.
            if (team == 0)
                window.DrawFilledRect(x - 22.0f, y - 6.0f, 44.0f, 3.0f, 72, 76, 90, 255);
            else
                window.DrawFilledRect(x - 22.0f, y - 6.0f, 44.0f, 3.0f, 150, 50, 44, 255);

            window.DrawFilledRect(x + dir * 8.0f - 5.0f, y - 3.0f, 10.0f, 6.0f,
                130, 205, 235, 220);

            window.DrawFilledRect(x - dir * 20.0f - 3.0f, y - 12.0f, 6.0f, 9.0f,
                40, 42, 50, 255);

            window.DrawGlow(x - dir * 24.0f, y, 14.0f, 255, 170, 90, 160);
            window.DrawFilledRect(x - dir * 26.0f - 2.0f, y - 2.0f, 4.0f, 4.0f,
                255, 220, 160, 255);
        }

        // canopyOpen: 0 = not yet deployed, 1 = fully blossomed.
        void DrawParachute(
            Window& window, float payloadX, float payloadY, float canopyOpen)
        {
            if (canopyOpen <= 0.0f)
                return;

            const float width = 34.0f * canopyOpen;
            const float canopyY = payloadY - 30.0f * canopyOpen - 6.0f;

            window.DrawFilledRect(payloadX - width * 0.50f, canopyY + 6.0f,
                width, 6.0f, 200, 70, 60, 230);
            window.DrawFilledRect(payloadX - width * 0.40f, canopyY + 2.0f,
                width * 0.80f, 5.0f, 212, 92, 72, 230);
            window.DrawFilledRect(payloadX - width * 0.28f, canopyY - 2.0f,
                width * 0.56f, 5.0f, 224, 112, 84, 230);
            window.DrawFilledRect(payloadX - width * 0.12f, canopyY - 5.0f,
                width * 0.24f, 4.0f, 232, 132, 92, 230);

            DrawThinLine(window, payloadX - width * 0.42f, canopyY + 10.0f,
                payloadX - 3.0f, payloadY - 12.0f, 210, 210, 200, 200);
            DrawThinLine(window, payloadX + width * 0.42f, canopyY + 10.0f,
                payloadX + 3.0f, payloadY - 12.0f, 210, 210, 200, 200);
        }

        void DrawDeliveryPayload(Window& window, float x, float y, DeliveryKind kind, int team)
        {
            const char* crate =
                kind == DeliveryKind::Drone ? "crate_drone"
                : kind == DeliveryKind::WeaponPickup ? "crate_weapon"
                : kind == DeliveryKind::Reinforcement ? nullptr
                : "crate_supply";

            if (const SpriteFrame* sprite = crate ? SpriteBank::Find(crate) : nullptr)
            {
                window.DrawSprite(*sprite, x, y);

                if (kind == DeliveryKind::Drone)
                {
                    window.DrawFilledRect(x, y - 4.0f, 2.0f, 1.0f,
                        team == 0 ? 90 : 255, team == 0 ? 170 : 80, team == 0 ? 255 : 60, 255);
                }
                return;
            }

            if (kind == DeliveryKind::Reinforcement)
            {
                // Soldier hanging in the harness, in team colors.
                if (team == 0)
                    window.DrawFilledRect(x - 4.0f, y - 16.0f, 8.0f, 10.0f, 70, 96, 130, 255);
                else
                    window.DrawFilledRect(x - 4.0f, y - 16.0f, 8.0f, 10.0f, 150, 60, 52, 255);
                window.DrawFilledRect(x - 3.0f, y - 20.0f, 6.0f, 5.0f,
                    214, 170, 130, 255);
                window.DrawFilledRect(x - 4.0f, y - 6.0f, 3.0f, 6.0f,
                    50, 54, 62, 255);
                window.DrawFilledRect(x + 1.0f, y - 6.0f, 3.0f, 6.0f,
                    50, 54, 62, 255);
            }
            else if (kind == DeliveryKind::Drone)
            {
                // Folded drone: steel case, rotor arms tucked, team light.
                window.DrawFilledRect(x - 7.0f, y - 12.0f, 14.0f, 10.0f, 24, 22, 27, 255);
                window.DrawFilledRect(x - 6.0f, y - 11.0f, 12.0f, 8.0f, 92, 98, 110, 255);
                window.DrawFilledRect(x - 10.0f, y - 12.0f, 20.0f, 2.0f, 50, 52, 60, 255);
                window.DrawFilledRect(x - 1.0f, y - 8.0f, 2.0f, 2.0f,
                    team == 0 ? 90 : 255, team == 0 ? 170 : 80, team == 0 ? 255 : 60, 255);
            }
            else if (kind == DeliveryKind::WeaponPickup)
            {
                // Olive-drab weapon case, distinct from the wooden
                // supply crate.
                window.DrawFilledRect(x - 8.0f, y - 14.0f, 16.0f, 12.0f,
                    24, 22, 27, 255);
                window.DrawFilledRect(x - 7.0f, y - 13.0f, 14.0f, 10.0f,
                    72, 82, 56, 255);
                window.DrawFilledRect(x - 7.0f, y - 13.0f, 14.0f, 2.0f,
                    92, 104, 70, 255);
                window.DrawFilledRect(x - 1.0f, y - 13.0f, 2.0f, 10.0f,
                    48, 54, 38, 255);
                window.DrawFilledRect(x - 3.0f, y - 15.0f, 6.0f, 2.0f,
                    48, 54, 38, 255);
            }
            else
            {
                // Wooden supply crate with cross straps.
                window.DrawFilledRect(x - 8.0f, y - 16.0f, 16.0f, 14.0f,
                    24, 22, 27, 255);
                window.DrawFilledRect(x - 7.0f, y - 15.0f, 14.0f, 12.0f,
                    128, 96, 58, 255);
                window.DrawFilledRect(x - 7.0f, y - 10.0f, 14.0f, 2.0f,
                    96, 70, 40, 255);
                window.DrawFilledRect(x - 1.0f, y - 15.0f, 2.0f, 12.0f,
                    96, 70, 40, 255);
            }
        }
    }

    const CraftInfo& GetCraftInfo(CraftKind kind)
    {
        static const CraftInfo Crafts[] =
        {
            { "SPARROW",   0,  5, 640.0f, 0.80f, "FAST LIGHT COURIER. FREE." },
            { "DROPSHIP", 25, 12, 480.0f, 1.00f, "THE WORKHORSE. ROOM FOR A SQUAD." },
            { "HAULER",   60, 24, 380.0f, 1.35f, "SLOW HEAVY LIFTER. BRINGS AN ARMY." },
        };

        static_assert(sizeof(Crafts) / sizeof(Crafts[0]) ==
            static_cast<std::size_t>(CraftKind::Count));

        const int index = std::clamp(static_cast<int>(kind), 0,
            static_cast<int>(CraftKind::Count) - 1);

        return Crafts[index];
    }

    int CargoUnits(DeliveryKind kind, int bodyClass)
    {
        switch (kind)
        {
        case DeliveryKind::Reinforcement:
            return bodyClass <= 0 ? 2 : bodyClass == 1 ? 3 : 4;
        case DeliveryKind::Drone:
            return 2;
        case DeliveryKind::WeaponPickup:
        case DeliveryKind::SupplyCrate:
        default:
            return 1;
        }
    }

    void DeliverySystem::Send(
        CraftKind kind,
        float originX,
        float shipDir,
        float targetX,
        std::vector<Cargo> hold,
        int ownerPlayer,
        int team)
    {
        if (hold.empty())
            return;

        Craft craft;
        craft.Kind = kind;
        craft.X = originX;
        craft.Dir = shipDir;
        craft.Speed = GetCraftInfo(kind).Speed;
        craft.TargetX = targetX;
        craft.Hold = std::move(hold);
        craft.OwnerPlayer = ownerPlayer;
        craft.Team = team;

        m_Crafts.push_back(std::move(craft));
    }

    void DeliverySystem::Order(
        DeliveryKind kind,
        float originX,
        float shipDir,
        float targetX,
        const WeaponDef* weapon,
        int ownerPlayer,
        int team,
        int bodyClass)
    {
        Cargo cargo;
        cargo.Kind = kind;
        cargo.Weapon = weapon;
        cargo.BodyClass = bodyClass;

        std::vector<Cargo> hold;
        hold.push_back(std::move(cargo));

        Send(CraftKind::Dropship, originX, shipDir, targetX, std::move(hold),
            ownerPlayer, team);
    }

    void DeliverySystem::Update(
        const Terrain& terrain,
        float deltaTime,
        std::vector<const Delivery*>& outLanded,
        std::vector<const Delivery*>& outDelivered)
    {
        outLanded.clear();
        outDelivered.clear();
        m_ShipPasses.clear();

        // Finished payloads go first, so pointers handed out below stay
        // valid until the next Update.
        m_Deliveries.erase(
            std::remove_if(m_Deliveries.begin(), m_Deliveries.end(),
                [](const Delivery& d) { return d.Phase == DeliveryPhase::Done; }),
            m_Deliveries.end());

        const float worldWidth = static_cast<float>(terrain.GetWidth());

        for (std::size_t i = 0; i < m_Crafts.size(); )
        {
            Craft& craft = m_Crafts[i];
            const CraftInfo& info = GetCraftInfo(craft.Kind);
            bool gone = false;

            switch (craft.Phase)
            {
            case CraftPhase::FlyIn:
            {
                const float dist = (craft.TargetX - craft.X) * craft.Dir;

                if (!craft.PassReported && dist < info.Speed * 1.1f)
                {
                    craft.PassReported = true;
                    m_ShipPasses.push_back(craft.TargetX);
                }

                // Cruise, then brake so it comes to a stop on the mark.
                const float wanted = std::clamp(
                    std::sqrt(2.0f * CraftBrake * std::max(dist, 0.0f)),
                    MinApproachSpeed, info.Speed);

                craft.Speed = wanted;

                const float step = craft.Speed * deltaTime;

                if (dist <= step)
                {
                    craft.X = craft.TargetX;
                    craft.Speed = 0.0f;
                    craft.Phase = CraftPhase::Hover;
                    craft.PhaseTimer = 0.0f;
                }
                else
                {
                    craft.X += craft.Dir * step;
                }
                break;
            }

            case CraftPhase::Hover:
            {
                craft.PhaseTimer += deltaTime;

                const int count = static_cast<int>(craft.Hold.size());

                while (craft.Released < count &&
                    craft.PhaseTimer >= HoverSettle +
                        static_cast<float>(craft.Released) * ReleaseInterval)
                {
                    const int n = craft.Released;
                    Cargo& cargo = craft.Hold[static_cast<std::size_t>(n)];

                    // Spread the drop either side of the mark.
                    const float side = (n % 2 == 0) ? -1.0f : 1.0f;
                    const float offset = static_cast<float>((n + 1) / 2) * CargoSpread * side;

                    Delivery delivery;
                    delivery.Kind = cargo.Kind;
                    delivery.PayloadX = std::clamp(craft.X + offset, 8.0f, worldWidth - 8.0f);
                    delivery.PayloadY = DeliveryAltitude + 10.0f * info.Scale;
                    delivery.FallVelY = 30.0f;
                    delivery.Weapon = cargo.Weapon;
                    delivery.Extras = std::move(cargo.Extras);
                    delivery.OwnerPlayer = craft.OwnerPlayer;
                    delivery.Team = craft.Team;
                    delivery.BodyClass = cargo.BodyClass;

                    m_Deliveries.push_back(std::move(delivery));
                    craft.Released++;
                }

                if (craft.Released >= count &&
                    craft.PhaseTimer >= HoverSettle +
                        static_cast<float>(count) * ReleaseInterval + 0.3f)
                {
                    craft.Phase = CraftPhase::FlyOut;
                    craft.PhaseTimer = 0.0f;
                }
                break;
            }

            case CraftPhase::FlyOut:
            default:
                craft.Speed = std::min(craft.Speed + 520.0f * deltaTime, info.Speed * 1.2f);
                craft.X += craft.Dir * craft.Speed * deltaTime;
                gone = craft.X < -ShipExitMargin || craft.X > worldWidth + ShipExitMargin;
                break;
            }

            if (gone)
                m_Crafts.erase(m_Crafts.begin() + static_cast<std::ptrdiff_t>(i));
            else
                i++;
        }

        for (Delivery& delivery : m_Deliveries)
        {
            switch (delivery.Phase)
            {
            case DeliveryPhase::Descending:
            {
                delivery.ChuteTimer += deltaTime;
                const bool chuteOpen = delivery.ChuteTimer > ChuteDeployDelay;

                delivery.FallVelY +=
                    (chuteOpen ? 90.0f : FreeFallGravity) * deltaTime;

                if (chuteOpen)
                    delivery.FallVelY = std::min(delivery.FallVelY, ChuteFallSpeed);

                delivery.PayloadY += delivery.FallVelY * deltaTime;

                if (terrain.IsSolid(delivery.PayloadX, delivery.PayloadY))
                {
                    while (delivery.PayloadY > 0.0f &&
                        terrain.IsSolid(delivery.PayloadX, delivery.PayloadY))
                    {
                        delivery.PayloadY -= 1.0f;
                    }

                    delivery.Phase = DeliveryPhase::Landed;
                    delivery.LandedTimer = 0.0f;
                    outLanded.push_back(&delivery);
                }
                break;
            }

            case DeliveryPhase::Landed:
            {
                delivery.LandedTimer += deltaTime;

                if (!delivery.Delivered && delivery.LandedTimer >= DeliverAfter)
                {
                    delivery.Delivered = true;
                    outDelivered.push_back(&delivery);
                }

                if (delivery.LandedTimer >= RemoveAfter)
                    delivery.Phase = DeliveryPhase::Done;

                break;
            }

            case DeliveryPhase::Done:
            default:
                break;
            }
        }
    }

    void DeliverySystem::Draw(Window& window) const
    {
        const float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;

        for (const Craft& craft : m_Crafts)
        {
            const CraftInfo& info = GetCraftInfo(craft.Kind);
            const bool hovering = craft.Phase == CraftPhase::Hover;

            // A gentle bob while it holds station, eased in and out.
            const float bob = hovering
                ? std::sin(time * 4.0f + craft.TargetX) * 1.5f *
                    std::min(1.0f, craft.PhaseTimer * 3.0f)
                : 0.0f;

            const float y = DeliveryAltitude + bob;

            DrawDropShipHull(window, craft.X, y, craft.Dir, craft.Team, info.Scale,
                hovering ? 0.35f : 1.0f);

            if (hovering)
            {
                // Downward thrusters holding it up.
                for (int side = -1; side <= 1; side += 2)
                {
                    const float tx = craft.X + static_cast<float>(side) * 12.0f * info.Scale;
                    const float flicker = 0.8f + 0.2f * std::sin(time * 37.0f + static_cast<float>(side));

                    window.DrawGlow(tx, y + 8.0f * info.Scale, 10.0f * info.Scale * flicker,
                        255, 170, 90, 150);
                    window.DrawFilledRect(tx - 1.0f, y + 6.0f * info.Scale, 2.0f,
                        4.0f * flicker, 255, 225, 170, 230);
                }
            }
        }

        for (const Delivery& delivery : m_Deliveries)
        {
            if (delivery.Phase == DeliveryPhase::Descending)
            {
                const float canopyOpen = std::clamp(
                    (delivery.ChuteTimer - ChuteDeployDelay) / 0.25f,
                    0.0f, 1.0f);

                DrawParachute(
                    window, delivery.PayloadX, delivery.PayloadY, canopyOpen);
                DrawDeliveryPayload(
                    window, delivery.PayloadX, delivery.PayloadY,
                    delivery.Kind, delivery.Team);
            }
            else if (delivery.Phase == DeliveryPhase::Landed &&
                !delivery.Delivered)
            {
                DrawDeliveryPayload(
                    window, delivery.PayloadX, delivery.PayloadY,
                    delivery.Kind, delivery.Team);
                window.DrawGlow(
                    delivery.PayloadX, delivery.PayloadY - 8.0f, 16.0f,
                    230, 205, 110, 90);
            }
        }
    }

    const std::vector<float>& DeliverySystem::GetShipPasses() const
    {
        return m_ShipPasses;
    }

    const std::vector<Craft>& DeliverySystem::GetCrafts() const
    {
        return m_Crafts;
    }

    int DeliverySystem::GetActiveCount() const
    {
        return static_cast<int>(m_Crafts.size() + m_Deliveries.size());
    }

    void DeliverySystem::Clear()
    {
        m_Crafts.clear();
        m_Deliveries.clear();
    }
}
