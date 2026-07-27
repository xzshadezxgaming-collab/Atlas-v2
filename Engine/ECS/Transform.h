#pragma once

namespace Atlas
{
    class Transform
    {
    public:
        Transform();

        void SetPosition(float x, float y);

        void Translate(float x, float y);

        float GetX() const;
        float GetY() const;

    private:
        float m_X;
        float m_Y;
    };
}