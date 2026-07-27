#pragma once

#include "Transform.h"

#include "../Graphics/Sprite.h"
#include "../Physics/RigidBody.h"

namespace Atlas
{
    class Entity
    {
    public:
        Entity();

        Transform& GetTransform();
        Sprite& GetSprite();
        RigidBody& GetRigidBody();

    private:
        Transform m_Transform;
        Sprite m_Sprite;
        RigidBody m_RigidBody;
    };
}