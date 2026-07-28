#pragma once

#include <cstddef>
#include <cstdint>

namespace Atlas
{
    enum class Material : std::uint8_t
    {
        Air = 0,
        Grass,
        Dirt,
        Stone,
        Gold,

        Count
    };

    struct MaterialInfo
    {
        const char* Name;

        std::uint8_t R;
        std::uint8_t G;
        std::uint8_t B;

        bool Solid;

        // How much a pixel of this material resists being dug or shot away.
        // Air is 0; harder materials take more energy to remove.
        float Strength;

        // Currency awarded per pixel destroyed, by any means (dig tools,
        // bullets, explosions). 0 for everything but valuable ores.
        int Value;
    };

    inline constexpr MaterialInfo MaterialTable[] =
    {
        //  Name      R    G    B    Solid  Strength  Value
        { "Air",     0,   0,   0,   false, 0.0f,      0 },
        { "Grass",  86,  140,  60,  true,  1.0f,       0 },
        { "Dirt",  116,   84,  48,  true,  1.5f,       0 },
        { "Stone", 100,  100, 108,  true,  8.0f,       0 },
        { "Gold",  218,  176,  56,  true,  5.0f,       1 },
    };

    static_assert(
        sizeof(MaterialTable) / sizeof(MaterialTable[0]) ==
        static_cast<std::size_t>(Material::Count),
        "MaterialTable must have one entry per Material");

    inline const MaterialInfo& GetMaterialInfo(Material material)
    {
        return MaterialTable[static_cast<std::size_t>(material)];
    }
}
