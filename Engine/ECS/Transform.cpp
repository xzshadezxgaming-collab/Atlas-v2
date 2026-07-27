#include "Transform.h"

namespace Atlas
{
    Transform::Transform()
        : m_X(0.0f),
        m_Y(0.0f)
    {
    }

    void Transform::SetPosition(float x, float y)
    {
        m_X = x;
        m_Y = y;
    }

    void Transform::Translate(float x, float y)
    {
        m_X += x;
        m_Y += y;
    }

    float Transform::GetX() const
    {
        return m_X;
    }

    float Transform::GetY() const
    {
        return m_Y;
    }
}