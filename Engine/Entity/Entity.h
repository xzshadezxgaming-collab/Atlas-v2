#pragma once

namespace Atlas
{
    class Entity
    {
    public:
        Entity();

        void SetPosition(float x, float y);

        float GetX() const;
        float GetY() const;

        void Move(float deltaX, float deltaY);

    private:
        float m_X;
        float m_Y;
    };
}