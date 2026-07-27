#pragma once

#include "Limb.h"

#include "../Graphics/Texture2D.h"

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class Terrain;
    class Window;

    // A walking character, Cortex Command style: the torso is one hitbox
    // and each leg is a Limb with its own foot hitbox. Feet find and hold
    // real footholds in the pixel terrain; the body's height comes from
    // where the planted feet actually are, so the actor climbs rubble,
    // straddles craters, and loses footing when the ground under a foot is
    // dug away.
    class Actor
    {
    public:
        Actor();

        Actor(const Actor&) = delete;
        Actor& operator=(const Actor&) = delete;

        bool LoadBodySprite(SDL_Renderer* renderer, const std::string& filename);

        // Places the actor standing on the terrain surface at the given x.
        void Spawn(const Terrain& terrain, float centerX);

        // moveInput: -1..1 walk input. jump: jump key state.
        void Update(
            const Terrain& terrain,
            float deltaTime,
            float moveInput,
            bool jump);

        void Draw(Window& window);

        // Body (torso) box, top-left and size.
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

        // Finds a standable surface near probeX within the leg's vertical
        // reach window. Returns true and fills footholdY on success.
        bool FindFoothold(
            const Terrain& terrain,
            float probeX,
            float& footholdY) const;

        // Finds a foothold at probeX that the given leg can actually reach
        // from its hip. A leg is never planted beyond its reach.
        bool FindReachableFoothold(
            const Terrain& terrain,
            int leg,
            float probeX,
            float& footholdY) const;

        void UpdateWalkGait(const Terrain& terrain, float deltaTime);
        void UpdateIdleFeet(const Terrain& terrain, float deltaTime);
        void UpdateDanglingFeet(float deltaTime);
        void TryLand(const Terrain& terrain);

        float HipWorldX(int leg) const;
        float HipWorldY(int leg) const;

        Texture2D m_BodyTexture;

        float m_X;
        float m_Y;

        float m_VelocityX;
        float m_VelocityY;

        Limb m_Legs[2];
        int m_SwingLeg;

        float m_FacingDir;
        bool m_Grounded;
    };
}
