#include "Delivery.h"

#include "../Core/Window.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float DeliveryAltitude = 24.0f;
        constexpr float ShipSpeed = 480.0f;
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
        void DrawDropShipHull(Window& window, float x, float y, float dir)
        {
            window.DrawFilledRect(x - 22.0f, y - 6.0f, 44.0f, 12.0f,
                40, 42, 50, 255);
            window.DrawFilledRect(x - 22.0f, y - 6.0f, 44.0f, 3.0f,
                72, 76, 90, 255);

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

        void DrawDeliveryPayload(
            Window& window, float x, float y, bool isReinforcement)
        {
            if (isReinforcement)
            {
                // Soldier hanging in the harness.
                window.DrawFilledRect(x - 4.0f, y - 16.0f, 8.0f, 10.0f,
                    70, 96, 130, 255);
                window.DrawFilledRect(x - 3.0f, y - 20.0f, 6.0f, 5.0f,
                    214, 170, 130, 255);
                window.DrawFilledRect(x - 4.0f, y - 6.0f, 3.0f, 6.0f,
                    50, 54, 62, 255);
                window.DrawFilledRect(x + 1.0f, y - 6.0f, 3.0f, 6.0f,
                    50, 54, 62, 255);
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

    void DeliverySystem::Order(
        DeliveryKind kind, float originX, float shipDir, float targetX)
    {
        Delivery delivery;
        delivery.Kind = kind;
        delivery.ShipX = originX;
        delivery.ShipDir = shipDir;
        delivery.TargetX = targetX;

        m_Deliveries.push_back(delivery);
    }

    void DeliverySystem::Update(
        const Terrain& terrain,
        float deltaTime,
        std::vector<const Delivery*>& outLanded,
        std::vector<const Delivery*>& outDelivered)
    {
        outLanded.clear();
        outDelivered.clear();

        for (std::size_t i = 0; i < m_Deliveries.size(); )
        {
            Delivery& delivery = m_Deliveries[i];

            switch (delivery.Phase)
            {
            case DeliveryPhase::FlyIn:
            {
                delivery.ShipX += delivery.ShipDir * ShipSpeed * deltaTime;
                delivery.PayloadX = delivery.ShipX;
                delivery.PayloadY = DeliveryAltitude;

                const bool arrived = delivery.ShipDir > 0.0f
                    ? delivery.ShipX >= delivery.TargetX
                    : delivery.ShipX <= delivery.TargetX;

                if (arrived)
                {
                    delivery.Phase = DeliveryPhase::Descending;
                    delivery.FallVelY = 30.0f;
                    delivery.ChuteTimer = 0.0f;
                }
                break;
            }

            case DeliveryPhase::Descending:
            {
                // The ship flies on through and off the scene; the
                // payload now falls under its own chute.
                delivery.ShipX += delivery.ShipDir * ShipSpeed * deltaTime;

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

            if (delivery.Phase == DeliveryPhase::Done)
            {
                m_Deliveries.erase(m_Deliveries.begin() +
                    static_cast<std::ptrdiff_t>(i));
            }
            else
            {
                i++;
            }
        }
    }

    void DeliverySystem::Draw(Window& window) const
    {
        for (const Delivery& delivery : m_Deliveries)
        {
            const bool isReinforcement =
                delivery.Kind == DeliveryKind::Reinforcement;

            if (delivery.Phase == DeliveryPhase::FlyIn ||
                delivery.Phase == DeliveryPhase::Descending)
            {
                DrawDropShipHull(
                    window, delivery.ShipX, DeliveryAltitude, delivery.ShipDir);
            }

            if (delivery.Phase == DeliveryPhase::Descending)
            {
                const float canopyOpen = std::clamp(
                    (delivery.ChuteTimer - ChuteDeployDelay) / 0.25f,
                    0.0f, 1.0f);

                DrawParachute(
                    window, delivery.PayloadX, delivery.PayloadY, canopyOpen);
                DrawDeliveryPayload(
                    window, delivery.PayloadX, delivery.PayloadY,
                    isReinforcement);
            }
            else if (delivery.Phase == DeliveryPhase::Landed &&
                !delivery.Delivered)
            {
                DrawDeliveryPayload(
                    window, delivery.PayloadX, delivery.PayloadY,
                    isReinforcement);
                window.DrawGlow(
                    delivery.PayloadX, delivery.PayloadY - 8.0f, 16.0f,
                    230, 205, 110, 90);
            }
        }
    }

    int DeliverySystem::GetActiveCount() const
    {
        return static_cast<int>(m_Deliveries.size());
    }

    void DeliverySystem::Clear()
    {
        m_Deliveries.clear();
    }
}
