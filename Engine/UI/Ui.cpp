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
        m_Time = time;
        m_FieldUsed = false;

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

    void Ui::Panel(float x, float y, float w, float h)
    {
        m_Window.DrawScreenRect(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, 120, 126, 148, 70);
        m_Window.DrawScreenRect(x, y, w, h, 12, 13, 20, 215);
        m_Window.DrawScreenRect(x, y, w, 1.0f, 170, 176, 198, 80);
    }

    bool Ui::Button(float x, float y, float w, float h, const std::string& label, bool enabled)
    {
        const bool hover = enabled && Hovered(x, y, w, h);
        const bool pressed = hover && m_MouseDown;

        const Uint8 border = hover ? 170 : 60;
        m_Window.DrawScreenRect(x, y, w, h, border, static_cast<Uint8>(border + 6), static_cast<Uint8>(border + 20), 255);

        const Uint8 fill = pressed ? 28 : (hover ? 48 : 26);
        m_Window.DrawScreenRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f,
            fill, static_cast<Uint8>(fill + 4), static_cast<Uint8>(fill + 14), 240);

        if (hover)
        {
            // A soft sweep of light across the face.
            const float sweep = std::fmod(m_Time * 0.8f, 1.0f);
            m_Window.DrawScreenRect(x + 1.0f + (w - 18.0f) * sweep, y + 1.0f, 16.0f, h - 2.0f,
                255, 255, 255, 14);
        }

        const Uint8 shade = enabled ? (hover ? 255 : 220) : 110;
        Text(x + w * 0.5f, y + (h - 5.0f * LabelScale) * 0.5f, LabelScale, label,
            shade, shade, static_cast<Uint8>(std::min(255, shade + 10)), 1);

        if (hover && m_Clicked)
        {
            Audio::Play(Sfx::Reload, 0.5f);
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
        const Uint8 border = hover ? 150 : 50;

        m_Window.DrawScreenRect(x, y, w, h, border, static_cast<Uint8>(border + 6), static_cast<Uint8>(border + 20), 255);
        m_Window.DrawScreenRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 22, 24, 34, 240);

        const float textY = y + (h - 5.0f * LabelScale) * 0.5f;
        Text(x + 12.0f, textY, LabelScale, label, 170, 176, 190);

        const float valueCenter = x + w - 110.0f;
        Text(valueCenter, textY, LabelScale, options[static_cast<std::size_t>(index)], 255, 236, 170, 1);
        Text(x + w - 210.0f, textY, LabelScale, "<", hover ? 230 : 120, hover ? 230 : 120, 140);
        Text(x + w - 18.0f, textY, LabelScale, ">", hover ? 230 : 120, hover ? 230 : 120, 140);

        if (hover && m_Clicked)
        {
            const int count = static_cast<int>(options.size());
            const bool back = m_MouseX < valueCenter;
            index = (index + (back ? count - 1 : 1)) % count;
            Audio::Play(Sfx::Reload, 0.5f);
            return true;
        }

        return false;
    }

    bool Ui::Slider(float x, float y, float w, float h, const std::string& label, float& value)
    {
        const bool hover = Hovered(x, y, w, h);
        const Uint8 border = hover ? 150 : 50;

        m_Window.DrawScreenRect(x, y, w, h, border, static_cast<Uint8>(border + 6), static_cast<Uint8>(border + 20), 255);
        m_Window.DrawScreenRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 22, 24, 34, 240);

        const float textY = y + (h - 5.0f * LabelScale) * 0.5f;
        Text(x + 12.0f, textY, LabelScale, label, 170, 176, 190);

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

        m_Window.DrawScreenRect(trackX, trackY, trackW, 6.0f, 40, 42, 54, 255);
        m_Window.DrawScreenRect(trackX, trackY, trackW * value, 6.0f, 140, 200, 255, 255);
        m_Window.DrawScreenRect(trackX + trackW * value - 3.0f, trackY - 4.0f, 6.0f, 14.0f, 235, 240, 250, 255);

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

        const Uint8 border = focused ? 200 : (hover ? 150 : 50);
        m_Window.DrawScreenRect(x, y, w, h, border, static_cast<Uint8>(border + 6), static_cast<Uint8>(border + 20), 255);
        m_Window.DrawScreenRect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 16, 18, 26, 245);

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
