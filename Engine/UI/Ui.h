#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace Atlas
{
    class Window;

    // Immediate-mode widgets for menus: call them every frame between
    // Begin and End and they draw themselves and report clicks. Screen
    // space, pixel font, mouse driven.
    class Ui
    {
    public:
        explicit Ui(Window& window);

        // Reads this frame's mouse state. time drives hover animation.
        void Begin(float time);

        // Ends the frame: releases text input if no field asked for it.
        void End();

        // Plain label; align 0 = left, 1 = centered on x, 2 = right.
        void Text(float x, float y, float scale, const std::string& text,
            Uint8 r = 230, Uint8 g = 232, Uint8 b = 238, int align = 0);

        // A dark panel with a light border.
        void Panel(float x, float y, float w, float h);

        bool Button(float x, float y, float w, float h,
            const std::string& label, bool enabled = true);

        // "label   < value >": click either half to step through the
        // options. Returns true when the index changed.
        bool Choice(float x, float y, float w, float h, const std::string& label,
            const std::vector<std::string>& options, int& index);

        // Drag (or click) to set value in 0..1.
        bool Slider(float x, float y, float w, float h, const std::string& label,
            float& value);

        // Single-line text entry, focused by clicking it.
        bool TextField(float x, float y, float w, float h, const std::string& label,
            std::string& text, std::size_t maxLength = 40);

        bool Hovered(float x, float y, float w, float h) const;

    private:
        Window& m_Window;

        float m_MouseX = 0.0f;
        float m_MouseY = 0.0f;
        bool m_MouseDown = false;
        bool m_Clicked = false;
        float m_Time = 0.0f;

        // Identity of the focused text field / dragged slider (by
        // position, which is stable for a static layout).
        float m_FocusX = -1.0f;
        float m_FocusY = -1.0f;
        bool m_FieldUsed = false;

        float m_DragX = -1.0f;
        float m_DragY = -1.0f;
    };
}
