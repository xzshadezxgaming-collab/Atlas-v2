#include "Camera.h"

namespace Atlas
{
    Camera::Camera()
        : m_X(0.0f),
        m_Y(0.0f)
    {
    }

    void Camera::SetPosition(float x, float y)
    {
        m_X = x;
        m_Y = y;
    }

    void Camera::Move(float x, float y)
    {
        m_X += x;
        m_Y += y;
    }

    float Camera::GetX() const
    {
        return m_X;
    }

    float Camera::GetY() const
    {
        return m_Y;
    }
}