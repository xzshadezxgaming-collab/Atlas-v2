#pragma once

namespace Atlas
{
    class Terrain;
    class Window;

    // One leg of an actor, Cortex Command style: the foot is its own small
    // hitbox that contacts the terrain independently of the body. A limb is
    // either planted (foot fixed to a terrain foothold, carrying the body)
    // or free (swinging toward the next foothold, or dangling in the air).
    class Limb
    {
    public:
        Limb();

        // hipOffset: position of the hip joint relative to the body's
        // top-left corner. thigh/shin: segment lengths in pixels.
        void Configure(
            float hipOffsetX,
            float hipOffsetY,
            float thighLength,
            float shinLength);

        float GetHipOffsetX() const;
        float GetHipOffsetY() const;

        // Maximum hip-to-foot distance.
        float GetReach() const;

        bool IsPlanted() const;
        void Plant(float footX, float footY);
        void Unplant();

        float GetFootX() const;
        float GetFootY() const;
        void SetFootPosition(float x, float y);

        // Whether the terrain under the planted foot's hitbox still exists
        // (digging can remove it, at which point the limb loses support).
        bool IsSupported(const Terrain& terrain) const;

        // Swings the free foot toward a foothold, lifting it in an arc.
        // Returns true when the foot has arrived (caller decides to plant).
        bool SwingToward(
            float targetX,
            float targetY,
            float speed,
            float lift,
            float deltaTime);

        // Relaxed drift toward a rest pose while airborne.
        void DangleToward(float restX, float restY, float deltaTime);

        // Draws the limb as a thigh and shin with an IK-bent knee, plus the
        // boot. facingDir is +1 (right) or -1 (left); nearSide selects the
        // brighter palette for the leg closest to the viewer. The tint
        // multiplies the palette (255 = unchanged).
        void Draw(
            Window& window,
            float hipX,
            float hipY,
            float facingDir,
            bool nearSide,
            unsigned char tintR = 255,
            unsigned char tintG = 255,
            unsigned char tintB = 255) const;

    private:
        float m_HipOffsetX;
        float m_HipOffsetY;
        float m_Thigh;
        float m_Shin;

        float m_FootX;
        float m_FootY;

        bool m_Planted;
    };
}
