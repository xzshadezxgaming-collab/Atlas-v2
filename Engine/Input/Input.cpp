#include "Input.h"

#include <cstring>

namespace Atlas
{
    const bool* Input::s_KeyboardState = nullptr;
    bool Input::s_PreviousKeys[SDL_SCANCODE_COUNT] = {};

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
}
