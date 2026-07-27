#pragma once

#include "../ECS/Transform.h"

namespace Atlas
{
    class RigidBody
    {
    public:
        RigidBody();

        void SetTransform(Transform* transform);

        void SetVelocity(float x, float y);

        void AddForce(float x, float y);

        void Update(float deltaTime);

        float GetVelocityX() const;
        float GetVelocityY() const;

        bool IsGrounded() const;
        void SetGrounded(bool grounded);

        void StopHorizontalMovement();
        void StopVerticalMovement();

    private:
        Transform* m_Transform;

        float m_VelocityX;
        float m_VelocityY;

        bool m_Grounded;
    };
}