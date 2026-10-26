#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace Atlas
{
    class Window;

    // Screen-space text. Once LoadFaces has found the TrueType faces in
    // Assets/Fonts, text is anti-aliased and rasterized at the display's
    // real pixel size (crisp at any window size); without them it falls
    // back to built-in pixel faces drawn as rects.
    //
    // Either way `scale` keeps its pixel-font meaning, so layouts carry
    // over: below 2 text is about 7px tall, from 2 up it is 5*scale, and
    // y is the top of the capitals. Large text uses the bold face.
    class PixelFont
    {
    public:
        // Loads the regular and bold faces (TTF/OTF). Returns false, and
        // keeps the pixel faces, if either can't be read.
        static bool LoadFaces(SDL_Renderer* renderer,
            const std::string& regularPath, const std::string& boldPath);

        static void Shutdown();

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
