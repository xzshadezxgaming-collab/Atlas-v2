#pragma once

namespace Atlas
{
    class Camera
    {
    public:
        Camera();

        void SetPosition(float x, float y);
        void Move(float x, float y);

        float GetX() const;
        float GetY() const;

    private:
        float m_X;
        float m_Y;
    };
}