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

        // Arm/weapon
        constexpr float ShoulderOffsetY = 14.0f;
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

        void DrawArmSegment(
            Window& window,
            float x0, float y0,
            float x1, float y1,
            Uint8 r, Uint8 g, Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float length = std::sqrt(dx * dx + dy * dy);

            const int steps = std::max(
                1,
                static_cast<int>(std::ceil(length / (ArmThickness * 0.5f))));

            for (int i = 0; i <= steps; i++)
            {
                const float t =
                    static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    x0 + dx * t - ArmThickness * 0.5f,
                    y0 + dy * t - ArmThickness * 0.5f,
                    ArmThickness,
                    ArmThickness,
                    r, g, b, 255);
            }
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
        m_TintB(255)
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
        bool jetpack)
    {
        if (!IsAlive())
            return;

        // Knockback decays; walking is layered on top of it.
        m_KnockVelX -= m_KnockVelX * std::min(1.0f, KnockbackDamping * deltaTime);

        m_VelocityX = moveInput * WalkSpeed + m_KnockVelX;

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

                MoveVerticalToward(
                    terrain,
                    supportY - StandHeight - BodyHeight,
                    deltaTime);
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
        const int nearLeg = m_FacingDir > 0.0f ? 1 : 0;
        const int farLeg = 1 - nearLeg;

        m_Legs[farLeg].Draw(
            window,
            HipWorldX(farLeg),
            HipWorldY(farLeg),
            m_FacingDir,
            false,
            m_TintR, m_TintG, m_TintB);

        if (m_BodyTexture.GetTexture())
        {
            window.DrawTexture(
                m_BodyTexture.GetTexture(),
                m_X - (SpriteWidth - BodyWidth) * 0.5f,
                m_Y,
                SpriteWidth,
                SpriteHeight,
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

        const Uint8 armR = static_cast<Uint8>(84 * m_TintR / 255);
        const Uint8 armG = static_cast<Uint8>(96 * m_TintG / 255);
        const Uint8 armB = static_cast<Uint8>(64 * m_TintB / 255);

        DrawArmSegment(window, sx, sy, elbowX, elbowY, armR, armG, armB);
        DrawArmSegment(window, elbowX, elbowY, handX, handY, armR, armG, armB);

        // The weapon: a chunky body at the hand and a barrel toward the aim.
        Uint8 gunR = 58, gunG = 58, gunB = 66;

        if (def.Kind == WeaponKind::Digger)
        {
            gunR = 122;
            gunG = 92;
            gunB = 52;
        }

        window.DrawFilledRect(
            handX - 3.0f,
            handY - 3.0f,
            7.0f,
            6.0f,
            gunR, gunG, gunB, 255);

        const float muzzleX = handX + m_AimDirX * def.BarrelLength;
        const float muzzleY = handY + m_AimDirY * def.BarrelLength;

        const float steps = std::ceil(def.BarrelLength / 2.0f);

        for (float i = 0.0f; i <= steps; i += 1.0f)
        {
            const float t = i / steps;

            window.DrawFilledRect(
                handX + (muzzleX - handX) * t - 1.5f,
                handY + (muzzleY - handY) * t - 1.5f,
                3.0f,
                3.0f,
                gunR, gunG, gunB, 255);
        }

        // Glove over the grip.
        window.DrawFilledRect(
            handX - 2.0f,
            handY - 2.0f,
            4.0f,
            4.0f,
            static_cast<Uint8>(70 * m_TintR / 255),
            static_cast<Uint8>(62 * m_TintG / 255),
            static_cast<Uint8>(52 * m_TintB / 255),
            255);

        if (m_MuzzleFlash > 0.0f)
        {
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
        m_MuzzleFlash = 0.05f;

        if (m_Weapon.GetDef())
        {
            const float recoil = m_Weapon.GetDef()->Recoil;

            m_KnockVelX -= m_AimDirX * recoil;
            m_VelocityY -= m_AimDirY * recoil * 0.3f;
        }
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
        return m_Y + (BodyHeight + StandHeight) * 0.5f;
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
            bool lifted = false;

            for (int lift = 1; lift <= BodyClearance; lift++)
            {
                if (IsBodyBoxFree(
                    terrain,
                    m_X + step,
                    m_Y - static_cast<float>(lift)))
                {
                    m_X += step;
                    m_Y -= static_cast<float>(lift);
                    lifted = true;
                    break;
                }
            }

            if (!lifted)
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

    bool Actor::FindFoothold(
        const Terrain& terrain,
        float probeX,
        float& footholdY) const
    {
        const float bodyBottom = m_Y + BodyHeight;

        const int top = static_cast<int>(bodyBottom - MaxStepUp);
        const int bottom = static_cast<int>(bodyBottom + StandHeight + MaxStepDown);

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
        // Mirror the hips when facing left so the front hip leads.
        const float offset = m_Legs[leg].GetHipOffsetX();

        if (m_FacingDir < 0.0f)
            return m_X + BodyWidth - offset;

        return m_X + offset;
    }

    float Actor::HipWorldY(int leg) const
    {
        return m_Y + m_Legs[leg].GetHipOffsetY();
    }
}
