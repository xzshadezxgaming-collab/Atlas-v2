#include "Input.h"

namespace Atlas
{
    const bool* Input::s_KeyboardState = nullptr;

    void Input::Update()
    {
        s_KeyboardState = SDL_GetKeyboardState(nullptr);
    }

    bool Input::IsKeyDown(SDL_Scancode key)
    {
        if (s_KeyboardState == nullptr)
        {
            return false;
        }

        return s_KeyboardState[key];
    }
}