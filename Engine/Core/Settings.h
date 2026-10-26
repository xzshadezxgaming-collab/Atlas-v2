#pragma once

#include <string>

namespace Atlas
{
    // Player preferences, kept in settings.ini under the user's data
    // folder (SDL_GetPrefPath), so they survive between runs.
    struct Settings
    {
        float Volume = 0.8f;      // 0..1, every sound
        float MusicVolume = 0.6f; // 0..1, on top of Volume
        int ScreenShake = 1;      // 0 off, 1 subtle, 2 strong
        bool Lighting = true;     // dynamic lights and dark caves
        bool Bloom = true;        // glow around bright light
        int Zoom = 2;             // index into ZoomLevels

        static constexpr float ZoomLevels[] = { 1.0f, 1.25f, 1.5f, 2.0f };
        static constexpr int ZoomCount = 4;
        float GetZoom() const { return ZoomLevels[Zoom]; }
        bool Fullscreen = false;
        bool ShowFps = false;

        // Last values typed into the join screen.
        std::string JoinAddress = "127.0.0.1";

        void Load();
        void Save() const;
    };
}
