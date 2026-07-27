#include "RigidBody.h"

namespace Atlas
{
    namespace
    {
        constexpr float Gravity = 900.0f;
    }

    RigidBody::RigidBody()
        : m_Transform(nullptr),
        m_VelocityX(0.0f),
        m_VelocityY(0.0f),
        m_Grounded(false)
    {
    }

    void RigidBody::SetTransform(Transform* transform)
    {
        m_Transform = transform;
    }

    void RigidBody::SetVelocity(float x, float y)
    {
        m_VelocityX = x;
        m_VelocityY = y;
    }

    void RigidBody::AddForce(float x, float y)
    {
        m_VelocityX += x;
        m_VelocityY += y;
    }

    void RigidBody::Update(float deltaTime)
    {
        if (!m_Transform)
            return;

        if (!m_Grounded)
        {
            m_VelocityY += Gravity * deltaTime;
        }

        m_Transform->Translate(
            m_VelocityX * deltaTime,
            m_VelocityY * deltaTime);

        m_Grounded = false;
    }

    float RigidBody::GetVelocityX() const
    {
        return m_VelocityX;
    }

    float RigidBody::GetVelocityY() const
    {
        return m_VelocityY;
    }

    bool RigidBody::IsGrounded() const
    {
        return m_Grounded;
    }

    void RigidBody::SetGrounded(bool grounded)
    {
        m_Grounded = grounded;
    }

    void RigidBody::StopHorizontalMovement()
    {
        m_VelocityX = 0.0f;
    }

    void RigidBody::StopVerticalMovement()
    {
        m_VelocityY = 0.0f;
    }
}