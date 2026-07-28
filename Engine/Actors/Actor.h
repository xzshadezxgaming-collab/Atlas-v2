#pragma once

#include "BodyPart.h"
#include "Limb.h"

#include "../Combat/Weapon.h"
#include "../Graphics/Texture2D.h"

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class ParticleSystem;
    class Terrain;
    class Window;

    // Independent, individually-destructible body parts. Only one arm
    // exists as an interactive element (the far arm is baked into the
    // torso sprite), so it stands alone rather than as a Left/Right pair.
    enum class BodyPartId
    {
        Head,
        Torso,
        Arm,
        LegBack,
        LegFront,
        None,
    };

    // A walking character, Cortex Command style: the torso is one hitbox
    // and each leg is a Limb with its own foot hitbox. Feet find and hold
    // real footholds in the pixel terrain. Carries a weapon on an IK aim
    // arm, has health, a jetpack, and gibs on death.
    class Actor
    {
    public:
        Actor();

        Actor(const Actor&) = delete;
        Actor& operator=(const Actor&) = delete;

        bool LoadBodySprite(SDL_Renderer* renderer, const std::string& filename);

        // Tints the body sprite and limbs (enemy coloring).
        void SetTint(Uint8 r, Uint8 g, Uint8 b);

        // Places the actor standing on the terrain surface at the given x.
        void Spawn(const Terrain& terrain, float centerX);

        // moveInput: -1..1 walk input. particles may be null (no effects).
        // crouch folds the legs so the actor fits through low spaces.
        void Update(
            const Terrain& terrain,
            ParticleSystem* particles,
            float deltaTime,
            float moveInput,
            bool jump,
            bool jetpack,
            bool crouch = false);

        bool IsCrouching() const;

        void Draw(Window& window);

        // --- Aiming and weapon ---

        // Aims the arm (and facing) at a world position.
        void SetAim(float targetX, float targetY);
        void ClearAim();

        void SetWeaponDef(const WeaponDef* def);
        Weapon& GetWeapon();

        float GetAimDirX() const;
        float GetAimDirY() const;
        float GetHandX() const;
        float GetHandY() const;
        float GetMuzzleX() const;
        float GetMuzzleY() const;

        // Flashes the muzzle briefly (call after a successful shot).
        void NotifyFired();

        // Drives the digger's plasma-helix beam visual: call every
        // rendered frame while the tool is actively cutting, with the
        // current impact point (a raycast preview, independent of the
        // tool's own fire-rate ticks so the beam stays smooth). Pass
        // active=false (or just stop calling with true) when not digging.
        void SetDigBeam(bool active, float targetX, float targetY);

        // --- Vitals ---

        bool IsAlive() const;
        int GetHealth() const;
        void TakeDamage(int damage, float impulseX, float impulseY);
        void Heal(int amount);

        // Spews gibs and blood at the body's position (call on death).
        void Gib(ParticleSystem& particles);

        // Restores health/fuel (respawn).
        void ResetVitals();

        void SetTeam(int team);
        int GetTeam() const;

        float GetFuel() const;
        bool IsJetting() const;

        // --- Limbs ---

        // Tests whether the world point (x, y) lands inside one of this
        // actor's limb boxes. On a hit, reports which part and the hit
        // position mapped to that part's local 0..1 box, for addressing
        // its per-pixel BodyPart grid.
        bool TestLimbHit(
            float x,
            float y,
            BodyPartId& outPart,
            float& outLocal01X,
            float& outLocal01Y) const;

        // Applies pixel-local damage to one limb (from TestLimbHit) on
        // top of the usual aggregate TakeDamage. Destroying a limb has
        // real consequences: head/torso destruction kills outright, the
        // arm can no longer fire, a destroyed leg can never plant again
        // (losing both legs is fatal). particles may be null.
        void TakeLimbDamage(
            BodyPartId part,
            int damage,
            float local01X,
            float local01Y,
            ParticleSystem* particles,
            float impulseX,
            float impulseY);

        bool IsPartDestroyed(BodyPartId part) const;
        bool IsArmDestroyed() const;

        // 1.0 = untouched, 0.0 = destroyed (or an invalid/None part).
        float GetPartHealth(BodyPartId part) const;

        // --- Economy ---

        // Currency mined from valuable ore (gold veins), for later
        // spending on equipment/deliveries. Persists across respawns.
        void AddGold(int amount);
        int GetGold() const;

        // --- Body ---

        float GetX() const;
        float GetY() const;
        float GetWidth() const;
        float GetHeight() const;

        float GetCenterX() const;
        float GetCenterY() const;

        float GetVelocityX() const;
        float GetVelocityY() const;

        bool IsGrounded() const;

        const Limb& GetLeg(int index) const;

    private:
        bool IsBodyBoxFree(const Terrain& terrain, float x, float y) const;
        void MoveHorizontal(const Terrain& terrain, float delta);
        void MoveVerticalToward(const Terrain& terrain, float targetY, float deltaTime);
        void MoveAirborne(const Terrain& terrain, float deltaTime);

        bool FindFoothold(
            const Terrain& terrain,
            float probeX,
            float& footholdY) const;

        bool FindReachableFoothold(
            const Terrain& terrain,
            int leg,
            float probeX,
            float& footholdY) const;

        void UpdateWalkGait(const Terrain& terrain, float deltaTime);
        void UpdateIdleFeet(const Terrain& terrain, float deltaTime);
        void UpdateDanglingFeet(float deltaTime);
        void TryLand(const Terrain& terrain);

        void DrawArmAndWeapon(Window& window) const;
        void DrawDigBeam(Window& window) const;

        float HipWorldX(int leg) const;
        float HipWorldY(int leg) const;
        float ShoulderX() const;
        float ShoulderY() const;

        // Current world-space AABB a limb occupies, used both for hit
        // testing and for rendering damage holes over it.
        void GetPartBox(
            BodyPartId part,
            float& outMinX,
            float& outMinY,
            float& outMaxX,
            float& outMaxY) const;

        BodyPart* GetPartGrid(BodyPartId part);
        const BodyPart* GetPartGrid(BodyPartId part) const;

        void DestroyPart(BodyPartId part, ParticleSystem* particles);

        Texture2D m_BodyTexture;

        float m_X;
        float m_Y;

        float m_VelocityX;
        float m_VelocityY;
        float m_KnockVelX; // decaying knockback, added on top of walk speed

        Limb m_Legs[2];
        int m_SwingLeg;

        float m_FacingDir;
        float m_MoveDir;
        bool m_Grounded;

        // Aim/weapon
        bool m_HasAim;
        float m_AimDirX;
        float m_AimDirY;
        Weapon m_Weapon;
        float m_MuzzleFlash;

        // Vitals
        int m_Health;
        int m_Team;
        float m_Fuel;
        bool m_Jetting;
        int m_Gold;

        Uint8 m_TintR;
        Uint8 m_TintG;
        Uint8 m_TintB;

        // Visual-only state.
        float m_BobPhase;
        float m_HurtFlash;

        bool m_Crouching;

        // Digger plasma beam.
        bool m_DigBeamActive;
        float m_DigBeamTargetX;
        float m_DigBeamTargetY;
        float m_BeamPhase;

        // Per-limb pixel destruction. Indexed by BodyPartId (excluding
        // None): Head, Torso, Arm, LegBack, LegFront.
        BodyPart m_HeadPart;
        BodyPart m_TorsoPart;
        BodyPart m_ArmPart;
        BodyPart m_LegParts[2];
        bool m_PartDetached[5];

        float CurrentStandHeight() const;
    };
}
