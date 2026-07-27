#include "Entity.h"

namespace Atlas
{
    Entity::Entity()
        : m_X(0.0f),
        m_Y(0.0f)
    {
    }

    void Entity::SetPosition(float x, float y)
    {
        m_X = x;
        m_Y = y;
    }

    float Entity::GetX() const
    {
        return m_X;
    }

    float Entity::GetY() const
    {
        return m_Y;
    }

    void Entity::Move(float deltaX, float deltaY)
    {
        m_X += deltaX;
        m_Y += deltaY;
    }
}