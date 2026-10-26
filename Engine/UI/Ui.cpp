#include "Ui.h"

#include "../Audio/Audio.h"
#include "../Core/Window.h"
#include "../Graphics/PixelFont.h"
#include "../Input/Input.h"

#include <algorithm>
#include <cmath>

namespace Atlas
{
    namespace
    {
        constexpr float LabelScale = 2.0f;
    }

    Ui::Ui(Window& window)
        : m_Window(window)
    {
    }

    void Ui::Begin(float time)
    {
        m_MouseX = Input::GetMouseX();
        m_MouseY = Input::GetMouseY();
        m_MouseDown = Input::IsMouseButtonDown(SDL_BUTTON_LEFT);
        m_Clicked = Input::WasMouseButtonPressed(SDL_BUTTON_LEFT);
        m_FrameTime = std::clamp(time - m_Time, 0.0f, 0.1f);
        m_Time = time;
        m_FieldUsed = false;

        // Forget controls that weren't drawn last frame (another screen).
        std::erase_if(m_Anims, [](const Anim& anim) { return !anim.Seen; });

        for (Anim& anim : m_Anims)
            anim.Seen = false;

        if (!m_MouseDown)
        {
            m_DragX = -1.0f;
            m_DragY = -1.0f;
        }
    }

    void Ui::End()
    {
        if (!m_FieldUsed)
        {
            m_FocusX = -1.0f;
            m_FocusY = -1.0f;
        }

        m_Window.SetTextInput(m_FocusX >= 0.0f);
    }

    bool Ui::Hovered(float x, float y, float w, float h) const
    {
        return m_MouseX >= x && m_MouseX < x + w && m_MouseY >= y && m_MouseY < y + h;
    }

    void Ui::Text(float x, float y, float scale, const std::string& text,
        Uint8 r, Uint8 g, Uint8 b, int align)
    {
        const float width = PixelFont::Measure(text, scale);

        if (align == 1)
            x -= width * 0.5f;
        else if (align == 2)
            x -= width;

        PixelFont::Draw(m_Window, x + scale * 0.5f, y + scale * 0.5f, scale, text, 8, 8, 12, 160);
        PixelFont::Draw(m_Window, x, y, scale, text, r, g, b);
    }

    float Ui::Ease(float x, float y, bool target, float speed)
    {
        Anim* found = nullptr;

        for (Anim& anim : m_Anims)
        {
            if (anim.X == x && anim.Y == y)
            {
                found = &anim;
                break;
            }
        }

        if (!found)
        {
            m_Anims.push_back({ x, y, target ? 1.0f : 0.0f, true });
            return m_Anims.back().Value;
        }

        const float goal = target ? 1.0f : 0.0f;
        found->Value += (goal - found->Value) * (1.0f - std::exp(-m_FrameTime * speed));
        found->Seen = true;
        return found->Value;
    }

    void Ui::Chamfer(float x, float y, float w, float h, float cut,
        SDL_Color top, SDL_Color bottom)
    {
        cut = std::min({ cut, w, h });

        auto color = [&](float py)
        {
            const float t = h > 0.0f ? std::clamp((py - y) / h, 0.0f, 1.0f) : 0.0f;
            auto mix = [t](Uint8 a, Uint8 b)
            {
                return (static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t) / 255.0f;
            };
            return SDL_FColor{ mix(top.r, bottom.r), mix(top.g, bottom.g), mix(top.b, bottom.b), mix(top.a, bottom.a) };
        };

        auto vertex = [&](float px, float py)
        {
            SDL_Vertex v;
            v.position = { px, py };
            v.color = color(py);
            v.tex_coord = { 0.0f, 0.0f };
            return v;
        };

        // Pentagon as a fan from the bottom-left corner.
        const SDL_Vertex a = vertex(x, y + h);
        const SDL_Vertex b = vertex(x, y);
        const SDL_Vertex c = vertex(x + w - cut, y);
        const SDL_Vertex d = vertex(x + w, y + cut);
        const SDL_Vertex e = vertex(x + w, y + h);

        m_Window.DrawTrianglesScreen({ a, b, c, a, c, d, a, d, e });
    }

    void Ui::Frame(float x, float y, float w, float h, float lit, bool pressed)
    {
        const float cut = std::min(10.0f, h * 0.3f);

        // Hairline border: the same shape one pixel larger behind.
        const Uint8 edgeA = static_cast<Uint8>(46.0f + lit * 70.0f);
        Chamfer(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, cut + 0.5f,
            { static_cast<Uint8>(90 + lit * 120), static_cast<Uint8>(98 + lit * 70), static_cast<Uint8>(120 + lit * 10), edgeA },
            { static_cast<Uint8>(70 + lit * 100), static_cast<Uint8>(74 + lit * 60), static_cast<Uint8>(90 + lit * 10), static_cast<Uint8>(edgeA * 0.6f) });

        const float glow = pressed ? 0.0f : lit;
        Chamfer(x, y, w, h, cut,
            { static_cast<Uint8>(22 + glow * 26), static_cast<Uint8>(25 + glow * 20), static_cast<Uint8>(34 + glow * 12), 214 },
            { static_cast<Uint8>(10 + glow * 14), static_cast<Uint8>(11 + glow * 10), static_cast<Uint8>(17 + glow * 6), 226 });

        // Top sheen.
        m_Window.DrawScreenRect(x, y, w - cut, 1.0f, 255, 255, 255,
            static_cast<Uint8>(16.0f + lit * 24.0f));

        if (lit > 0.01f)
        {
            // Accent bar grows from the middle of the left edge.
            const float barH = (h - 8.0f) * lit;
            m_Window.DrawScreenRect(x, y + (h - barH) * 0.5f, 3.0f, barH, 255, 176, 72,
                static_cast<Uint8>(lit * 255.0f));
            m_Window.DrawGlowScreen(x + 2.0f, y + h * 0.5f, h * 0.7f, 255, 150, 50,
                static_cast<Uint8>(lit * 26.0f));
        }
    }

    void Ui::Panel(float x, float y, float w, float h)
    {
        const float cut = std::min(18.0f, h * 0.2f);

        Chamfer(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, cut + 0.5f,
            { 120, 130, 160, 70 }, { 80, 86, 110, 40 });
        Chamfer(x, y, w, h, cut, { 18, 20, 30, 228 }, { 7, 8, 13, 240 });
        m_Window.DrawScreenRect(x, y, w - cut, 1.0f, 210, 220, 255, 40);

        // Accent strip along the top-left, the house style's signature.
        m_Window.DrawScreenRect(x, y, std::min(64.0f, w * 0.25f), 2.0f, 255, 176, 72, 230);
    }

    bool Ui::Button(float x, float y, float w, float h, const std::string& label, bool enabled)
    {
        const bool hover = enabled && Hovered(x, y, w, h);
        const bool pressed = hover && m_MouseDown;
        const float lit = Ease(x, y, hover);

        Frame(x, y, w, h, lit, pressed);

        // Hover warms the label toward the accent and nudges it right.
        const float shade = enabled ? 214.0f + lit * 41.0f : 110.0f;
        Text(x + w * 0.5f + lit * 4.0f, y + (h - 5.0f * LabelScale) * 0.5f, LabelScale, label,
            static_cast<Uint8>(shade),
            static_cast<Uint8>(enabled ? shade - lit * 24.0f : shade),
            static_cast<Uint8>(enabled ? std::min(255.0f, shade + 10.0f) - lit * 90.0f : shade + 10.0f), 1);

        if (hover && m_Clicked)
        {
            Audio::Play(Sfx::UiClick, 0.5f);
            return true;
        }

        return false;
    }

    bool Ui::Choice(float x, float y, float w, float h, const std::string& label,
        const std::vector<std::string>& options, int& index)
    {
        if (options.empty())
            return false;

        index = std::clamp(index, 0, static_cast<int>(options.size()) - 1);

        const bool hover = Hovered(x, y, w, h);

        Frame(x, y, w, h, Ease(x, y, hover) * 0.7f);

        const float textY = y + (h - 5.0f * LabelScale) * 0.5f;
        Text(x + 14.0f, textY, LabelScale, label, 170, 176, 190);

        const float valueCenter = x + w - 110.0f;
        Text(valueCenter, textY, LabelScale, options[static_cast<std::size_t>(index)], 255, 236, 170, 1);
        Text(x + w - 210.0f, textY, LabelScale, "<", hover ? 230 : 120, hover ? 230 : 120, 140);
        Text(x + w - 18.0f, textY, LabelScale, ">", hover ? 230 : 120, hover ? 230 : 120, 140);

        if (hover && m_Clicked)
        {
            const int count = static_cast<int>(options.size());
            const bool back = m_MouseX < valueCenter;
            index = (index + (back ? count - 1 : 1)) % count;
            Audio::Play(Sfx::UiClick, 0.5f);
            return true;
        }

        return false;
    }

    bool Ui::Slider(float x, float y, float w, float h, const std::string& label, float& value)
    {
        const bool hover = Hovered(x, y, w, h);

        Frame(x, y, w, h, Ease(x, y, hover) * 0.7f);

        const float textY = y + (h - 5.0f * LabelScale) * 0.5f;
        Text(x + 14.0f, textY, LabelScale, label, 170, 176, 190);

        const float trackX = x + w - 220.0f;
        const float trackW = 200.0f;
        const float trackY = y + h * 0.5f - 3.0f;

        if (hover && m_Clicked)
        {
            m_DragX = x;
            m_DragY = y;
        }

        bool changed = false;

        if (m_DragX == x && m_DragY == y && m_MouseDown)
        {
            const float next = std::clamp((m_MouseX - trackX) / trackW, 0.0f, 1.0f);
            changed = next != value;
            value = next;
        }

        m_Window.DrawScreenRect(trackX, trackY + 2.0f, trackW, 2.0f, 60, 64, 84, 255);
        m_Window.DrawScreenRect(trackX, trackY + 1.0f, trackW * value, 4.0f, 255, 176, 72, 255);
        m_Window.DrawGlowScreen(trackX + trackW * value, trackY + 3.0f, 10.0f, 255, 160, 60, 40);
        m_Window.DrawScreenRect(trackX + trackW * value - 2.0f, trackY - 4.0f, 4.0f, 14.0f, 245, 246, 250, 255);

        Text(trackX - 14.0f, textY, LabelScale,
            std::to_string(static_cast<int>(std::lround(value * 100.0f))), 255, 236, 170, 2);

        return changed;
    }

    bool Ui::TextField(float x, float y, float w, float h, const std::string& label,
        std::string& text, std::size_t maxLength)
    {
        const bool hover = Hovered(x, y, w, h);

        if (m_Clicked)
        {
            if (hover)
            {
                m_FocusX = x;
                m_FocusY = y;
            }
            else if (m_FocusX == x && m_FocusY == y)
            {
                m_FocusX = -1.0f;
                m_FocusY = -1.0f;
            }
        }

        const bool focused = m_FocusX == x && m_FocusY == y;
        bool changed = false;

        if (focused)
        {
            m_FieldUsed = true;

            for (char c : Input::ConsumeText())
            {
                if (text.size() < maxLength && c >= 32 && c < 127)
                {
                    text.push_back(c);
                    changed = true;
                }
            }

            if (Input::WasKeyPressed(SDL_SCANCODE_BACKSPACE) && !text.empty())
            {
                text.pop_back();
                changed = true;
            }
        }

        Frame(x, y, w, h, Ease(x, y, focused || hover) * (focused ? 1.0f : 0.6f));

        const float textY = y + (h - 5.0f * LabelScale) * 0.5f;
        Text(x + 12.0f, textY, LabelScale, label, 170, 176, 190);

        const float valueX = x + w - 300.0f;
        Text(valueX, textY, LabelScale, text, 255, 236, 170);

        if (focused && std::fmod(m_Time, 1.0f) < 0.55f)
        {
            m_Window.DrawScreenRect(valueX + PixelFont::Measure(text, LabelScale) + 2.0f,
                textY - 1.0f, 2.0f, 12.0f, 255, 236, 170, 255);
        }

        return changed;
    }
}
