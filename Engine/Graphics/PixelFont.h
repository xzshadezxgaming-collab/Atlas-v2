#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class Window;

    // Built-in pixel fonts (A-Z, 0-9, and a few symbols), drawn as
    // screen-space rects. No font files needed. Scales of 2 and up use a
    // chunky 3x5 face (4*scale advance); smaller scales use a crisp 5x7
    // face at one pixel per font pixel (6px advance, 7px tall).
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
