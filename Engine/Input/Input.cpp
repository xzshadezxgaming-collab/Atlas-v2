#include "Input.h"

#include <cstring>

namespace Atlas
{
    const bool* Input::s_KeyboardState = nullptr;
    bool Input::s_PreviousKeys[SDL_SCANCODE_COUNT] = {};

    SDL_Renderer* Input::s_Renderer = nullptr;
    float Input::s_MouseX = 0.0f;
    float Input::s_MouseY = 0.0f;
    SDL_MouseButtonFlags Input::s_MouseButtons = 0;
    SDL_MouseButtonFlags Input::s_PreviousMouseButtons = 0;

    void Input::Update()
    {
        if (s_KeyboardState)
        {
            std::memcpy(
                s_PreviousKeys,
                s_KeyboardState,
                sizeof(s_PreviousKeys));
        }

        s_PreviousMouseButtons = s_MouseButtons;

        s_KeyboardState = SDL_GetKeyboardState(nullptr);
        s_MouseButtons = SDL_GetMouseState(&s_MouseX, &s_MouseY);

        if (s_Renderer)
            SDL_RenderCoordinatesFromWindow(s_Renderer, s_MouseX, s_MouseY, &s_MouseX, &s_MouseY);
    }

    void Input::SetRenderer(SDL_Renderer* renderer)
    {
        s_Renderer = renderer;
    }

    bool Input::IsKeyDown(SDL_Scancode key)
    {
        if (s_KeyboardState == nullptr)
        {
            return false;
        }

        return s_KeyboardState[key];
    }

    bool Input::WasKeyPressed(SDL_Scancode key)
    {
        if (s_KeyboardState == nullptr)
            return false;

        return s_KeyboardState[key] && !s_PreviousKeys[key];
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
        return (s_MouseButtons & SDL_BUTTON_MASK(button)) != 0 &&
            (s_PreviousMouseButtons & SDL_BUTTON_MASK(button)) == 0;
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
