#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class Window;

    // Tiny built-in 3x5 pixel font (A-Z, 0-9, and a few symbols), drawn
    // as screen-space rects. No font files needed.
    class PixelFont
    {
    public:
        static void Draw(
            Window& window,
            float x,
            float y,
            float scale,
            const std::string& text,
            Uint8 r,
            Uint8 g,
            Uint8 b,
            Uint8 a = 255);

        // Pixel width of the rendered string.
        static float Measure(const std::string& text, float scale);
    };
}
