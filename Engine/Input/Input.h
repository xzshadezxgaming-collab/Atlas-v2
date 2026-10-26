#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class Input
    {
    public:
        static void Update();

        // Mouse positions are reported in this renderer's logical
        // coordinates (the game's fixed view size, however the window is
        // scaled or letterboxed).
        static void SetRenderer(SDL_Renderer* renderer);

        static bool IsKeyDown(SDL_Scancode key);

        // True only on the frame the key went down.
        static bool WasKeyPressed(SDL_Scancode key);

        // Mouse position in render (logical view) coordinates.
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

        // Key and button presses seen by the event loop. They are latched
        // until the next Update, so a tap that goes down and up between
        // two frames still counts once.
        static void NoteKeyPressed(SDL_Scancode key);
        static void NoteMouseButtonPressed(int button);

        // Typed text (from SDL text input events) since the last call.
        static void AppendText(const char* text);
        static std::string ConsumeText();

    private:
        static std::string s_Text;

        static SDL_Renderer* s_Renderer;

        static float s_WheelAccumulator;

        // Our own copy of the keyboard: SDL's array is live and already
        // reflects this frame's events by the time Update runs, so it
        // can't serve as "last frame".
        static bool s_Keys[SDL_SCANCODE_COUNT];
        static bool s_HasKeys;

        // Presses gathered by the event loop since the last Update, and
        // the ones this frame reports.
        static bool s_PendingPresses[SDL_SCANCODE_COUNT];
        static bool s_Presses[SDL_SCANCODE_COUNT];
        static SDL_MouseButtonFlags s_PendingMousePresses;
        static SDL_MouseButtonFlags s_MousePresses;

        static float s_MouseX;
        static float s_MouseY;
        static SDL_MouseButtonFlags s_MouseButtons;
        static SDL_MouseButtonFlags s_PreviousMouseButtons;
    };
}
