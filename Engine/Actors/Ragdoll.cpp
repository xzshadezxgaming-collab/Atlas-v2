#include "Ragdoll.h"

#include "Actor.h"
#include "Limb.h"

#include "../Core/Window.h"
#include "../Graphics/SpriteBank.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float Gravity = 1000.0f;
        constexpr float AirDamping = 0.995f;
        constexpr int Substeps = 2;
        constexpr int Iterations = 5;

        constexpr float CorpseLifetime = 30.0f;
        constexpr float PieceLifetime = 20.0f;

        constexpr float RadToDeg = 57.2957795f;

        // Body art layout (32x40, facing right): where the joints sit in
        // the sprite, and the rects the head and torso are cut from.
        constexpr float SpriteWidth = 32.0f;
        constexpr float SpriteHipX = 16.0f;
        constexpr float SpriteHipY = 38.0f;
        constexpr float SpriteNeckX = 19.0f;
        constexpr float SpriteNeckY = 21.0f;
        constexpr float SpriteCrownX = 19.0f;
        constexpr float SpriteCrownY = 3.0f;

        constexpr Uint8 OutlineR = 24;
        constexpr Uint8 OutlineG = 22;
        constexpr Uint8 OutlineB = 27;

        void Stamp(Window& window, float x0, float y0, float x1, float y1,
            float width, Uint8 r, Uint8 g, Uint8 b)
        {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const int steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy))));

            for (int i = 0; i <= steps; i++)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);

                window.DrawFilledRect(
                    std::round(x0 + dx * t - width * 0.5f),
                    std::round(y0 + dy * t - width * 0.5f),
                    width, width, r, g, b, 255);
            }
        }

        // Raw meat at a severed joint.
        void DrawStump(Window& window, float x, float y, float size)
        {
            window.DrawFilledRect(std::round(x - size * 0.5f - 1.0f), std::round(y - size * 0.5f - 1.0f),
                size + 2.0f, size + 2.0f, OutlineR, OutlineG, OutlineB, 255);
            window.DrawFilledRect(std::round(x - size * 0.5f), std::round(y - size * 0.5f),
                size, size, 150, 26, 24, 255);
            window.DrawFilledRect(std::round(x - size * 0.5f), std::round(y - size * 0.5f),
                size, 1.0f, 200, 70, 60, 255);
        }

        const char* BodySpriteName(int bodyClass)
        {
            return bodyClass == 0 ? "body_scout" : bodyClass == 2 ? "body_heavy" : "body_rifleman";
        }

        // Rotation that turns the sprite vector (from a to b, mirrored
        // for facing left) onto the world vector between two joints.
        float PieceAngle(float facing, float spriteAX, float spriteAY, float spriteBX, float spriteBY,
            float worldAX, float worldAY, float worldBX, float worldBY)
        {
            const float spriteAngle = std::atan2(spriteBY - spriteAY, (spriteBX - spriteAX) * facing);
            const float worldAngle = std::atan2(worldBY - worldAY, worldBX - worldAX);

            return (worldAngle - spriteAngle) * RadToDeg;
        }
    }

    Ragdoll::Ragdoll()
        : m_X{},
        m_Y{},
        m_PrevX{},
        m_PrevY{},
        m_Resting{},
        m_Sticks{},
        m_StickCount(0),
        m_Pieces(0),
        m_BodyClass(1),
        m_TintR(255),
        m_TintG(255),
        m_TintB(255),
        m_Facing(1.0f),
        m_BodyTexture(nullptr),
        m_Age(0.0f),
        m_Lifetime(PieceLifetime),
        m_Bleeding(0.0f),
        m_BleedTimer(0.0f),
        m_StillTime(0.0f)
    {
    }

    void Ragdoll::SetPieces(std::uint8_t pieces)
    {
        m_Pieces = pieces;
        m_Lifetime = (pieces & Torso) ? CorpseLifetime : PieceLifetime;
    }

    std::uint8_t Ragdoll::GetPieces() const
    {
        return m_Pieces;
    }

    void Ragdoll::SetJoint(Joint joint, float x, float y, float velX, float velY)
    {
        // Stored as verlet: the previous position carries the velocity
        // (scaled to one substep of a 60 Hz tick).
        const float step = 1.0f / (60.0f * Substeps);

        m_X[joint] = x;
        m_Y[joint] = y;
        m_PrevX[joint] = x - velX * step;
        m_PrevY[joint] = y - velY * step;
    }

    float Ragdoll::GetJointX(Joint joint) const { return m_X[joint]; }
    float Ragdoll::GetJointY(Joint joint) const { return m_Y[joint]; }

    bool Ragdoll::UsesJoint(int joint) const
    {
        switch (joint)
        {
        case Hip: return (m_Pieces & (Torso | LegB | LegF)) != 0;
        case Neck: return (m_Pieces & (Torso | Head | ArmB | ArmF)) != 0;
        case Crown: return (m_Pieces & Head) != 0;
        case KneeB: case FootB: return (m_Pieces & LegB) != 0;
        case KneeF: case FootF: return (m_Pieces & LegF) != 0;
        case ElbowB: case HandB: return (m_Pieces & ArmB) != 0;
        case ElbowF: case HandF: return (m_Pieces & ArmF) != 0;
        default: return false;
        }
    }

    void Ragdoll::AddStick(int a, int b)
    {
        if (m_StickCount >= MaxSticks)
            return;

        const float dx = m_X[b] - m_X[a];
        const float dy = m_Y[b] - m_Y[a];

        m_Sticks[m_StickCount++] = {
            static_cast<std::uint8_t>(a),
            static_cast<std::uint8_t>(b),
            std::max(1.0f, std::sqrt(dx * dx + dy * dy)) };
    }

    void Ragdoll::Bind()
    {
        m_StickCount = 0;

        if ((m_Pieces & Torso) != 0)
            AddStick(Hip, Neck);

        if ((m_Pieces & Head) != 0)
        {
            AddStick(Neck, Crown);

            // Head rigid on the shoulders while the torso is there.
            if ((m_Pieces & Torso) != 0)
                AddStick(Hip, Crown);
        }

        if ((m_Pieces & LegB) != 0)
        {
            AddStick(Hip, KneeB);
            AddStick(KneeB, FootB);
        }

        if ((m_Pieces & LegF) != 0)
        {
            AddStick(Hip, KneeF);
            AddStick(KneeF, FootF);
        }

        if ((m_Pieces & ArmB) != 0)
        {
            AddStick(Neck, ElbowB);
            AddStick(ElbowB, HandB);
        }

        if ((m_Pieces & ArmF) != 0)
        {
            AddStick(Neck, ElbowF);
            AddStick(ElbowF, HandF);
        }

        for (bool& resting : m_Resting)
            resting = false;
    }

    void Ragdoll::SetLook(int bodyClass, Uint8 tintR, Uint8 tintG, Uint8 tintB,
        float facing, SDL_Texture* bodyTexture)
    {
        m_BodyClass = bodyClass;
        m_TintR = tintR;
        m_TintG = tintG;
        m_TintB = tintB;
        m_Facing = facing < 0.0f ? -1.0f : 1.0f;
        m_BodyTexture = bodyTexture;
    }

    void Ragdoll::SetBleeding(float seconds)
    {
        m_Bleeding = seconds;
    }

    void Ragdoll::SolveTerrain(const Terrain& terrain, int joint)
    {
        float& x = m_X[joint];
        float& y = m_Y[joint];

        if (!terrain.IsSolid(x, y))
            return;

        const float vx = x - m_PrevX[joint];
        const float vy = y - m_PrevY[joint];

        if (!terrain.IsSolid(x, m_PrevY[joint]))
        {
            // Hit a floor (or ceiling): stop, bounce a little, and drag
            // along the ground.
            y = m_PrevY[joint];
            m_PrevX[joint] = x - vx * 0.55f;
            m_PrevY[joint] = y + vy * 0.2f;
        }
        else if (!terrain.IsSolid(m_PrevX[joint], y))
        {
            // Hit a wall.
            x = m_PrevX[joint];
            m_PrevX[joint] = x + vx * 0.3f;
        }
        else if (!terrain.IsSolid(m_PrevX[joint], m_PrevY[joint]))
        {
            x = m_PrevX[joint];
            y = m_PrevY[joint];
        }
        else
        {
            // Buried (terrain fell on it, or spawned inside): climb out.
            for (int lift = 1; lift <= 8; lift++)
            {
                if (!terrain.IsSolid(x, y - static_cast<float>(lift)))
                {
                    y -= static_cast<float>(lift);
                    break;
                }
            }

            m_PrevX[joint] = x;
            m_PrevY[joint] = y;
        }
    }

    void Ragdoll::Update(const Terrain& terrain, ParticleSystem& particles, float deltaTime)
    {
        m_Age += deltaTime;

        // Blood pumps from the open wounds for a while.
        if (m_Bleeding > 0.0f)
        {
            m_Bleeding -= deltaTime;
            m_BleedTimer -= deltaTime;

            if (m_BleedTimer <= 0.0f)
            {
                m_BleedTimer = 0.06f;

                // Where it bleeds from: a missing head leaves the neck
                // open, a missing limb its shoulder or hip, and a severed
                // piece its cut end.
                int source = -1;

                if ((m_Pieces & Torso) != 0)
                {
                    if ((m_Pieces & Head) == 0)
                        source = Neck;
                    else if ((m_Pieces & (LegB | LegF)) != (LegB | LegF))
                        source = Hip;
                    else if ((m_Pieces & (ArmB | ArmF)) != (ArmB | ArmF))
                        source = Neck;
                }
                else if ((m_Pieces & (LegB | LegF)) != 0)
                {
                    source = Hip;
                }
                else
                {
                    source = Neck;
                }

                if (source >= 0)
                {
                    const float spurt = std::min(1.0f, m_Bleeding) * 70.0f;
                    const float wobble = std::sin(m_Age * 23.0f) * spurt;

                    particles.SpawnBlood(m_X[source], m_Y[source] - 1.0f,
                        wobble, -spurt);
                }
            }
        }

        // Asleep once it has settled; a push wakes it.
        if (m_StillTime > 1.0f)
            return;

        const float step = deltaTime / static_cast<float>(Substeps);
        float moved = 0.0f;

        for (int sub = 0; sub < Substeps; sub++)
        {
            for (int j = 0; j < JointCount; j++)
            {
                if (!UsesJoint(j))
                    continue;

                const float vx = (m_X[j] - m_PrevX[j]) * AirDamping;
                const float vy = (m_Y[j] - m_PrevY[j]) * AirDamping;

                m_PrevX[j] = m_X[j];
                m_PrevY[j] = m_Y[j];

                m_X[j] += vx;
                m_Y[j] += vy + Gravity * step * step;

                SolveTerrain(terrain, j);

                moved = std::max(moved, std::fabs(vx) + std::fabs(vy));
            }

            for (int iteration = 0; iteration < Iterations; iteration++)
            {
                for (int s = 0; s < m_StickCount; s++)
                {
                    const Stick& stick = m_Sticks[s];

                    const float dx = m_X[stick.B] - m_X[stick.A];
                    const float dy = m_Y[stick.B] - m_Y[stick.A];
                    const float distance = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
                    const float correction = (distance - stick.Length) / distance * 0.5f;

                    const float ax = m_X[stick.A] + dx * correction;
                    const float ay = m_Y[stick.A] + dy * correction;
                    const float bx = m_X[stick.B] - dx * correction;
                    const float by = m_Y[stick.B] - dy * correction;

                    // Constraints never shove a joint into rock.
                    if (!terrain.IsSolid(ax, ay))
                    {
                        m_X[stick.A] = ax;
                        m_Y[stick.A] = ay;
                    }

                    if (!terrain.IsSolid(bx, by))
                    {
                        m_X[stick.B] = bx;
                        m_Y[stick.B] = by;
                    }
                }
            }
        }

        // Settled: nothing moving more than a hair per substep.
        if (moved < 0.06f)
            m_StillTime += deltaTime;
        else
            m_StillTime = 0.0f;
    }

    float Ragdoll::Push(float x, float y, float radius, float strength)
    {
        const float step = 1.0f / (60.0f * Substeps);
        float strongest = 0.0f;

        for (int j = 0; j < JointCount; j++)
        {
            if (!UsesJoint(j))
                continue;

            float dx = m_X[j] - x;
            float dy = m_Y[j] - y;
            const float distance = std::sqrt(dx * dx + dy * dy);

            if (distance > radius)
                continue;

            const float falloff = 1.0f - distance / radius;
            const float impulse = strength * falloff;

            dx /= distance + 0.001f;
            dy /= distance + 0.001f;

            // Verlet: velocity lives in the gap to the previous position.
            m_PrevX[j] -= dx * impulse * step;
            m_PrevY[j] -= (dy - 0.6f) * impulse * step;

            strongest = std::max(strongest, impulse);
        }

        if (strongest > 0.0f)
            m_StillTime = 0.0f;

        return strongest;
    }

    bool Ragdoll::Hit(float x, float y, float reach, float velX, float velY)
    {
        int nearest = -1;
        float best = reach * reach;

        for (int j = 0; j < JointCount; j++)
        {
            if (!UsesJoint(j))
                continue;

            const float dx = m_X[j] - x;
            const float dy = m_Y[j] - y;
            const float d = dx * dx + dy * dy;

            if (d < best)
            {
                best = d;
                nearest = j;
            }
        }

        if (nearest < 0)
            return false;

        const float step = 1.0f / (60.0f * Substeps);

        m_PrevX[nearest] -= velX * step;
        m_PrevY[nearest] -= velY * step;
        m_StillTime = 0.0f;

        return true;
    }

    void Ragdoll::GetBounds(float& minX, float& minY, float& maxX, float& maxY) const
    {
        minX = minY = 1.0e9f;
        maxX = maxY = -1.0e9f;

        for (int j = 0; j < JointCount; j++)
        {
            if (!UsesJoint(j))
                continue;

            minX = std::min(minX, m_X[j]);
            minY = std::min(minY, m_Y[j]);
            maxX = std::max(maxX, m_X[j]);
            maxY = std::max(maxY, m_Y[j]);
        }
    }

    float Ragdoll::GetCenterX() const
    {
        float minX, minY, maxX, maxY;
        GetBounds(minX, minY, maxX, maxY);
        return (minX + maxX) * 0.5f;
    }

    float Ragdoll::GetCenterY() const
    {
        float minX, minY, maxX, maxY;
        GetBounds(minX, minY, maxX, maxY);
        return (minY + maxY) * 0.5f;
    }

    bool Ragdoll::IsExpired() const
    {
        return m_Age >= m_Lifetime;
    }

    void Ragdoll::Expire()
    {
        m_Age = m_Lifetime;
    }

    void Ragdoll::Draw(Window& window) const
    {
        const BodyClass build = static_cast<BodyClass>(std::clamp(m_BodyClass, 0, 2));
        const LegStyle& legStyle = GetLegStyle(build);

        int sleeveR = 0, sleeveG = 0, sleeveB = 0;
        GetSleeveColor(build, sleeveR, sleeveG, sleeveB);

        const Uint8 armR = static_cast<Uint8>(sleeveR * m_TintR / 255);
        const Uint8 armG = static_cast<Uint8>(sleeveG * m_TintG / 255);
        const Uint8 armB = static_cast<Uint8>(sleeveB * m_TintB / 255);

        const bool hasTorso = (m_Pieces & Torso) != 0;

        // Shoulders sit a little down the torso from the neck; a lone arm
        // just hangs from its cut end.
        float shoulderX = m_X[Neck];
        float shoulderY = m_Y[Neck];

        if (hasTorso)
        {
            shoulderX += (m_X[Hip] - m_X[Neck]) * 0.15f;
            shoulderY += (m_Y[Hip] - m_Y[Neck]) * 0.15f;
        }

        auto drawLeg = [&](Joint knee, Joint foot, bool nearSide)
        {
            // The knee bends to whichever side of the hip-foot line it
            // ended up on; the toe follows it.
            const float cross =
                (m_X[foot] - m_X[Hip]) * (m_Y[knee] - m_Y[Hip]) -
                (m_Y[foot] - m_Y[Hip]) * (m_X[knee] - m_X[Hip]);
            const float bend = cross < 0.0f ? 1.0f : -1.0f;

            Limb::DrawShape(window, m_X[Hip], m_Y[Hip], m_X[knee], m_Y[knee],
                m_X[foot], m_Y[foot], 0.4f * bend, bend, nearSide, legStyle,
                m_TintR, m_TintG, m_TintB);
        };

        auto drawArm = [&](Joint elbow, Joint hand, int shade)
        {
            const Uint8 r = static_cast<Uint8>(std::max(0, armR + shade));
            const Uint8 g = static_cast<Uint8>(std::max(0, armG + shade));
            const Uint8 b = static_cast<Uint8>(std::max(0, armB + shade));

            Stamp(window, shoulderX, shoulderY, m_X[elbow], m_Y[elbow], 6.0f, OutlineR, OutlineG, OutlineB);
            Stamp(window, m_X[elbow], m_Y[elbow], m_X[hand], m_Y[hand], 6.0f, OutlineR, OutlineG, OutlineB);
            Stamp(window, shoulderX, shoulderY, m_X[elbow], m_Y[elbow], 4.0f, r, g, b);
            Stamp(window, m_X[elbow], m_Y[elbow], m_X[hand], m_Y[hand], 4.0f, r, g, b);

            window.DrawFilledRect(std::round(m_X[hand] - 2.0f), std::round(m_Y[hand] - 2.0f), 4.0f, 4.0f,
                static_cast<Uint8>(78 * m_TintR / 255),
                static_cast<Uint8>(68 * m_TintG / 255),
                static_cast<Uint8>(56 * m_TintB / 255), 255);
        };

        // Back limbs behind the body.
        if ((m_Pieces & ArmB) != 0)
            drawArm(ElbowB, HandB, -24);

        if ((m_Pieces & LegB) != 0)
            drawLeg(KneeB, FootB, false);

        const SpriteFrame* sprite = SpriteBank::Find(BodySpriteName(m_BodyClass));
        SDL_Texture* texture = sprite ? sprite->Texture : m_BodyTexture;
        const bool flip = m_Facing < 0.0f;

        if (texture)
            SDL_SetTextureColorMod(texture, m_TintR, m_TintG, m_TintB);

        if (hasTorso)
        {
            if (texture)
            {
                const float angle = PieceAngle(m_Facing,
                    SpriteHipX, SpriteHipY, SpriteNeckX, SpriteNeckY,
                    m_X[Hip], m_Y[Hip], m_X[Neck], m_Y[Neck]);

                window.DrawTextureRegion(texture, SpriteWidth,
                    0.0f, 21.0f, 32.0f, 19.0f,
                    SpriteHipX, SpriteHipY, m_X[Hip], m_Y[Hip], angle, flip);
            }
            else
            {
                Stamp(window, m_X[Hip], m_Y[Hip], m_X[Neck], m_Y[Neck], 12.0f, armR, armG, armB);
            }

            if ((m_Pieces & Head) == 0)
                DrawStump(window, m_X[Neck], m_Y[Neck], 5.0f);
        }

        if ((m_Pieces & Head) != 0)
        {
            if (texture)
            {
                const float angle = PieceAngle(m_Facing,
                    SpriteNeckX, SpriteNeckY, SpriteCrownX, SpriteCrownY,
                    m_X[Neck], m_Y[Neck], m_X[Crown], m_Y[Crown]);

                // Helmet and face, without the top of the backpack.
                window.DrawTextureRegion(texture, SpriteWidth,
                    8.0f, 0.0f, 22.0f, 16.0f,
                    SpriteNeckX, SpriteNeckY, m_X[Neck], m_Y[Neck], angle, flip);
                window.DrawTextureRegion(texture, SpriteWidth,
                    11.0f, 16.0f, 17.0f, 6.0f,
                    SpriteNeckX, SpriteNeckY, m_X[Neck], m_Y[Neck], angle, flip);
            }
            else
            {
                Stamp(window, m_X[Neck], m_Y[Neck], m_X[Crown], m_Y[Crown], 10.0f, 214, 164, 110);
            }

            if (!hasTorso)
                DrawStump(window, m_X[Neck], m_Y[Neck], 4.0f);
        }

        if (texture)
            SDL_SetTextureColorMod(texture, 255, 255, 255);

        if ((m_Pieces & LegF) != 0)
            drawLeg(KneeF, FootF, true);

        if ((m_Pieces & ArmF) != 0)
            drawArm(ElbowF, HandF, 0);

        // Cut ends of severed limbs, and stumps left on a corpse.
        if (!hasTorso)
        {
            if ((m_Pieces & (LegB | LegF)) != 0)
                DrawStump(window, m_X[Hip], m_Y[Hip], 4.0f);
            else if ((m_Pieces & (ArmB | ArmF)) != 0)
                DrawStump(window, m_X[Neck], m_Y[Neck], 3.0f);
        }
        else
        {
            if ((m_Pieces & ArmF) == 0)
                DrawStump(window, shoulderX, shoulderY, 4.0f);

            if ((m_Pieces & LegF) == 0 || (m_Pieces & LegB) == 0)
                DrawStump(window, m_X[Hip], m_Y[Hip], 4.0f);
        }
    }
}
