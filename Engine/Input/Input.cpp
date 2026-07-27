#include "Input.h"

namespace Atlas
{
    const bool* Input::s_KeyboardState = nullptr;

    float Input::s_MouseX = 0.0f;
    float Input::s_MouseY = 0.0f;
    SDL_MouseButtonFlags Input::s_MouseButtons = 0;

    void Input::Update()
    {
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
}
