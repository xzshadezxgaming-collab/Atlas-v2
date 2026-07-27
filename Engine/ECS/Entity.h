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

        // The sprite and rigid body hold pointers into this entity's own
        // transform, so copying/moving would leave them dangling.
        Entity(const Entity&) = delete;
        Entity& operator=(const Entity&) = delete;

        Transform& GetTransform();
        Sprite& GetSprite();
        RigidBody& GetRigidBody();

    private:
        Transform m_Transform;
        Sprite m_Sprite;
        RigidBody m_RigidBody;
    };
}