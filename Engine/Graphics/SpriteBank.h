#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace Atlas
{
    // One named image from the sprite pack, with the pixel it is placed
    // and rotated around.
    struct SpriteFrame
    {
        SDL_Texture* Texture = nullptr;
        float Width = 0.0f;
        float Height = 0.0f;
        float PivotX = 0.0f;
        float PivotY = 0.0f;
    };

    // The client's sprite pack (Assets/Sprites, listed in sprites.ini).
    // Anything that draws asks for its sprite by name and falls back to
    // its procedural drawing when the pack has none, so headless runs and
    // a missing Assets folder still work.
    class SpriteBank
    {
    public:
        // Loads every sprite listed in <folder>/sprites.ini. Returns how
        // many loaded.
        static int Load(SDL_Renderer* renderer, const std::string& folder);
        static void Unload();

        static const SpriteFrame* Find(const std::string& name);
    };
}
