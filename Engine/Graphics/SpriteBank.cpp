#include "SpriteBank.h"

#include "Texture2D.h"

#include "../Core/IniFile.h"

#include <map>
#include <memory>

namespace Atlas
{
    namespace
    {
        struct Entry
        {
            std::unique_ptr<Texture2D> Texture;
            SpriteFrame Frame;
        };

        std::map<std::string, Entry>& Entries()
        {
            static std::map<std::string, Entry> entries;
            return entries;
        }
    }

    int SpriteBank::Load(SDL_Renderer* renderer, const std::string& folder)
    {
        Unload();

        IniFile ini;

        if (!ini.Load(folder + "/sprites.ini"))
            return 0;

        int loaded = 0;

        for (const std::string& name : ini.GetSectionNames())
        {
            const std::string file = ini.GetString(name, "File", name + ".bmp");

            auto texture = std::make_unique<Texture2D>();

            if (!texture->Load(renderer, folder + "/" + file))
                continue;

            Entry entry;
            entry.Frame.Texture = texture->GetTexture();
            entry.Frame.Width = static_cast<float>(texture->GetWidth());
            entry.Frame.Height = static_cast<float>(texture->GetHeight());
            entry.Frame.PivotX = ini.GetFloat(name, "PivotX", 0.0f);
            entry.Frame.PivotY = ini.GetFloat(name, "PivotY", 0.0f);
            entry.Texture = std::move(texture);

            Entries()[name] = std::move(entry);
            loaded++;
        }

        return loaded;
    }

    void SpriteBank::Unload()
    {
        Entries().clear();
    }

    const SpriteFrame* SpriteBank::Find(const std::string& name)
    {
        auto& entries = Entries();
        auto it = entries.find(name);

        return it != entries.end() ? &it->second.Frame : nullptr;
    }
}
