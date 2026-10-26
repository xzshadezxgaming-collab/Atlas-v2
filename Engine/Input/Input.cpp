#include "Input.h"

#include <algorithm>

namespace Atlas
{
    bool Input::s_Keys[SDL_SCANCODE_COUNT] = {};
    bool Input::s_HasKeys = false;
    bool Input::s_PendingPresses[SDL_SCANCODE_COUNT] = {};
    bool Input::s_Presses[SDL_SCANCODE_COUNT] = {};
    SDL_MouseButtonFlags Input::s_PendingMousePresses = 0;
    SDL_MouseButtonFlags Input::s_MousePresses = 0;

    SDL_Renderer* Input::s_Renderer = nullptr;
    float Input::s_MouseX = 0.0f;
    float Input::s_MouseY = 0.0f;
    SDL_MouseButtonFlags Input::s_MouseButtons = 0;
    SDL_MouseButtonFlags Input::s_PreviousMouseButtons = 0;

    void Input::Update()
    {
        // Called after the frame's events were polled: whatever went
        // down since the last Update is this frame's presses.
        int count = 0;
        const bool* state = SDL_GetKeyboardState(&count);
        count = std::min(count, static_cast<int>(SDL_SCANCODE_COUNT));

        for (int i = 0; i < SDL_SCANCODE_COUNT; i++)
        {
            const bool down = state && i < count && state[i];

            // Also catch a key that went down without an event we saw
            // (focus regained with it held counts on the next frame).
            s_Presses[i] = s_PendingPresses[i] || (s_HasKeys && down && !s_Keys[i]);
            s_PendingPresses[i] = false;
            s_Keys[i] = down;
        }

        s_HasKeys = true;

        s_PreviousMouseButtons = s_MouseButtons;
        s_MouseButtons = SDL_GetMouseState(&s_MouseX, &s_MouseY);

        s_MousePresses = s_PendingMousePresses |
            (s_MouseButtons & ~s_PreviousMouseButtons);
        s_PendingMousePresses = 0;

        if (s_Renderer)
            SDL_RenderCoordinatesFromWindow(s_Renderer, s_MouseX, s_MouseY, &s_MouseX, &s_MouseY);
    }

    void Input::NoteKeyPressed(SDL_Scancode key)
    {
        if (key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT)
            s_PendingPresses[key] = true;
    }

    void Input::NoteMouseButtonPressed(int button)
    {
        if (button >= 1 && button <= 32)
            s_PendingMousePresses |= SDL_BUTTON_MASK(button);
    }

    void Input::SetRenderer(SDL_Renderer* renderer)
    {
        s_Renderer = renderer;
    }

    bool Input::IsKeyDown(SDL_Scancode key)
    {
        return key >= 0 && key < SDL_SCANCODE_COUNT && s_Keys[key];
    }

    bool Input::WasKeyPressed(SDL_Scancode key)
    {
        return key >= 0 && key < SDL_SCANCODE_COUNT && s_Presses[key];
    }

    float Input::GetMouseX()
    {
        return s_MouseX;
    }

    float Input::GetMouseY()
    {
        return s_MouseY;
    }

    bool Input::IsMouseButtonDown(int button)
    {
        return (s_MouseButtons & SDL_BUTTON_MASK(button)) != 0;
    }

    bool Input::WasMouseButtonPressed(int button)
    {
        return (s_MousePresses & SDL_BUTTON_MASK(button)) != 0;
    }

    float Input::s_WheelAccumulator = 0.0f;

    void Input::AccumulateWheel(float amount)
    {
        s_WheelAccumulator += amount;
    }

    int Input::ConsumeWheelSteps()
    {
        const int steps = static_cast<int>(s_WheelAccumulator);

        s_WheelAccumulator -= static_cast<float>(steps);

        // Treat any sub-step remainder as a full step so light scrolls
        // still switch.
        if (steps == 0 && s_WheelAccumulator > 0.5f)
        {
            s_WheelAccumulator = 0.0f;
            return 1;
        }

        if (steps == 0 && s_WheelAccumulator < -0.5f)
        {
            s_WheelAccumulator = 0.0f;
            return -1;
        }

        return steps;
    }

    std::string Input::s_Text;

    void Input::AppendText(const char* text)
    {
        if (text)
            s_Text += text;
    }

    std::string Input::ConsumeText()
    {
        std::string text;
        text.swap(s_Text);
        return text;
    }
}
