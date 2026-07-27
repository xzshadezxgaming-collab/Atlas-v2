#include "RigidBody.h"

#include "../World/Terrain.h"

#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float Gravity = 1000.0f;

        // Highest ledge, in pixels, a body climbs automatically while
        // moving horizontally.
        constexpr int StepHeight = 10;

        // Spacing between collision sample points along the box edges.
        constexpr float SampleSpacing = 2.0f;
    }

    RigidBody::RigidBody()
        : m_Transform(nullptr),
        m_Width(16.0f),
        m_Height(16.0f),
        m_VelocityX(0.0f),
        m_VelocityY(0.0f),
        m_Grounded(false)
    {
    }

    void RigidBody::SetTransform(Transform* transform)
    {
        m_Transform = transform;
    }

    void RigidBody::SetSize(float width, float height)
    {
        m_Width = width;
        m_Height = height;
    }

    float RigidBody::GetWidth() const
    {
        return m_Width;
    }

    float RigidBody::GetHeight() const
    {
        return m_Height;
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

    void RigidBody::Update(const Terrain& terrain, float deltaTime)
    {
        if (!m_Transform)
            return;

        const bool wasGrounded = m_Grounded;

        m_VelocityY += Gravity * deltaTime;

        float x = m_Transform->GetX();
        float y = m_Transform->GetY();

        // Horizontal movement, at most one pixel at a time. On a blocked
        // step, try shifting up over a small ledge before giving up.
        const float deltaX = m_VelocityX * deltaTime;
        const int stepsX = static_cast<int>(std::ceil(std::fabs(deltaX)));

        if (stepsX > 0)
        {
            const float stepX = deltaX / static_cast<float>(stepsX);

            for (int i = 0; i < stepsX; i++)
            {
                if (IsBoxFree(terrain, x + stepX, y))
                {
                    x += stepX;
                    continue;
                }

                bool stepped = false;

                for (int lift = 1; lift <= StepHeight; lift++)
                {
                    if (IsBoxFree(
                        terrain,
                        x + stepX,
                        y - static_cast<float>(lift)))
                    {
                        x += stepX;
                        y -= static_cast<float>(lift);
                        stepped = true;
                        break;
                    }
                }

                if (!stepped)
                {
                    m_VelocityX = 0.0f;
                    break;
                }
            }
        }

        // Ground snapping: a body that was walking on the ground sticks to
        // downhill slopes instead of going airborne each step. Only snaps
        // if ground exists within StepHeight below; a real ledge is still a
        // fall.
        if (wasGrounded &&
            m_VelocityY >= 0.0f &&
            IsBoxFree(terrain, x, y + 1.0f))
        {
            int drop = 0;

            while (drop < StepHeight &&
                IsBoxFree(terrain, x, y + static_cast<float>(drop) + 1.0f))
            {
                drop++;
            }

            if (drop < StepHeight)
            {
                y += static_cast<float>(drop);
            }
        }

        // Vertical movement.
        const float deltaY = m_VelocityY * deltaTime;
        const int stepsY = static_cast<int>(std::ceil(std::fabs(deltaY)));

        if (stepsY > 0)
        {
            const float stepY = deltaY / static_cast<float>(stepsY);

            for (int i = 0; i < stepsY; i++)
            {
                if (IsBoxFree(terrain, x, y + stepY))
                {
                    y += stepY;
                    continue;
                }

                m_VelocityY = 0.0f;
                break;
            }
        }

        m_Grounded = !IsBoxFree(terrain, x, y + 1.0f);

        m_Transform->SetPosition(x, y);
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

    void RigidBody::StopHorizontalMovement()
    {
        m_VelocityX = 0.0f;
    }

    void RigidBody::StopVerticalMovement()
    {
        m_VelocityY = 0.0f;
    }

    bool RigidBody::IsBoxFree(const Terrain& terrain, float x, float y) const
    {
        const float left = x + 0.5f;
        const float right = x + m_Width - 0.5f;
        const float top = y + 0.5f;
        const float bottom = y + m_Height - 0.5f;

        // Top and bottom edges.
        for (float sampleX = left; ; sampleX += SampleSpacing)
        {
            const float clampedX = std::fmin(sampleX, right);

            if (terrain.IsSolid(clampedX, top) ||
                terrain.IsSolid(clampedX, bottom))
            {
                return false;
            }

            if (clampedX >= right)
                break;
        }

        // Left and right edges.
        for (float sampleY = top; ; sampleY += SampleSpacing)
        {
            const float clampedY = std::fmin(sampleY, bottom);

            if (terrain.IsSolid(left, clampedY) ||
                terrain.IsSolid(right, clampedY))
            {
                return false;
            }

            if (clampedY >= bottom)
                break;
        }

        return true;
    }
}
