#pragma once

#include <SDL3/SDL.h>

namespace Atlas
{
    class Input
    {
    public:
        static void Update();

        static bool IsKeyDown(SDL_Scancode key);

    private:
        static const bool* s_KeyboardState;
    };
}