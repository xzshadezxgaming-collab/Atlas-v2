#include "Actor.h"

#include "../Core/Window.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Atlas
{
    namespace
    {
        // Torso hitbox (the legs hang below it).
        constexpr float BodyWidth = 24.0f;
        constexpr float BodyHeight = 40.0f;

        // The body sprite is a little wider than the hitbox.
        constexpr float SpriteWidth = 32.0f;
        constexpr float SpriteHeight = 40.0f;

        // Hip joints, relative to the body's top-left corner.
        constexpr float HipY = 38.0f;
        constexpr float HipXBack = 8.0f;
        constexpr float HipXFront = 16.0f;

        constexpr float ThighLength = 17.0f;
        constexpr float ShinLength = 16.0f;
        constexpr float LegReach = ThighLength + ShinLength;

        // Body-bottom height above the supporting feet when standing.
        constexpr float StandHeight = 24.0f;

        // How far a foot may reach for the next foothold.
        constexpr float MaxStepUp = 18.0f;
        constexpr float MaxStepDown = 36.0f;
        constexpr float StepAhead = 16.0f;
        constexpr float FootLift = 10.0f;

        // A foot may only be planted within this hip distance...
        constexpr float PlantReach = LegReach + 3.0f;

        // ...and loses grip beyond this one. The gap is hysteresis: a
        // fresh plant at full stretch doesn't immediately let go again.
        constexpr float MaxFootDistance = LegReach + 8.0f;

        // Stance stretch at which the swinging leg plants on the nearest
        // foothold instead of finishing its full step.
        constexpr float StanceStrain = LegReach + 1.0f;

        constexpr float WalkSpeed = 170.0f;
        constexpr float Gravity = 1000.0f;
        constexpr float JumpVelocity = -460.0f;

        // Vertical body speed limit while the legs push it up or let it
        // down (climbing is slower than free fall).
        constexpr float ClimbRate = 260.0f;

        // Clearance the torso may ride up while moving horizontally into
        // rising ground (the legs handle real steps).
        constexpr int BodyClearance = 12;

        constexpr float SampleSpacing = 2.0f;

        // Arm/weapon (the sprite's shoulders sit at y=23 of the 40px art).
        constexpr float ShoulderOffsetY = 23.0f;
        constexpr float HandDistance = 13.0f;
        constexpr float ArmThickness = 4.0f;

        // Jetpack
        constexpr float JetAcceleration = 1750.0f;
        constexpr float JetMaxRise = -330.0f;
        constexpr float JetBurnPerSecond = 1.0f / 3.2f;
        constexpr float JetRegenPerSecond = 1.0f / 2.4f;

        // Landing faster than this hurts.
        constexpr float FallDamageSpeed = 640.0f;

        constexpr float KnockbackDamping = 5.0f;

        void DrawArmSegmentPass(
            Window& window,
            float x0, float y0,
            float x1, float y1,
            float thickness,
            Uint8 r, Uint8 g, Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(length / (thickness * 0.5f))));

            for (int i = 0; i <= steps; i++)
            {
                const float t =
                    static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    x0 + dx * t - thickness * 0.5f,
                    y0 + dy * t - thickness * 0.5f,
                    thickness,
                    thickness,
                    r, g, b, 255);
            }
        }

        void DrawArmSegment(
            Window& window,
            float x0, float y0,
            float x1, float y1,
            Uint8 r, Uint8 g, Uint8 b)
        {
            // Outline, fill, highlight.
            DrawArmSegmentPass(
                window, x0, y0, x1, y1, ArmThickness + 2.0f, 24, 22, 27);
            DrawArmSegmentPass(
                window, x0, y0, x1, y1, ArmThickness, r, g, b);
            DrawArmSegmentPass(
                window, x0 - 1.0f, y0 - 1.0f, x1 - 1.0f, y1 - 1.0f,
                std::max(1.0f, ArmThickness - 3.0f),
                static_cast<Uint8>(std::min(255, r + 24)),
                static_cast<Uint8>(std::min(255, g + 24)),
                static_cast<Uint8>(std::min(255, b + 24)));
        }

        // Axis-aligned outlined box, for weapon parts.
        void DrawPart(
            Window& window,
            float x, float y, float w, float h,
            Uint8 r, Uint8 g, Uint8 b)
        {
            window.DrawFilledRect(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f,
                24, 22, 27, 255);
            window.DrawFilledRect(x, y, w, h, r, g, b, 255);
        }
    }

    Actor::Actor()
        : m_X(0.0f),
        m_Y(0.0f),
        m_VelocityX(0.0f),
        m_VelocityY(0.0f),
        m_KnockVelX(0.0f),
        m_SwingLeg(0),
        m_FacingDir(1.0f),
        m_MoveDir(1.0f),
        m_Grounded(false),
        m_HasAim(false),
        m_AimDirX(1.0f),
        m_AimDirY(0.0f),
        m_MuzzleFlash(0.0f),
        m_Health(100),
        m_Team(0),
        m_Fuel(1.0f),
        m_Jetting(false),
        m_TintR(255),
        m_TintG(255),
        m_TintB(255),
        m_BobPhase(0.0f),
        m_HurtFlash(0.0f),
        m_Crouching(false)
    {
        m_Legs[0].Configure(HipXBack, HipY, ThighLength, ShinLength);
        m_Legs[1].Configure(HipXFront, HipY, ThighLength, ShinLength);
    }

    bool Actor::LoadBodySprite(
        SDL_Renderer* renderer,
        const std::string& filename)
    {
        if (!m_BodyTexture.Load(renderer, filename))
            return false;

        SDL_SetTextureColorMod(
            m_BodyTexture.GetTexture(), m_TintR, m_TintG, m_TintB);

        return true;
    }

    void Actor::SetTint(Uint8 r, Uint8 g, Uint8 b)
    {
        m_TintR = r;
        m_TintG = g;
        m_TintB = b;

        if (m_BodyTexture.GetTexture())
            SDL_SetTextureColorMod(m_BodyTexture.GetTexture(), r, g, b);
    }

    void Actor::Spawn(const Terrain& terrain, float centerX)
    {
        float surfaceY = 0.0f;

        for (int y = 0; y < terrain.GetHeight(); y++)
        {
            if (terrain.IsSolid(centerX, static_cast<float>(y)))
            {
                surfaceY = static_cast<float>(y);
                break;
            }
        }

        m_X = centerX - BodyWidth * 0.5f;
        m_Y = surfaceY - StandHeight - BodyHeight;
        m_VelocityX = 0.0f;
        m_VelocityY = 0.0f;
        m_KnockVelX = 0.0f;

        for (int i = 0; i < 2; i++)
        {
            float footholdY = surfaceY;
            FindFoothold(terrain, HipWorldX(i), footholdY);
            m_Legs[i].Plant(HipWorldX(i), footholdY);
        }

        m_Grounded = true;
    }

    void Actor::Update(
        const Terrain& terrain,
        ParticleSystem* particles,
        float deltaTime,
        float moveInput,
        bool jump,
        bool jetpack,
        bool crouch)
    {
        if (!IsAlive())
            return;

        m_Crouching = crouch && !jetpack;

        // Knockback decays; walking is layered on top of it.
        m_KnockVelX -= m_KnockVelX * std::min(1.0f, KnockbackDamping * deltaTime);

        const float speedScale = m_Crouching ? 0.5f : 1.0f;

        m_VelocityX = moveInput * WalkSpeed * speedScale + m_KnockVelX;

        if (moveInput < 0.0f)
            m_MoveDir = -1.0f;
        else if (moveInput > 0.0f)
            m_MoveDir = 1.0f;

        // Facing follows the aim when aiming, else the movement.
        if (m_HasAim)
            m_FacingDir = m_AimDirX < 0.0f ? -1.0f : 1.0f;
        else if (moveInput != 0.0f)
            m_FacingDir = m_MoveDir;

        if (m_MuzzleFlash > 0.0f)
            m_MuzzleFlash -= deltaTime;

        if (m_HurtFlash > 0.0f)
            m_HurtFlash -= deltaTime;

        // Walk-cycle bob for the body sprite.
        if (m_Grounded)
            m_BobPhase += std::fabs(m_VelocityX) * deltaTime * 0.09f;

        m_VelocityY += Gravity * deltaTime;

        // Jetpack.
        m_Jetting = false;

        if (jetpack && m_Fuel > 0.0f)
        {
            m_Jetting = true;
            m_Fuel = std::max(0.0f, m_Fuel - JetBurnPerSecond * deltaTime);

            m_VelocityY = std::max(
                JetMaxRise,
                m_VelocityY - JetAcceleration * deltaTime);

            m_Legs[0].Unplant();
            m_Legs[1].Unplant();

            // Exhaust.
            if (particles)
            {
                particles->SpawnSmoke(
                    GetCenterX() - 4.0f + static_cast<float>(rand() % 8),
                    m_Y + BodyHeight,
                    -m_VelocityX * 0.1f,
                    140.0f);

                particles->SpawnSpark(
                    GetCenterX(),
                    m_Y + BodyHeight + 2.0f,
                    m_VelocityX * -0.2f + static_cast<float>(rand() % 60 - 30),
                    170.0f);
            }
        }

        // A planted foot only counts while the terrain under it still
        // exists (digging can remove it) and it is still within leg reach.
        for (int i = 0; i < 2; i++)
        {
            if (!m_Legs[i].IsPlanted())
                continue;

            if (!m_Legs[i].IsSupported(terrain))
            {
                m_Legs[i].Unplant();
                continue;
            }

            const float dx = m_Legs[i].GetFootX() - HipWorldX(i);
            const float dy = m_Legs[i].GetFootY() - HipWorldY(i);

            if (std::sqrt(dx * dx + dy * dy) > MaxFootDistance)
                m_Legs[i].Unplant();
        }

        m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();

        if (m_Grounded && jump)
        {
            m_VelocityY = JumpVelocity;
            m_Legs[0].Unplant();
            m_Legs[1].Unplant();
            m_Grounded = false;
        }

        if (m_Grounded)
        {
            m_VelocityY = 0.0f;
            m_Fuel = std::min(1.0f, m_Fuel + JetRegenPerSecond * deltaTime);

            MoveHorizontal(terrain, m_VelocityX * deltaTime);

            if (std::fabs(m_VelocityX) > 1.0f)
                UpdateWalkGait(terrain, deltaTime);
            else
                UpdateIdleFeet(terrain, deltaTime);

            // The body rides at stand height above the supporting feet.
            float supportSum = 0.0f;
            int supportCount = 0;

            for (int i = 0; i < 2; i++)
            {
                if (m_Legs[i].IsPlanted())
                {
                    supportSum += m_Legs[i].GetFootY();
                    supportCount++;
                }
            }

            if (supportCount > 0)
            {
                const float supportY = supportSum / supportCount;

                float targetY =
                    supportY - CurrentStandHeight() - BodyHeight;

                // If the ceiling is too low for the preferred ride
                // height, compress the legs further (down to almost
                // sitting on the feet) so the actor can wriggle through
                // tight passages instead of jamming its head.
                const float lowestY = supportY - 2.0f - BodyHeight;

                while (targetY < lowestY &&
                    !IsBodyBoxFree(terrain, m_X, targetY))
                {
                    targetY += 1.0f;
                }

                MoveVerticalToward(terrain, targetY, deltaTime);
            }

            m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();
        }

        if (!m_Grounded)
        {
            const float fallSpeed = m_VelocityY;

            MoveAirborne(terrain, deltaTime);
            UpdateDanglingFeet(deltaTime);

            if (m_VelocityY >= 0.0f && !m_Jetting)
                TryLand(terrain);

            m_Grounded = m_Legs[0].IsPlanted() || m_Legs[1].IsPlanted();

            if (m_Grounded)
            {
                m_VelocityY = 0.0f;

                // Fall damage on hard landings.
                if (fallSpeed > FallDamageSpeed)
                {
                    const int damage = static_cast<int>(
                        (fallSpeed - FallDamageSpeed) * 0.1f);

                    TakeDamage(damage, 0.0f, 0.0f);

                    if (particles && damage > 0)
                    {
                        particles->BurstBlood(
                            GetCenterX(),
                            m_Y + BodyHeight + StandHeight * 0.5f,
                            damage / 2 + 3,
                            120.0f);
                    }
                }
            }
        }
    }

    void Actor::Draw(Window& window)
    {
        // Leg 1 is always the near (brighter) leg so shading doesn't
        // flicker when the actor turns around.
        const int nearLeg = 1;
        const int farLeg = 0;

        m_Legs[farLeg].Draw(
            window,
            HipWorldX(farLeg),
            HipWorldY(farLeg),
            m_FacingDir,
            false,
            m_TintR, m_TintG, m_TintB);

        if (m_BodyTexture.GetTexture())
        {
            // Bob while walking, lean into the direction of travel, and
            // hunch forward when crouching.
            const float bob = m_Grounded
                ? std::sin(m_BobPhase) * (m_Crouching ? 0.8f : 1.4f)
                : 0.0f;

            float lean = std::clamp(
                m_VelocityX * 0.02f,
                -6.0f,
                6.0f);

            if (m_Crouching)
                lean += m_FacingDir * 11.0f;

            window.DrawTextureRotated(
                m_BodyTexture.GetTexture(),
                m_X - (SpriteWidth - BodyWidth) * 0.5f,
                m_Y + bob,
                SpriteWidth,
                SpriteHeight,
                lean,
                m_FacingDir < 0.0f);
        }

        m_Legs[nearLeg].Draw(
            window,
            HipWorldX(nearLeg),
            HipWorldY(nearLeg),
            m_FacingDir,
            true,
            m_TintR, m_TintG, m_TintB);

        DrawArmAndWeapon(window);

        // Jetpack flame under the body while thrusting.
        if (m_Jetting)
        {
            const float flameX = GetCenterX();
            const float flameY = m_Y + BodyHeight + 3.0f;

            window.DrawGlow(flameX, flameY, 13.0f, 255, 170, 70, 70);

            window.DrawFilledRect(
                flameX - 3.0f, flameY - 2.0f, 6.0f, 7.0f,
                255, 214, 130, 230);
            window.DrawFilledRect(
                flameX - 2.0f, flameY + 4.0f, 4.0f, 5.0f,
                255, 150, 60, 200);
        }

        // Hit flash: a hot red pulse around the figure.
        if (m_HurtFlash > 0.0f)
        {
            const float strength = std::min(1.0f, m_HurtFlash / 0.12f);

            window.DrawGlow(
                GetCenterX(),
                GetCenterY(),
                30.0f,
                255, 90, 60,
                static_cast<Uint8>(70.0f * strength));
        }
    }

    void Actor::DrawArmAndWeapon(Window& window) const
    {
        if (!m_Weapon.GetDef())
            return;

        const WeaponDef& def = *m_Weapon.GetDef();

        const float sx = ShoulderX();
        const float sy = ShoulderY();

        const float handX = GetHandX();
        const float handY = GetHandY();

        // Two-bone arm IK, elbow bent downward.
        float dx = handX - sx;
        float dy = handY - sy;
        float distance = std::sqrt(dx * dx + dy * dy);

        const float upper = HandDistance * 0.62f;
        const float lower = HandDistance * 0.62f;

        distance = std::min(distance, upper + lower - 0.2f);

        const float ux = dx / (distance + 0.001f);
        const float uy = dy / (distance + 0.001f);

        const float along =
            (upper * upper - lower * lower + distance * distance) /
            (2.0f * distance + 0.001f);

        const float height = std::sqrt(
            std::max(0.0f, upper * upper - along * along));

        // Elbow always hangs below the shoulder-hand line.
        const float elbowX = sx + ux * along - uy * height * m_FacingDir;
        const float elbowY = sy + uy * along + ux * height * m_FacingDir;

        const Uint8 armR = static_cast<Uint8>(92 * m_TintR / 255);
        const Uint8 armG = static_cast<Uint8>(104 * m_TintG / 255);
        const Uint8 armB = static_cast<Uint8>(72 * m_TintB / 255);

        DrawArmSegment(window, sx, sy, elbowX, elbowY, armR, armG, armB);
        DrawArmSegment(window, elbowX, elbowY, handX, handY, armR, armG, armB);

        // Shoulder pad over the joint.
        DrawPart(window, sx - 3.0f, sy - 3.0f, 6.0f, 6.0f,
            static_cast<Uint8>(108 * m_TintR / 255),
            static_cast<Uint8>(120 * m_TintG / 255),
            static_cast<Uint8>(84 * m_TintB / 255));
        window.DrawFilledRect(sx - 3.0f, sy - 3.0f, 6.0f, 2.0f,
            static_cast<Uint8>(130 * m_TintR / 255),
            static_cast<Uint8>(142 * m_TintG / 255),
            static_cast<Uint8>(102 * m_TintB / 255),
            255);

        // --- The weapon ---
        const float muzzleX = handX + m_AimDirX * def.BarrelLength;
        const float muzzleY = handY + m_AimDirY * def.BarrelLength;

        if (def.Kind == WeaponKind::Shovel)
        {
            // Shovel: wooden shaft, small grip, steel blade at the end.
            DrawArmSegmentPass(window,
                handX - m_AimDirX * 4.0f, handY - m_AimDirY * 4.0f,
                muzzleX, muzzleY, 4.0f, 24, 22, 27);
            DrawArmSegmentPass(window,
                handX - m_AimDirX * 4.0f, handY - m_AimDirY * 4.0f,
                muzzleX, muzzleY, 2.0f, 128, 96, 58);

            // Grip knob at the back of the shaft.
            DrawPart(window,
                handX - m_AimDirX * 6.0f - 2.0f,
                handY - m_AimDirY * 6.0f - 2.0f,
                4.0f, 4.0f,
                104, 76, 46);

            // Blade.
            DrawPart(window,
                muzzleX - 3.0f, muzzleY - 4.0f, 7.0f, 8.0f,
                134, 138, 150);
            window.DrawFilledRect(
                muzzleX - 3.0f, muzzleY - 4.0f, 7.0f, 2.0f,
                170, 174, 186, 255);
            window.DrawFilledRect(
                muzzleX + m_AimDirX * 3.0f - 1.0f,
                muzzleY + m_AimDirY * 3.0f - 1.0f,
                3.0f, 3.0f,
                108, 112, 124, 255);
        }
        else if (def.Kind == WeaponKind::Digger)
        {
            // Digger tool: brown housing, warning stripe, spinning bit.
            DrawPart(window, handX - 4.0f, handY - 4.0f, 9.0f, 8.0f,
                118, 90, 52);
            window.DrawFilledRect(handX - 4.0f, handY - 4.0f, 9.0f, 2.0f,
                146, 116, 70, 255);
            window.DrawFilledRect(handX - 4.0f, handY + 1.0f, 9.0f, 2.0f,
                190, 150, 40, 255);

            // Bit: tapering segments toward the muzzle.
            for (int i = 0; i < 3; i++)
            {
                const float t = static_cast<float>(i + 1) / 3.0f;
                const float size = 5.0f - static_cast<float>(i);

                window.DrawFilledRect(
                    handX + (muzzleX - handX) * t - size * 0.5f,
                    handY + (muzzleY - handY) * t - size * 0.5f,
                    size,
                    size,
                    130 - i * 20, 130 - i * 20, 140 - i * 20, 255);
            }
        }
        else
        {
            // Rifle-style gun built from parts along the aim.
            const float backX = -m_AimDirX;
            const float backY = -m_AimDirY;

            // Stock (behind the hand).
            DrawPart(window,
                handX + backX * 6.0f - 3.0f,
                handY + backY * 6.0f - 2.0f,
                6.0f, 5.0f,
                74, 58, 44);

            // Barrel with a lighter top edge.
            DrawArmSegmentPass(window,
                handX + m_AimDirX * 3.0f, handY + m_AimDirY * 3.0f,
                muzzleX, muzzleY, 5.0f, 24, 22, 27);
            DrawArmSegmentPass(window,
                handX + m_AimDirX * 3.0f, handY + m_AimDirY * 3.0f,
                muzzleX, muzzleY, 3.0f, 74, 76, 88);
            DrawArmSegmentPass(window,
                handX + m_AimDirX * 3.0f - 1.0f,
                handY + m_AimDirY * 3.0f - 1.0f,
                muzzleX - 1.0f, muzzleY - 1.0f, 1.0f, 108, 110, 124);

            // Receiver over the hand.
            DrawPart(window, handX - 4.0f, handY - 3.0f, 10.0f, 6.0f,
                58, 58, 66);
            window.DrawFilledRect(handX - 4.0f, handY - 3.0f, 10.0f, 2.0f,
                86, 86, 98, 255);

            // Magazine, hanging ahead of the grip.
            DrawPart(window,
                handX + m_AimDirX * 5.0f - 1.5f,
                handY + 3.0f,
                3.0f, 6.0f,
                48, 48, 56);

            // Muzzle tip.
            window.DrawFilledRect(
                muzzleX - 1.5f, muzzleY - 1.5f, 3.0f, 3.0f,
                36, 36, 42, 255);
        }

        // Glove over the grip.
        DrawPart(window, handX - 2.0f, handY - 2.0f, 5.0f, 5.0f,
            static_cast<Uint8>(78 * m_TintR / 255),
            static_cast<Uint8>(68 * m_TintG / 255),
            static_cast<Uint8>(56 * m_TintB / 255));

        if (m_MuzzleFlash > 0.0f)
        {
            // Light spill around the muzzle plus the flash itself.
            window.DrawGlow(muzzleX, muzzleY, 28.0f, 255, 200, 100, 60);

            window.DrawFilledRect(
                muzzleX - 3.0f,
                muzzleY - 3.0f,
                6.0f,
                6.0f,
                255, 236, 140, 255);

            window.DrawFilledRect(
                muzzleX + m_AimDirX * 4.0f - 2.0f,
                muzzleY + m_AimDirY * 4.0f - 2.0f,
                4.0f,
                4.0f,
                255, 180, 70, 255);
        }
    }

    void Actor::SetAim(float targetX, float targetY)
    {
        const float dx = targetX - ShoulderX();
        const float dy = targetY - ShoulderY();
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 0.001f)
            return;

        m_HasAim = true;
        m_AimDirX = dx / distance;
        m_AimDirY = dy / distance;
    }

    void Actor::ClearAim()
    {
        m_HasAim = false;
    }

    void Actor::SetWeaponDef(const WeaponDef* def)
    {
        m_Weapon.SetDef(def);
    }

    Weapon& Actor::GetWeapon()
    {
        return m_Weapon;
    }

    float Actor::GetAimDirX() const { return m_AimDirX; }
    float Actor::GetAimDirY() const { return m_AimDirY; }

    float Actor::GetHandX() const
    {
        return ShoulderX() + m_AimDirX * HandDistance;
    }

    float Actor::GetHandY() const
    {
        return ShoulderY() + m_AimDirY * HandDistance;
    }

    float Actor::GetMuzzleX() const
    {
        const float barrel =
            m_Weapon.GetDef() ? m_Weapon.GetDef()->BarrelLength : 12.0f;

        return GetHandX() + m_AimDirX * (barrel + 2.0f);
    }

    float Actor::GetMuzzleY() const
    {
        const float barrel =
            m_Weapon.GetDef() ? m_Weapon.GetDef()->BarrelLength : 12.0f;

        return GetHandY() + m_AimDirY * (barrel + 2.0f);
    }

    void Actor::NotifyFired()
    {
        const WeaponDef* def = m_Weapon.GetDef();

        // Muzzle flash and recoil are gun things; dig tools don't kick
        // or flash.
        if (!def || def->Kind != WeaponKind::Gun)
            return;

        m_MuzzleFlash = 0.05f;

        m_KnockVelX -= m_AimDirX * def->Recoil;
        m_VelocityY -= m_AimDirY * def->Recoil * 0.3f;
    }

    bool Actor::IsAlive() const
    {
        return m_Health > 0;
    }

    int Actor::GetHealth() const
    {
        return m_Health;
    }

    void Actor::TakeDamage(int damage, float impulseX, float impulseY)
    {
        if (!IsAlive())
            return;

        m_Health -= damage;
        m_HurtFlash = 0.12f;

        m_KnockVelX += impulseX;
        m_VelocityY += impulseY;

        // Big hits knock the actor off its feet.
        if (std::fabs(impulseX) + std::fabs(impulseY) > 220.0f)
        {
            m_Legs[0].Unplant();
            m_Legs[1].Unplant();
            m_Grounded = false;
        }
    }

    void Actor::Heal(int amount)
    {
        if (!IsAlive())
            return;

        m_Health = std::min(100, m_Health + amount);
    }

    void Actor::Gib(ParticleSystem& particles)
    {
        const float cx = GetCenterX();
        const float cy = GetCenterY();

        particles.BurstBlood(cx, cy, 46, 260.0f);

        // Chunks: uniform, skin and helmet colored.
        struct ChunkColor { Uint8 R, G, B; };

        const ChunkColor colors[] =
        {
            { 96, 108, 74 },   // uniform
            { 96, 108, 74 },
            { 214, 164, 110 }, // skin
            { 90, 106, 68 },   // helmet
            { 158, 24, 24 },   // meat
            { 158, 24, 24 },
            { 86, 90, 104 },   // pants
            { 56, 46, 38 },    // boot
        };

        int index = 0;

        for (const ChunkColor& color : colors)
        {
            const float angle =
                static_cast<float>(index) * 0.785f + 0.4f;

            particles.SpawnGib(
                cx,
                cy - 8.0f + static_cast<float>(index % 3) * 6.0f,
                std::cos(angle) * (160.0f + (index % 4) * 60.0f),
                -180.0f - (index % 5) * 40.0f,
                static_cast<Uint8>(color.R * m_TintR / 255),
                static_cast<Uint8>(color.G * m_TintG / 255),
                static_cast<Uint8>(color.B * m_TintB / 255),
                2 + (index % 2));

            index++;
        }
    }

    void Actor::ResetVitals()
    {
        m_Health = 100;
        m_Fuel = 1.0f;
        m_KnockVelX = 0.0f;
        m_VelocityY = 0.0f;
    }

    void Actor::SetTeam(int team)
    {
        m_Team = team;
    }

    int Actor::GetTeam() const
    {
        return m_Team;
    }

    float Actor::GetFuel() const
    {
        return m_Fuel;
    }

    bool Actor::IsJetting() const
    {
        return m_Jetting;
    }

    float Actor::GetX() const { return m_X; }
    float Actor::GetY() const { return m_Y; }
    float Actor::GetWidth() const { return BodyWidth; }
    float Actor::GetHeight() const { return BodyHeight; }

    float Actor::GetCenterX() const { return m_X + BodyWidth * 0.5f; }

    float Actor::GetCenterY() const
    {
        // Center of the whole figure, legs included.
        return m_Y + (BodyHeight + CurrentStandHeight()) * 0.5f;
    }

    float Actor::GetVelocityX() const { return m_VelocityX; }
    float Actor::GetVelocityY() const { return m_VelocityY; }

    bool Actor::IsGrounded() const { return m_Grounded; }

    const Limb& Actor::GetLeg(int index) const
    {
        return m_Legs[index];
    }

    float Actor::ShoulderX() const
    {
        return m_X + BodyWidth * 0.5f;
    }

    float Actor::ShoulderY() const
    {
        return m_Y + ShoulderOffsetY;
    }

    bool Actor::IsBodyBoxFree(const Terrain& terrain, float x, float y) const
    {
        const float left = x + 0.5f;
        const float right = x + BodyWidth - 0.5f;
        const float top = y + 0.5f;
        const float bottom = y + BodyHeight - 0.5f;

        for (float sampleX = left; ; sampleX += SampleSpacing)
        {
            const float clampedX = std::fmin(sampleX, right);

            if (terrain.IsSolid(clampedX, top) ||
                terrain.IsSolid(clampedX, bottom))
            {
                return false;
            }

            if (clampedX >= right)
                break;
        }

        for (float sampleY = top; ; sampleY += SampleSpacing)
        {
            const float clampedY = std::fmin(sampleY, bottom);

            if (terrain.IsSolid(left, clampedY) ||
                terrain.IsSolid(right, clampedY))
            {
                return false;
            }

            if (clampedY >= bottom)
                break;
        }

        return true;
    }

    void Actor::MoveHorizontal(const Terrain& terrain, float delta)
    {
        const int steps = static_cast<int>(std::ceil(std::fabs(delta)));

        if (steps == 0)
            return;

        const float step = delta / static_cast<float>(steps);

        for (int i = 0; i < steps; i++)
        {
            if (IsBodyBoxFree(terrain, m_X + step, m_Y))
            {
                m_X += step;
                continue;
            }

            // Ride up over rising ground within a small clearance; real
            // steps are the legs' job.
            bool moved = false;

            for (int lift = 1; lift <= BodyClearance; lift++)
            {
                if (IsBodyBoxFree(
                    terrain,
                    m_X + step,
                    m_Y - static_cast<float>(lift)))
                {
                    m_X += step;
                    m_Y -= static_cast<float>(lift);
                    moved = true;
                    break;
                }
            }

            // Crouched, a descending ceiling is ducked under instead. The
            // search covers the full stand-to-crouch height difference:
            // stepping down gradually (via MoveVerticalToward) can get
            // stuck if the body is still wedged against the ceiling at
            // every intermediate height, so this checks candidate drops
            // directly and can jump straight past the overlap in one go.
            if (!moved && m_Crouching)
            {
                const int maxDrop = static_cast<int>(
                    StandHeight - CurrentStandHeight()) + 8;

                for (int drop = 1; drop <= maxDrop; drop++)
                {
                    if (IsBodyBoxFree(
                        terrain,
                        m_X + step,
                        m_Y + static_cast<float>(drop)))
                    {
                        m_X += step;
                        m_Y += static_cast<float>(drop);
                        moved = true;
                        break;
                    }
                }
            }

            if (!moved)
            {
                m_VelocityX = 0.0f;
                m_KnockVelX = 0.0f;
                break;
            }
        }
    }

    void Actor::MoveVerticalToward(
        const Terrain& terrain,
        float targetY,
        float deltaTime)
    {
        const float maxStep = ClimbRate * deltaTime;
        const float delta = std::clamp(targetY - m_Y, -maxStep, maxStep);

        const int steps = static_cast<int>(std::ceil(std::fabs(delta)));

        if (steps == 0)
            return;

        const float step = delta / static_cast<float>(steps);

        for (int i = 0; i < steps; i++)
        {
            if (!IsBodyBoxFree(terrain, m_X, m_Y + step))
                break;

            m_Y += step;
        }
    }

    void Actor::MoveAirborne(const Terrain& terrain, float deltaTime)
    {
        const float deltaX = m_VelocityX * deltaTime;
        const int stepsX = static_cast<int>(std::ceil(std::fabs(deltaX)));

        if (stepsX > 0)
        {
            const float stepX = deltaX / static_cast<float>(stepsX);

            for (int i = 0; i < stepsX; i++)
            {
                if (!IsBodyBoxFree(terrain, m_X + stepX, m_Y))
                {
                    m_VelocityX = 0.0f;
                    m_KnockVelX = 0.0f;
                    break;
                }

                m_X += stepX;
            }
        }

        const float deltaY = m_VelocityY * deltaTime;
        const int stepsY = static_cast<int>(std::ceil(std::fabs(deltaY)));

        if (stepsY > 0)
        {
            const float stepY = deltaY / static_cast<float>(stepsY);

            for (int i = 0; i < stepsY; i++)
            {
                if (!IsBodyBoxFree(terrain, m_X, m_Y + stepY))
                {
                    // Torso landed on terrain directly (a rim or spike
                    // narrower than the legs' stance): crouch onto it.
                    if (stepY > 0.0f)
                    {
                        const float bottom = m_Y + BodyHeight;

                        m_Legs[0].Plant(HipWorldX(0), bottom + 2.0f);
                        m_Legs[1].Plant(HipWorldX(1), bottom + 2.0f);
                    }

                    m_VelocityY = 0.0f;
                    break;
                }

                m_Y += stepY;
            }
        }
    }

    float Actor::CurrentStandHeight() const
    {
        // Crouching folds the legs almost flat, dropping the whole
        // figure from ~64px to ~48px so it fits into tight tunnels.
        return m_Crouching ? 8.0f : StandHeight;
    }

    bool Actor::IsCrouching() const
    {
        return m_Crouching;
    }

    bool Actor::FindFoothold(
        const Terrain& terrain,
        float probeX,
        float& footholdY) const
    {
        const float bodyBottom = m_Y + BodyHeight;

        const int top = static_cast<int>(bodyBottom - MaxStepUp);
        const int bottom = static_cast<int>(
            bodyBottom + CurrentStandHeight() + MaxStepDown);

        for (int y = top; y <= bottom; y++)
        {
            if (terrain.IsSolid(probeX, static_cast<float>(y)) &&
                !terrain.IsSolid(probeX, static_cast<float>(y - 1)))
            {
                footholdY = static_cast<float>(y);
                return true;
            }
        }

        return false;
    }

    bool Actor::FindReachableFoothold(
        const Terrain& terrain,
        int leg,
        float probeX,
        float& footholdY) const
    {
        if (!FindFoothold(terrain, probeX, footholdY))
            return false;

        const float dx = probeX - HipWorldX(leg);
        const float dy = footholdY - HipWorldY(leg);

        return std::sqrt(dx * dx + dy * dy) <= PlantReach;
    }

    void Actor::UpdateWalkGait(const Terrain& terrain, float deltaTime)
    {
        Limb& swing = m_Legs[m_SwingLeg];
        Limb& stance = m_Legs[1 - m_SwingLeg];

        swing.Unplant();

        // The gait works in the direction of travel, which is not always
        // the facing direction (aiming backwards while walking).
        const float gaitDir = m_VelocityX < 0.0f ? -1.0f : 1.0f;

        // If the stance leg is close to full stretch (fast on downhill
        // slopes), put the swinging foot down under its hip now rather
        // than finishing the full stride and losing footing.
        if (stance.IsPlanted())
        {
            const float stretchX =
                stance.GetFootX() - HipWorldX(1 - m_SwingLeg);
            const float stretchY =
                stance.GetFootY() - HipWorldY(1 - m_SwingLeg);

            const float stretch =
                std::sqrt(stretchX * stretchX + stretchY * stretchY);

            if (stretch > StanceStrain)
            {
                const float hipX = HipWorldX(m_SwingLeg);

                float emergencyY = 0.0f;

                if (FindReachableFoothold(
                    terrain, m_SwingLeg, hipX, emergencyY))
                {
                    swing.Plant(hipX, emergencyY);
                    m_SwingLeg = 1 - m_SwingLeg;
                    return;
                }
            }
        }

        // Reach for a foothold ahead of the swinging leg's hip; if the
        // ground there is out of reach, try progressively closer.
        const float hipX = HipWorldX(m_SwingLeg);

        float footholdY = 0.0f;
        bool found = false;
        float probeX = 0.0f;

        for (float fraction = 1.0f; fraction >= 0.0f; fraction -= 0.5f)
        {
            probeX = hipX + gaitDir * StepAhead * fraction;

            if (FindReachableFoothold(terrain, m_SwingLeg, probeX, footholdY))
            {
                found = true;
                break;
            }
        }

        if (found)
        {
            const float swingSpeed = std::max(
                140.0f,
                std::fabs(m_VelocityX) * 2.6f);

            if (swing.SwingToward(
                probeX,
                footholdY,
                swingSpeed,
                FootLift,
                deltaTime))
            {
                swing.Plant(probeX, footholdY);
                m_SwingLeg = 1 - m_SwingLeg;
            }
        }
        else
        {
            // Nothing to stand on ahead (walking off an edge): the foot
            // keeps reaching forward and down until the stance leg gives
            // out and the actor falls.
            swing.DangleToward(
                hipX + gaitDir * StepAhead,
                HipWorldY(m_SwingLeg) + swing.GetReach() * 0.85f,
                deltaTime);
        }
    }

    void Actor::UpdateIdleFeet(const Terrain& terrain, float deltaTime)
    {
        for (int i = 0; i < 2; i++)
        {
            if (m_Legs[i].IsPlanted())
                continue;

            const float hipX = HipWorldX(i);

            float footholdY = 0.0f;

            if (FindReachableFoothold(terrain, i, hipX, footholdY))
            {
                if (m_Legs[i].SwingToward(
                    hipX,
                    footholdY,
                    120.0f,
                    FootLift * 0.5f,
                    deltaTime))
                {
                    m_Legs[i].Plant(hipX, footholdY);
                }
            }
            else
            {
                m_Legs[i].DangleToward(
                    hipX,
                    HipWorldY(i) + m_Legs[i].GetReach() * 0.85f,
                    deltaTime);
            }
        }
    }

    void Actor::UpdateDanglingFeet(float deltaTime)
    {
        for (int i = 0; i < 2; i++)
        {
            m_Legs[i].Unplant();

            m_Legs[i].DangleToward(
                HipWorldX(i) + m_FacingDir * 2.0f,
                HipWorldY(i) + m_Legs[i].GetReach() * 0.8f,
                deltaTime);
        }
    }

    void Actor::TryLand(const Terrain& terrain)
    {
        // While falling, a leg catches the ground once a reachable surface
        // comes within range below. Each hip probes a few nearby columns
        // so a slope beside the hip still counts.
        for (int i = 0; i < 2; i++)
        {
            for (float offset : { 0.0f, 6.0f, -6.0f })
            {
                const float probeX = HipWorldX(i) + offset;

                float footholdY = 0.0f;

                if (FindReachableFoothold(terrain, i, probeX, footholdY))
                {
                    m_Legs[i].Plant(probeX, footholdY);
                    break;
                }
            }
        }
    }

    float Actor::HipWorldX(int leg) const
    {
        // Hips stay fixed to the body regardless of facing: mirroring
        // them on a turn used to teleport the joints away from planted
        // feet and made the legs pop when reversing direction.
        return m_X + m_Legs[leg].GetHipOffsetX();
    }

    float Actor::HipWorldY(int leg) const
    {
        return m_Y + m_Legs[leg].GetHipOffsetY();
    }
}
