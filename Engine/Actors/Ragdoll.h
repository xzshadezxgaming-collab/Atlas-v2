#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

namespace Atlas
{
    class ParticleSystem;
    class Terrain;
    class Window;

    // A limp body: what is left of an actor once it dies (a corpse that
    // collapses and tumbles), or one piece shot off it (a severed head,
    // arm or leg). Joints are verlet points held together by sticks and
    // pushed out of the terrain, so pieces fall, bounce, roll down slopes
    // and come to rest the way Cortex Command's do. Purely cosmetic: it
    // never touches the simulation's gameplay state.
    class Ragdoll
    {
    public:
        enum Joint : std::uint8_t
        {
            Hip,
            Neck,
            Crown,   // top of the head
            KneeB,
            FootB,
            KneeF,
            FootF,
            ElbowB,
            HandB,
            ElbowF,
            HandF,

            JointCount
        };

        // Pieces a ragdoll can be made of (bit flags).
        enum Piece : std::uint8_t
        {
            Torso = 1 << 0,
            Head = 1 << 1,
            LegB = 1 << 2,
            LegF = 1 << 3,
            ArmB = 1 << 4,
            ArmF = 1 << 5,

            AllPieces = 0x3F
        };

        Ragdoll();

        // Which pieces exist; joints not used by any piece are ignored.
        void SetPieces(std::uint8_t pieces);
        std::uint8_t GetPieces() const;

        // Places a joint with a velocity (px/s).
        void SetJoint(Joint joint, float x, float y, float velX, float velY);
        float GetJointX(Joint joint) const;
        float GetJointY(Joint joint) const;

        // Builds the sticks from the joints' current spacing. Call once,
        // after every joint is placed.
        void Bind();

        // Looks: build (0 light, 1 medium, 2 heavy), team tint, facing
        // when it died (the body art is mirrored for -1), and the body
        // texture to cut the head and torso from.
        void SetLook(int bodyClass, Uint8 tintR, Uint8 tintG, Uint8 tintB,
            float facing, SDL_Texture* bodyTexture);

        // Seconds of open wounds still pumping blood (stumps, neck).
        void SetBleeding(float seconds);

        void Update(const Terrain& terrain, ParticleSystem& particles, float deltaTime);

        // Pushes every joint within radius away from (x, y). Returns the
        // strongest velocity change dealt, in px/s.
        float Push(float x, float y, float radius, float strength);

        // A single hit (a bullet) at the joint nearest (x, y) within
        // reach: knocks it along (velX, velY). True if anything was hit.
        bool Hit(float x, float y, float reach, float velX, float velY);

        // Bounding box of the joints, for cheap rejection.
        void GetBounds(float& minX, float& minY, float& maxX, float& maxY) const;

        float GetCenterX() const;
        float GetCenterY() const;

        // Fades out once its time is up.
        bool IsExpired() const;
        void Expire();

        void Draw(Window& window) const;

    private:
        struct Stick
        {
            std::uint8_t A;
            std::uint8_t B;
            float Length;
        };

        static constexpr int MaxSticks = 16;

        bool UsesJoint(int joint) const;
        void AddStick(int a, int b);
        void SolveTerrain(const Terrain& terrain, int joint);

        float m_X[JointCount];
        float m_Y[JointCount];
        float m_PrevX[JointCount];
        float m_PrevY[JointCount];
        bool m_Resting[JointCount];

        Stick m_Sticks[MaxSticks];
        int m_StickCount;

        std::uint8_t m_Pieces;

        int m_BodyClass;
        Uint8 m_TintR;
        Uint8 m_TintG;
        Uint8 m_TintB;
        float m_Facing;
        SDL_Texture* m_BodyTexture;

        float m_Age;
        float m_Lifetime;
        float m_Bleeding;
        float m_BleedTimer;
        float m_StillTime;
    };
}
