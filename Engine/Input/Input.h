#pragma once

#include <SDL3/SDL.h>

namespace Atlas
{
    class Input
    {
    public:
        static void Update();

        static bool IsKeyDown(SDL_Scancode key);

        // Mouse position in window coordinates.
        static float GetMouseX();
        static float GetMouseY();

        // button is one of SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, ...
        static bool IsMouseButtonDown(int button);

    private:
        static const bool* s_KeyboardState;

        static float s_MouseX;
        static float s_MouseY;
        static SDL_MouseButtonFlags s_MouseButtons;
    };
}
