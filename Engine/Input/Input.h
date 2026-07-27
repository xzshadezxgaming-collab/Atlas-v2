#pragma once

#include <SDL3/SDL.h>

namespace Atlas
{
    class Input
    {
    public:
        static void Update();

        static bool IsKeyDown(SDL_Scancode key);

        // True only on the frame the key went down.
        static bool WasKeyPressed(SDL_Scancode key);

        // Mouse position in window coordinates.
        static float GetMouseX();
        static float GetMouseY();

        // button is one of SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, ...
        static bool IsMouseButtonDown(int button);

        // True only on the frame the button went down.
        static bool WasMouseButtonPressed(int button);

        // Scroll wheel: accumulated by the event loop, consumed by the
        // game once per frame (positive = scrolled up).
        static void AccumulateWheel(float amount);
        static int ConsumeWheelSteps();

    private:
        static float s_WheelAccumulator;

        static const bool* s_KeyboardState;
        static bool s_PreviousKeys[SDL_SCANCODE_COUNT];

        static float s_MouseX;
        static float s_MouseY;
        static SDL_MouseButtonFlags s_MouseButtons;
        static SDL_MouseButtonFlags s_PreviousMouseButtons;
    };
}
