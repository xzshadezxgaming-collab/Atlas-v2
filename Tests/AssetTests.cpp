// Checks the shipped art and sound packs: everything sprites.ini lists
// loads, every weapon and build has its art, and every sound effect has
// at least one take that loads. Runs headless (SDL loads files without
// a window or audio device).

#include "Audio/Audio.h"
#include "Combat/Weapon.h"
#include "Core/IniFile.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <set>
#include <string>

using namespace Atlas;

namespace
{
    int g_Failures = 0;

    void Check(bool condition, const std::string& what)
    {
        if (!condition)
        {
            std::printf("  FAIL: %s\n", what.c_str());
            g_Failures++;
        }
    }

    void TestSprites(const std::string& folder)
    {
        std::printf("sprite pack\n");

        IniFile ini;
        Check(ini.Load(folder + "/sprites.ini"), "sprites.ini loads");

        std::set<std::string> names;

        for (const std::string& name : ini.GetSectionNames())
        {
            names.insert(name);

            const std::string file = folder + "/" + ini.GetString(name, "File", name + ".bmp");
            SDL_Surface* surface = SDL_LoadBMP(file.c_str());

            Check(surface != nullptr, name + ": " + file + " loads");

            if (!surface)
                continue;

            const float pivotX = ini.GetFloat(name, "PivotX", 0.0f);
            const float pivotY = ini.GetFloat(name, "PivotY", 0.0f);

            Check(pivotX >= 0.0f && pivotX <= static_cast<float>(surface->w) &&
                pivotY >= 0.0f && pivotY <= static_cast<float>(surface->h),
                name + ": pivot inside the image");

            SDL_DestroySurface(surface);
        }

        for (const char* body : { "body_scout", "body_rifleman", "body_heavy" })
            Check(names.count(body) == 1, std::string("has ") + body);

        for (const WeaponDef& def : BuiltInWeaponDefs())
        {
            const std::string sprite = WeaponSpriteName(def);
            Check(names.count(sprite) == 1, def.Name + " has " + sprite);
            Check(names.count("icon_" + sprite) == 1, def.Name + " has icon_" + sprite);
        }

        for (const char* other : { "ship_team0", "ship_team1", "drone_team0", "drone_team1",
            "crate_drone", "crate_weapon", "crate_supply", "medkit", "rocket", "grenade",
            "icon_health", "icon_fuel", "icon_ammo", "icon_gold" })
        {
            Check(names.count(other) == 1, std::string("has ") + other);
        }

        std::printf("  %zu sprites\n", names.size());
    }

    void TestSounds(const std::string& folder)
    {
        std::printf("sound pack\n");

        int takes = 0;

        for (int i = 0; i < static_cast<int>(Sfx::Count); i++)
        {
            const std::string name = Audio::GetSoundName(static_cast<Sfx>(i));
            int found = 0;

            for (int take = 0; take <= 16; take++)
            {
                const std::string path = folder + "/" + name +
                    (take == 0 ? std::string() : "_" + std::to_string(take)) + ".wav";

                SDL_AudioSpec spec;
                Uint8* data = nullptr;
                Uint32 length = 0;

                if (!SDL_LoadWAV(path.c_str(), &spec, &data, &length))
                {
                    if (take > 0)
                        break;
                    continue;
                }

                Check(length > 0, path + " has audio");
                SDL_free(data);
                found++;
            }

            Check(found > 0, name + " has a take");
            takes += found;
        }

        std::printf("  %d takes\n", takes);
    }
}

int main()
{
    const std::string assets = ATLAS_ASSET_DIR;

    TestSprites(assets + "/Sprites");
    TestSounds(assets + "/Sounds");

    if (g_Failures > 0)
    {
        std::printf("%d check(s) failed\n", g_Failures);
        return 1;
    }

    std::printf("all asset checks passed\n");
    return 0;
}
