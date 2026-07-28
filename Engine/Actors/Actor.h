#pragma once

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

        float HipWorldX(int leg) const;
        float HipWorldY(int leg) const;
        float ShoulderX() const;
        float ShoulderY() const;

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

        Uint8 m_TintR;
        Uint8 m_TintG;
        Uint8 m_TintB;

        // Visual-only state.
        float m_BobPhase;
        float m_HurtFlash;

        bool m_Crouching;

        float CurrentStandHeight() const;
    };
}
