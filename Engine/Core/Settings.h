#pragma once

#include <string>

namespace Atlas
{
    // Player preferences, kept in settings.ini under the user's data
    // folder (SDL_GetPrefPath), so they survive between runs.
    struct Settings
    {
        float Volume = 0.8f;      // 0..1, every sound
        bool ScreenShake = true;
        bool Fullscreen = false;
        bool ShowFps = false;

        // Last values typed into the join screen.
        std::string JoinAddress = "127.0.0.1";

        void Load();
        void Save() const;
    };
}
