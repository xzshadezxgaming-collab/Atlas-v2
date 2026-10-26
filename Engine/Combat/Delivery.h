#pragma once

#include <cstdint>
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
        Drone,        // a packed drone that unfolds and flies off
    };

    // The ships an order can be flown in on, Cortex Command style: a
    // bigger craft costs more but carries more cargo in one trip.
    enum class CraftKind : std::uint8_t
    {
        Sparrow,
        Dropship,
        Hauler,
        Count,
    };

    struct CraftInfo
    {
        const char* Name;
        int Cost;        // gold, on top of the cargo
        int Capacity;    // cargo units (see CargoUnits)
        float Speed;     // cruise speed, px/s
        float Scale;     // hull art scale
        const char* Blurb;
    };

    const CraftInfo& GetCraftInfo(CraftKind kind);

    // Hold space one piece of cargo takes. Weapons packed into a body's
    // inventory ride free.
    int CargoUnits(DeliveryKind kind, int bodyClass = 1);

    enum class DeliveryPhase
    {
        Descending,
        Landed,
        Done,
    };

    // One piece of cargo after it leaves the craft: it parachutes down,
    // sits for a moment, then takes effect.
    struct Delivery
    {
        DeliveryKind Kind;
        DeliveryPhase Phase = DeliveryPhase::Descending;

        float PayloadX = 0.0f;
        float PayloadY = 0.0f;
        float FallVelY = 0.0f;
        float ChuteTimer = 0.0f;
        float LandedTimer = 0.0f;

        bool Delivered = false;

        // Reinforcement: the weapon it lands equipped with.
        // WeaponPickup: the weapon the field pickup grants.
        // SupplyCrate: unused (always null).
        const WeaponDef* Weapon = nullptr;

        // Reinforcement: more weapons in its inventory.
        std::vector<const WeaponDef*> Extras;

        // Player slot that ordered it (supply crates resupply them).
        int OwnerPlayer = 0;

        int Team = 0; // whose ship it is

        // Reinforcement: its build (a BodyClass value).
        int BodyClass = 1;
    };

    // What to load into a craft.
    struct Cargo
    {
        DeliveryKind Kind = DeliveryKind::SupplyCrate;
        const WeaponDef* Weapon = nullptr;
        std::vector<const WeaponDef*> Extras;
        int BodyClass = 1;
    };

    enum class CraftPhase
    {
        FlyIn,
        Hover,
        FlyOut,
    };

    struct Craft
    {
        CraftKind Kind = CraftKind::Dropship;
        CraftPhase Phase = CraftPhase::FlyIn;

        float X = 0.0f;
        float Dir = 1.0f;
        float Speed = 0.0f;
        float TargetX = 0.0f;

        float PhaseTimer = 0.0f;
        int Released = 0;

        std::vector<Cargo> Hold;

        int OwnerPlayer = 0;
        int Team = 0;

        // The craft's approach has been reported (see GetShipPasses).
        bool PassReported = false;
    };

    // Cortex Command-style order delivery: a craft flies in from off to
    // one side, brakes to a hover over the landing zone, drops its cargo
    // piece by piece (each under its own parachute), and flies off.
    //
    // This class owns only the flight/parachute physics and timing; it
    // has no idea what a "supply crate" or "reinforcement" actually does
    // gameplay-wise (heal, refill ammo, spawn an ally actor) since that
    // needs Actor/Application-level context it shouldn't depend on.
    // Update() reports back which deliveries reached their landing-
    // deliver moment this tick (each fires exactly once) so the caller
    // can apply the real effect.
    class DeliverySystem
    {
    public:
        // Sends one craft carrying the whole hold. originX: world X the
        // craft starts from (off to one side of the view). shipDir: +1
        // flies right, -1 flies left. targetX: where it hovers and drops.
        void Send(
            CraftKind kind,
            float originX,
            float shipDir,
            float targetX,
            std::vector<Cargo> hold,
            int ownerPlayer = 0,
            int team = 0);

        // A one-item craft (enemy drops).
        void Order(
            DeliveryKind kind,
            float originX,
            float shipDir,
            float targetX,
            const WeaponDef* weapon = nullptr,
            int ownerPlayer = 0,
            int team = 0,
            int bodyClass = 1);

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

        // X of each craft that started its approach to its drop point
        // during the last Update - about a second out, so a fly-by sound
        // peaks as the ship arrives.
        const std::vector<float>& GetShipPasses() const;

        const std::vector<Craft>& GetCrafts() const;

        int GetActiveCount() const;
        void Clear();

    private:
        std::vector<Craft> m_Crafts;
        std::vector<Delivery> m_Deliveries;
        std::vector<float> m_ShipPasses;
    };
}
