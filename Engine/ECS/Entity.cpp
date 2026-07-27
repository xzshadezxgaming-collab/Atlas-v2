#include "Entity.h"

namespace Atlas
{
    Entity::Entity()
    {
        m_Sprite.SetTransform(&m_Transform);
        m_RigidBody.SetTransform(&m_Transform);
    }

    Transform& Entity::GetTransform()
    {
        return m_Transform;
    }

    Sprite& Entity::GetSprite()
    {
        return m_Sprite;
    }

    RigidBody& Entity::GetRigidBody()
    {
        return m_RigidBody;
    }
}
