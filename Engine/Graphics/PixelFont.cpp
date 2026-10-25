#include "PixelFont.h"

#include "../Core/Window.h"

#include <cctype>
#include <cmath>
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
            case '+': return Pack(0b000, 0b010, 0b111, 0b010, 0b000);
            case '.': return Pack(0b000, 0b000, 0b000, 0b000, 0b010);
            case '%': return Pack(0b101, 0b001, 0b010, 0b100, 0b101);
            case '#': return Pack(0b101, 0b111, 0b101, 0b111, 0b101);
            case '<': return Pack(0b001, 0b010, 0b100, 0b010, 0b001);
            case '>': return Pack(0b100, 0b010, 0b001, 0b010, 0b100);
            case '_': return Pack(0b000, 0b000, 0b000, 0b000, 0b111);
            case ',': return Pack(0b000, 0b000, 0b000, 0b010, 0b100);
            case '!': return Pack(0b010, 0b010, 0b010, 0b000, 0b010);
            case '?': return Pack(0b110, 0b001, 0b010, 0b000, 0b010);
            case '(': return Pack(0b010, 0b100, 0b100, 0b100, 0b010);
            case ')': return Pack(0b010, 0b001, 0b001, 0b001, 0b010);
            case '=': return Pack(0b000, 0b111, 0b000, 0b111, 0b000);
            case '\'': return Pack(0b010, 0b010, 0b000, 0b000, 0b000);
            case '[': return Pack(0b110, 0b100, 0b100, 0b100, 0b110);
            case ']': return Pack(0b011, 0b001, 0b001, 0b001, 0b011);

            default: return 0;
            }
        }

        // The small face: 5x7 glyphs, one row per byte (bit 4 = leftmost
        // column). Used for small text, where the 3x5 face at a
        // fractional scale smeared into unreadable blobs.
        struct SmallGlyph
        {
            std::uint8_t Rows[7];
        };

        SmallGlyph SmallGlyphFor(char c)
        {
            switch (std::toupper(static_cast<unsigned char>(c)))
            {
            case 'A': return { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
            case 'B': return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 };
            case 'C': return { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 };
            case 'D': return { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 };
            case 'E': return { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 };
            case 'F': return { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 };
            case 'G': return { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111 };
            case 'H': return { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
            case 'I': return { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
            case 'J': return { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 };
            case 'K': return { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 };
            case 'L': return { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 };
            case 'M': return { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 };
            case 'N': return { 0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001 };
            case 'O': return { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
            case 'P': return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 };
            case 'Q': return { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 };
            case 'R': return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 };
            case 'S': return { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 };
            case 'T': return { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 };
            case 'U': return { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
            case 'V': return { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 };
            case 'W': return { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 };
            case 'X': return { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 };
            case 'Y': return { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 };
            case 'Z': return { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 };
            case '0': return { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 };
            case '1': return { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
            case '2': return { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 };
            case '3': return { 0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110 };
            case '4': return { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 };
            case '5': return { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 };
            case '6': return { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 };
            case '7': return { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 };
            case '8': return { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 };
            case '9': return { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 };
            case ':': return { 0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000 };
            case '/': return { 0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000 };
            case '-': return { 0b00000, 0b00000, 0b00000, 0b01110, 0b00000, 0b00000, 0b00000 };
            case '+': return { 0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000 };
            case '.': return { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100 };
            case '%': return { 0b11001, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b10011 };
            case '#': return { 0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010 };
            case '<': return { 0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010 };
            case '>': return { 0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000 };
            case '_': return { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111 };
            case ',': return { 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b00100, 0b01000 };
            case '!': return { 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100 };
            case '?': return { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100 };
            case '(': return { 0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010 };
            case ')': return { 0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000 };
            case '=': return { 0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000 };
            case '\'': return { 0b00100, 0b00100, 0b01000, 0b00000, 0b00000, 0b00000, 0b00000 };
            case '[': return { 0b01110, 0b01000, 0b01000, 0b01000, 0b01000, 0b01000, 0b01110 };
            case ']': return { 0b01110, 0b00010, 0b00010, 0b00010, 0b00010, 0b00010, 0b01110 };
            default: return { { 0, 0, 0, 0, 0, 0, 0 } };
            }
        }

        // Below this scale text uses the 5x7 face at one screen pixel per
        // font pixel. Its 6px advance matches the 3x5 face at scale 1.5,
        // so layouts measured either way line up.
        constexpr float SmallFaceBelow = 2.0f;
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

        if (scale < SmallFaceBelow)
        {
            // Whole pixels so every stroke stays one crisp pixel wide,
            // with runs in a row merged into one rect.
            penX = std::round(penX);
            const float top = std::round(y);

            for (char c : text)
            {
                const SmallGlyph glyph = SmallGlyphFor(c);

                for (int row = 0; row < 7; row++)
                {
                    const unsigned bits = glyph.Rows[row];
                    int col = 0;

                    while (col < 5)
                    {
                        if (!(bits & (0b10000u >> col)))
                        {
                            col++;
                            continue;
                        }

                        int end = col;

                        while (end < 5 && (bits & (0b10000u >> end)))
                            end++;

                        window.DrawScreenRect(
                            penX + static_cast<float>(col),
                            top + static_cast<float>(row),
                            static_cast<float>(end - col), 1.0f,
                            r, g, b, a);
                        col = end;
                    }
                }

                penX += 6.0f;
            }

            return;
        }

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
        if (scale < SmallFaceBelow)
            return static_cast<float>(text.size()) * 6.0f;

        return static_cast<float>(text.size()) * 4.0f * scale;
    }
}
