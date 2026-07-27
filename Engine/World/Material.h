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
    };

    inline constexpr MaterialInfo MaterialTable[] =
    {
        //  Name      R    G    B    Solid  Strength
        { "Air",     0,   0,   0,   false, 0.0f  },
        { "Grass",  86,  140,  60,  true,  1.0f  },
        { "Dirt",  116,   84,  48,  true,  1.5f  },
        { "Stone", 100,  100, 108,  true,  8.0f  },
        { "Gold",  218,  176,  56,  true,  5.0f  },
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
