#pragma once

#include <vector>

namespace Atlas
{
    class Terrain;
    class Window;
    struct WeaponDef;

    enum class DeliveryKind
    {
        SupplyCrate,
        Reinforcement,
        WeaponPickup, // a loose weapon crate that becomes a field pickup
    };

    enum class DeliveryPhase
    {
        FlyIn,
        Descending,
        Landed,
        Done,
    };

    struct Delivery
    {
        DeliveryKind Kind;
        DeliveryPhase Phase = DeliveryPhase::FlyIn;

        float ShipX = 0.0f;
        float ShipDir = 1.0f;

        float PayloadX = 0.0f;
        float PayloadY = 0.0f;
        float FallVelY = 0.0f;
        float ChuteTimer = 0.0f;
        float LandedTimer = 0.0f;

        float TargetX = 0.0f;
        bool Delivered = false;

        // Reinforcement: the weapon it lands equipped with.
        // WeaponPickup: the weapon the field pickup grants.
        // SupplyCrate: unused (always null).
        const WeaponDef* Weapon = nullptr;
    };

    // Cortex Command-style order delivery: a drop ship flies in from off
    // to one side, releases its cargo once above the target, and the
    // payload parachutes down to the ground.
    //
    // This class owns only the ship-flight/parachute-physics/timing; it
    // has no idea what a "supply crate" or "reinforcement" actually does
    // gameplay-wise (heal, refill ammo, spawn an ally actor) since that
    // needs Actor/Application-level context it shouldn't depend on.
    // Update() reports back which deliveries reached their landing-
    // deliver moment this tick (each fires exactly once) so the caller
    // can apply the real effect.
    class DeliverySystem
    {
    public:
        // originX: world X the ship starts from (already offset off to
        // one side of the view). shipDir: +1 flies right, -1 flies left.
        // targetX: world X to fly to and release the payload above.
        // weapon: only meaningful for Reinforcement (equipped on landing)
        // and WeaponPickup (what the field pickup grants).
        void Order(
            DeliveryKind kind,
            float originX,
            float shipDir,
            float targetX,
            const WeaponDef* weapon = nullptr);

        // outLanded: deliveries whose payload touched down this tick (for
        // a landing thud). outDelivered: deliveries that reached their
        // landing-deliver moment this tick (each fires exactly once) so
        // the caller can apply the real effect - heal, refill ammo, spawn
        // an ally actor.
        void Update(
            const Terrain& terrain,
            float deltaTime,
            std::vector<const Delivery*>& outLanded,
            std::vector<const Delivery*>& outDelivered);

        void Draw(Window& window) const;

        int GetActiveCount() const;
        void Clear();

    private:
        std::vector<Delivery> m_Deliveries;
    };
}
