#pragma once

#include "../ECS/Transform.h"

namespace Atlas
{
    class Terrain;

    // Axis-aligned body that moves through the pixel terrain. Movement is
    // integrated in steps of at most one pixel so fast bodies can't tunnel,
    // and small ledges (up to StepHeight pixels) are climbed automatically,
    // which is what lets a body walk up the slopes and craters of a
    // per-pixel world.
    class RigidBody
    {
    public:
        RigidBody();

        void SetTransform(Transform* transform);

        // Size of the collision box, in pixels.
        void SetSize(float width, float height);

        float GetWidth() const;
        float GetHeight() const;

        void SetVelocity(float x, float y);

        void AddForce(float x, float y);

        void Update(const Terrain& terrain, float deltaTime);

        float GetVelocityX() const;
        float GetVelocityY() const;

        bool IsGrounded() const;

        void StopHorizontalMovement();
        void StopVerticalMovement();

    private:
        bool IsBoxFree(const Terrain& terrain, float x, float y) const;

        Transform* m_Transform;

        float m_Width;
        float m_Height;

        float m_VelocityX;
        float m_VelocityY;

        bool m_Grounded;
    };
}
