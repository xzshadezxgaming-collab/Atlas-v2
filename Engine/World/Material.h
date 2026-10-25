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
        Sand,
        Gravel,
        Water,
        Lava,
        Obsidian,
        Bedrock,

        Count
    };

    // How a material behaves in the terrain simulation.
    enum class MaterialBehavior : std::uint8_t
    {
        Gas,    // empty space
        Static, // stays where it is until destroyed
        Powder, // falls and piles up (sand, gravel)
        Liquid, // falls and spreads sideways (water, lava)
    };

    struct MaterialInfo
    {
        const char* Name;

        std::uint8_t R;
        std::uint8_t G;
        std::uint8_t B;

        // Blocks movement, bullets and footing.
        bool Solid;

        MaterialBehavior Behavior;

        // Heavier powders and liquids sink through lighter liquids.
        std::uint8_t Density;

        // How much a pixel of this material resists being dug or shot away.
        // Air is 0; harder materials take more energy to remove.
        float Strength;

        // Currency awarded per pixel destroyed, by any means (dig tools,
        // bullets, explosions). 0 for everything but valuable ores.
        int Value;

        // Rock-like: dig tools apply their hard-material cost.
        bool Hard;

        // Nothing removes it (the world's floor).
        bool Indestructible;

        // 0..255 opacity when drawn (liquids are see-through).
        std::uint8_t Alpha;
    };

    inline constexpr MaterialInfo MaterialTable[] =
    {
        //  Name         R    G    B   Solid  Behavior                   Dens Strength  Value Hard   Indestr. Alpha
        { "Air",         0,   0,   0, false, MaterialBehavior::Gas,      0,   0.0f,    0, false, false,   0 },
        { "Grass",      86, 140,  60, true,  MaterialBehavior::Static,   0,   1.0f,    0, false, false, 255 },
        { "Dirt",      116,  84,  48, true,  MaterialBehavior::Static,   0,   1.5f,    0, false, false, 255 },
        { "Stone",     100, 100, 108, true,  MaterialBehavior::Static,   0,   8.0f,    0, true,  false, 255 },
        { "Gold",      218, 176,  56, true,  MaterialBehavior::Static,   0,   5.0f,    1, true,  false, 255 },
        { "Sand",      206, 178, 120, true,  MaterialBehavior::Powder,   9,   0.8f,    0, false, false, 255 },
        { "Gravel",    150, 136, 118, true,  MaterialBehavior::Powder,  10,   2.5f,    0, false, false, 255 },
        { "Water",      52, 112, 196, false, MaterialBehavior::Liquid,   4,   0.0f,    0, false, false, 170 },
        { "Lava",      255, 104,  28, false, MaterialBehavior::Liquid,   7,   0.0f,    0, false, false, 245 },
        { "Obsidian",   46,  36,  58, true,  MaterialBehavior::Static,   0,  12.0f,    0, true,  false, 255 },
        { "Bedrock",    40,  38,  44, true,  MaterialBehavior::Static,   0, 1.0e9f,    0, true,  true,  255 },
    };

    static_assert(
        sizeof(MaterialTable) / sizeof(MaterialTable[0]) ==
        static_cast<std::size_t>(Material::Count),
        "MaterialTable must have one entry per Material");

    inline const MaterialInfo& GetMaterialInfo(Material material)
    {
        return MaterialTable[static_cast<std::size_t>(material)];
    }

    inline bool IsFluid(Material material)
    {
        return GetMaterialInfo(material).Behavior == MaterialBehavior::Liquid;
    }
}
