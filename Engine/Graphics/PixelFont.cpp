#include "PixelFont.h"

#include "../Core/Window.h"

#include <cctype>
#include <cstdint>

namespace Atlas
{
    namespace
    {
        // Each glyph: 5 rows x 3 columns, packed top-to-bottom into 15
        // bits (row * 3 + column, column 0 = leftmost = highest bit of
        // its row group).
        constexpr std::uint16_t Pack(
            unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4)
        {
            return static_cast<std::uint16_t>(
                (r0 << 12) | (r1 << 9) | (r2 << 6) | (r3 << 3) | r4);
        }

        std::uint16_t GlyphFor(char c)
        {
            switch (std::toupper(static_cast<unsigned char>(c)))
            {
            case 'A': return Pack(0b010, 0b101, 0b111, 0b101, 0b101);
            case 'B': return Pack(0b110, 0b101, 0b110, 0b101, 0b110);
            case 'C': return Pack(0b011, 0b100, 0b100, 0b100, 0b011);
            case 'D': return Pack(0b110, 0b101, 0b101, 0b101, 0b110);
            case 'E': return Pack(0b111, 0b100, 0b110, 0b100, 0b111);
            case 'F': return Pack(0b111, 0b100, 0b110, 0b100, 0b100);
            case 'G': return Pack(0b011, 0b100, 0b101, 0b101, 0b011);
            case 'H': return Pack(0b101, 0b101, 0b111, 0b101, 0b101);
            case 'I': return Pack(0b111, 0b010, 0b010, 0b010, 0b111);
            case 'J': return Pack(0b001, 0b001, 0b001, 0b101, 0b010);
            case 'K': return Pack(0b101, 0b101, 0b110, 0b101, 0b101);
            case 'L': return Pack(0b100, 0b100, 0b100, 0b100, 0b111);
            case 'M': return Pack(0b101, 0b111, 0b111, 0b101, 0b101);
            case 'N': return Pack(0b110, 0b101, 0b101, 0b101, 0b101);
            case 'O': return Pack(0b010, 0b101, 0b101, 0b101, 0b010);
            case 'P': return Pack(0b110, 0b101, 0b110, 0b100, 0b100);
            case 'Q': return Pack(0b010, 0b101, 0b101, 0b010, 0b001);
            case 'R': return Pack(0b110, 0b101, 0b110, 0b110, 0b101);
            case 'S': return Pack(0b011, 0b100, 0b010, 0b001, 0b110);
            case 'T': return Pack(0b111, 0b010, 0b010, 0b010, 0b010);
            case 'U': return Pack(0b101, 0b101, 0b101, 0b101, 0b111);
            case 'V': return Pack(0b101, 0b101, 0b101, 0b101, 0b010);
            case 'W': return Pack(0b101, 0b101, 0b111, 0b111, 0b101);
            case 'X': return Pack(0b101, 0b101, 0b010, 0b101, 0b101);
            case 'Y': return Pack(0b101, 0b101, 0b010, 0b010, 0b010);
            case 'Z': return Pack(0b111, 0b001, 0b010, 0b100, 0b111);

            case '0': return Pack(0b111, 0b101, 0b101, 0b101, 0b111);
            case '1': return Pack(0b010, 0b110, 0b010, 0b010, 0b111);
            case '2': return Pack(0b111, 0b001, 0b111, 0b100, 0b111);
            case '3': return Pack(0b111, 0b001, 0b111, 0b001, 0b111);
            case '4': return Pack(0b101, 0b101, 0b111, 0b001, 0b001);
            case '5': return Pack(0b111, 0b100, 0b111, 0b001, 0b111);
            case '6': return Pack(0b111, 0b100, 0b111, 0b101, 0b111);
            case '7': return Pack(0b111, 0b001, 0b001, 0b010, 0b010);
            case '8': return Pack(0b111, 0b101, 0b111, 0b101, 0b111);
            case '9': return Pack(0b111, 0b101, 0b111, 0b001, 0b111);

            case ':': return Pack(0b000, 0b010, 0b000, 0b010, 0b000);
            case '/': return Pack(0b001, 0b001, 0b010, 0b100, 0b100);
            case '-': return Pack(0b000, 0b000, 0b111, 0b000, 0b000);
            case '.': return Pack(0b000, 0b000, 0b000, 0b000, 0b010);
            case '%': return Pack(0b101, 0b001, 0b010, 0b100, 0b101);

            default: return 0;
            }
        }
    }

    void PixelFont::Draw(
        Window& window,
        float x,
        float y,
        float scale,
        const std::string& text,
        Uint8 r,
        Uint8 g,
        Uint8 b,
        Uint8 a)
    {
        float penX = x;

        for (char c : text)
        {
            const std::uint16_t glyph = GlyphFor(c);

            if (glyph != 0)
            {
                for (int row = 0; row < 5; row++)
                {
                    const unsigned bits =
                        (glyph >> ((4 - row) * 3)) & 0b111u;

                    for (int col = 0; col < 3; col++)
                    {
                        if (bits & (0b100u >> col))
                        {
                            window.DrawScreenRect(
                                penX + static_cast<float>(col) * scale,
                                y + static_cast<float>(row) * scale,
                                scale,
                                scale,
                                r, g, b, a);
                        }
                    }
                }
            }

            penX += 4.0f * scale;
        }
    }

    float PixelFont::Measure(const std::string& text, float scale)
    {
        return static_cast<float>(text.size()) * 4.0f * scale;
    }
}
