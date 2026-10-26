#pragma once

#include "Material.h"

#include <cstdint>

namespace Atlas
{
    // The kind of world a map is generated as. Changes the terrain's
    // layers, caves and liquids, and the sky drawn behind it.
    enum class Biome : std::uint8_t
    {
        Temperate, // grass over dirt and stone, water-filled caves
        Desert,    // deep dunes over sandstone mesas, dry caves
        Arctic,    // snowfields, ice sheets and frozen lakes
        Volcanic,  // ash over basalt, lava everywhere, rich gold

        Count
    };

    inline const char* GetBiomeName(Biome biome)
    {
        switch (biome)
        {
        case Biome::Temperate: return "TEMPERATE";
        case Biome::Desert: return "DESERT";
        case Biome::Arctic: return "ARCTIC";
        case Biome::Volcanic: return "VOLCANIC";
        default: return "?";
        }
    }

    inline const char* GetBiomeBlurb(Biome biome)
    {
        switch (biome)
        {
        case Biome::Temperate: return "ROLLING HILLS, FLOODED CAVES.";
        case Biome::Desert: return "SHIFTING DUNES AND SANDSTONE MESAS. LITTLE WATER.";
        case Biome::Arctic: return "SNOWDRIFTS COLLAPSE. THIN ICE OVER COLD LAKES.";
        case Biome::Volcanic: return "LAVA UNDERFOOT, BUT THE ROCK IS RICH IN GOLD.";
        default: return "";
        }
    }

    // What a base's levelled ground is packed with: Fill packs caves and
    // pools under it so the bunker can't slump, Top is its surface row.
    // Loose is the biome's surface powder, left where it lies so the
    // base keeps the landscape's look (Air = none; on level ground a
    // powder stays put).
    struct BiomeGround
    {
        Material Top;
        Material Fill;
        Material Loose;
    };

    inline BiomeGround GetBiomeGround(Biome biome)
    {
        switch (biome)
        {
        case Biome::Desert: return { Material::Sand, Material::Sandstone, Material::Sand };
        case Biome::Arctic: return { Material::Snow, Material::Dirt, Material::Snow };
        case Biome::Volcanic: return { Material::Ash, Material::Basalt, Material::Ash };
        default: return { Material::Grass, Material::Dirt, Material::Air };
        }
    }

    // A menu choice of biome: 0 picks one from the seed, 1.. picks
    // Biome(choice - 1). Seeded so a map preview and the match agree.
    inline Biome ResolveBiome(int choice, unsigned int seed)
    {
        constexpr int Count = static_cast<int>(Biome::Count);

        if (choice >= 1 && choice <= Count)
            return static_cast<Biome>(choice - 1);

        std::uint32_t h = seed * 2654435761u;
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;

        return static_cast<Biome>(h % static_cast<std::uint32_t>(Count));
    }
}
