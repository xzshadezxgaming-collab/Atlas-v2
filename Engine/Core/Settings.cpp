#include "Settings.h"

#include "IniFile.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <fstream>

namespace Atlas
{
    namespace
    {
        std::string SettingsPath()
        {
            char* folder = SDL_GetPrefPath("Atlas", "Atlas");

            if (!folder)
                return std::string();

            std::string path = std::string(folder) + "settings.ini";
            SDL_free(folder);
            return path;
        }
    }

    void Settings::Load()
    {
        const std::string path = SettingsPath();

        if (path.empty())
            return;

        IniFile ini;

        if (!ini.Load(path))
            return;

        Volume = std::clamp(ini.GetFloat("Audio", "Volume", Volume), 0.0f, 1.0f);
        MusicVolume = std::clamp(ini.GetFloat("Audio", "Music", MusicVolume), 0.0f, 1.0f);
        ScreenShake = std::clamp(ini.GetInt("Video", "ScreenShake", ScreenShake), 0, 2);
        Fullscreen = ini.GetInt("Video", "Fullscreen", Fullscreen ? 1 : 0) != 0;
        ShowFps = ini.GetInt("Video", "ShowFps", ShowFps ? 1 : 0) != 0;
        JoinAddress = ini.GetString("Network", "JoinAddress", JoinAddress);
    }

    void Settings::Save() const
    {
        const std::string path = SettingsPath();

        if (path.empty())
            return;

        std::ofstream out(path);

        if (!out)
            return;

        out << "[Audio]\n"
            << "Volume = " << Volume << "\n"
            << "Music = " << MusicVolume << "\n\n"
            << "[Video]\n"
            << "ScreenShake = " << ScreenShake << "\n"
            << "Fullscreen = " << (Fullscreen ? 1 : 0) << "\n"
            << "ShowFps = " << (ShowFps ? 1 : 0) << "\n\n"
            << "[Network]\n"
            << "JoinAddress = " << JoinAddress << "\n";
    }
}
