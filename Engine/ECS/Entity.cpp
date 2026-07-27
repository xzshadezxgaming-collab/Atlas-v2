#include "Entity.h"

namespace Atlas
{
    Entity::Entity()
    {
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