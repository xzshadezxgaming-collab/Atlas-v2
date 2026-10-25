#pragma once

#include <cstdint>

namespace Atlas
{
    class Terrain;
    class Window;

    // How a leg is dressed. Each body build has its own (fatigues, the
    // scout's light trousers, the heavy's armour plates).
    struct LegStyle
    {
        std::uint8_t PantsR, PantsG, PantsB;
        std::uint8_t BootR, BootG, BootB;

        // Knee pad / plate colour; KneeSize 0 draws a bare knee.
        std::uint8_t PadR, PadG, PadB;
        float KneeSize;

        float ThighWidth; // at the hip; tapers toward the knee
        float ShinWidth;  // at the knee; tapers toward the ankle
        float BootScale;

        // Armoured shins get a plate down the front.
        bool ShinPlate;
    };

    // One leg of an actor, Cortex Command style: the foot is its own small
    // hitbox that contacts the terrain independently of the body. A limb is
    // either planted (foot fixed to a terrain foothold, carrying the body)
    // or free (swinging toward the next foothold, or dangling in the air).
    class Limb
    {
    public:
        // Height of the ankle joint above the sole. The IK chain ends at
        // the ankle; the boot fills the gap down to the ground.
        static constexpr float AnkleHeight = 3.0f;

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

        // Maximum hip-to-sole distance.
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

        // Moves the free foot along a stepping arc: from where it was when
        // the swing began to (targetX, targetY), taking `duration` seconds
        // and lifting up to `lift` pixels at mid-swing (more when stepping
        // up, so the toe clears the ledge). The target may move between
        // calls (the hip moves on, terrain changes) and the arc follows.
        // Returns true when the foot has arrived (caller decides to plant).
        bool AdvanceSwing(
            float targetX,
            float targetY,
            float duration,
            float lift,
            float deltaTime);

        // 0 at lift-off .. 1 at touch-down; 0 while planted or dangling.
        float GetSwingProgress() const;

        // Relaxed drift toward a pose while airborne (or reaching for
        // missing ground). Breaks off any swing in progress.
        void DangleToward(float restX, float restY, float rate, float deltaTime);

        // Eases the foot position used for rendering toward the real
        // (logical) foot position, and the boot's pitch toward what the
        // foot is doing. Call once per simulation tick. Some logical
        // repositions are instantaneous snaps (an emergency recovery
        // plant, catching a foothold on landing) that are correct for
        // support/collision but look like a pop if drawn directly; this
        // turns any such snap into a brief, smooth slide on screen
        // without touching the logical position at all.
        void EaseVisual(float deltaTime);

        // Two-bone IK for a leg: where the knee goes for a hip and an
        // ankle position. bend is the knee's facing (+1 = knee points
        // right, -1 = left); values in between swivel it through the
        // view plane (used while turning around). The ankle is pulled in
        // to full stretch if out of reach.
        static void SolveKnee(
            float hipX,
            float hipY,
            float ankleX,
            float ankleY,
            float thigh,
            float shin,
            float bend,
            float& outKneeX,
            float& outKneeY,
            float& outAnkleX,
            float& outAnkleY);

        // Draws the limb: a tapered thigh and shin meeting at an IK knee
        // that always points the way the body faces, and a boot that
        // rolls through the step. bend: see SolveKnee. nearSide selects
        // the brighter palette for the leg closest to the viewer. The
        // tint multiplies the palette (255 = unchanged).
        void Draw(
            Window& window,
            float hipX,
            float hipY,
            float bend,
            bool nearSide,
            const LegStyle& style,
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

        // Stepping arc (see AdvanceSwing).
        bool m_Swinging;
        float m_SwingFromX;
        float m_SwingFromY;
        float m_SwingT;
        float m_SwingDirX;

        // Rendered-only foot position and boot pitch (see EaseVisual):
        // the logical foot plus an offset left over from a snap, which
        // decays away.
        float m_VisualFootX;
        float m_VisualFootY;
        float m_SnapOffsetX;
        float m_SnapOffsetY;
        float m_LastFootX;
        float m_LastFootY;
        float m_VisualPitch;
        bool m_VisualInitialized;
    };
}
