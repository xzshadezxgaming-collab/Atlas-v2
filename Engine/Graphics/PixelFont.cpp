#include "PixelFont.h"

#include "../Core/Window.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>

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

    namespace
    {
    void DrawPixelFace(
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

    float MeasurePixelFace(const std::string& text, float scale)
    {
        if (scale < SmallFaceBelow)
            return static_cast<float>(text.size()) * 6.0f;

        return static_cast<float>(text.size()) * 4.0f * scale;
    }
    }

    // --- TrueType faces ---

    namespace
    {
        // Glyphs are rasterized on first use into shared atlas pages,
        // one entry per (face, size in real pixels, character).
        struct Glyph
        {
            int Page = -1;
            int X = 0, Y = 0, W = 0, H = 0;
            int OffsetX = 0, OffsetY = 0;
        };

        struct Face
        {
            std::vector<unsigned char> Data;
            stbtt_fontinfo Info{};
            float CapUnits = 1.0f; // height of 'H' in font units
        };

        constexpr int PageSize = 1024;

        struct TextState
        {
            SDL_Renderer* Renderer = nullptr;
            Face Faces[2];
            bool Loaded = false;

            std::vector<SDL_Texture*> Pages;
            int ShelfX = 0, ShelfY = 0, ShelfH = 0;
            std::unordered_map<std::uint64_t, Glyph> Glyphs;
            std::vector<SDL_Vertex> Vertices;
        };

        TextState& State()
        {
            static TextState state;
            return state;
        }

        bool ReadFile(const std::string& path, std::vector<unsigned char>& out)
        {
            std::size_t size = 0;
            void* data = SDL_LoadFile(path.c_str(), &size);

            if (!data)
                return false;

            out.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
            SDL_free(data);
            return true;
        }

        bool LoadFace(Face& face, const std::string& path)
        {
            if (!ReadFile(path, face.Data))
                return false;

            const int offset = stbtt_GetFontOffsetForIndex(face.Data.data(), 0);

            if (offset < 0 || !stbtt_InitFont(&face.Info, face.Data.data(), offset))
                return false;

            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;

            if (!stbtt_GetCodepointBox(&face.Info, 'H', &x0, &y0, &x1, &y1) || y1 <= 0)
                return false;

            face.CapUnits = static_cast<float>(y1);
            return true;
        }

        // Old pixel-font scale -> capital height in view pixels.
        float CapHeightFor(float scale)
        {
            return scale < SmallFaceBelow ? 7.5f : 5.0f * scale;
        }

        int FaceFor(float scale)
        {
            return scale < SmallFaceBelow ? 0 : 1;
        }

        // Uppercase UI text reads better slightly open.
        float TrackingFor(float capHeight)
        {
            return capHeight * 0.06f;
        }

        void ResetAtlas(TextState& state)
        {
            for (SDL_Texture* page : state.Pages)
                SDL_DestroyTexture(page);

            state.Pages.clear();
            state.Glyphs.clear();
            state.ShelfX = state.ShelfY = state.ShelfH = 0;
        }

        bool AddPage(TextState& state)
        {
            SDL_Texture* page = SDL_CreateTexture(state.Renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STATIC, PageSize, PageSize);

            if (!page)
                return false;

            std::vector<Uint32> clear(static_cast<std::size_t>(PageSize) * PageSize, 0x00FFFFFFu);
            SDL_UpdateTexture(page, nullptr, clear.data(), PageSize * 4);
            SDL_SetTextureBlendMode(page, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(page, SDL_SCALEMODE_LINEAR);

            state.Pages.push_back(page);
            state.ShelfX = state.ShelfY = state.ShelfH = 0;
            return true;
        }

        const Glyph* GetGlyph(TextState& state, int faceIndex, int capPixels, int codepoint)
        {
            const std::uint64_t key =
                (static_cast<std::uint64_t>(faceIndex) << 48) |
                (static_cast<std::uint64_t>(capPixels) << 24) |
                static_cast<std::uint64_t>(codepoint & 0xFFFFFF);

            if (auto it = state.Glyphs.find(key); it != state.Glyphs.end())
                return &it->second;

            const Face& face = state.Faces[faceIndex];
            const float fontScale = static_cast<float>(capPixels) / face.CapUnits;

            Glyph glyph;
            int w = 0, h = 0;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(
                &face.Info, fontScale, fontScale, codepoint, &w, &h, &glyph.OffsetX, &glyph.OffsetY);

            if (bitmap && w > 0 && h > 0 && w < PageSize - 2 && h < PageSize - 2)
            {
                // Shelf packing with a one-texel gutter (bilinear filtering
                // would otherwise bleed neighbours in).
                if (state.Pages.empty() || state.ShelfX + w + 1 > PageSize)
                {
                    if (!state.Pages.empty())
                    {
                        state.ShelfX = 0;
                        state.ShelfY += state.ShelfH + 1;
                        state.ShelfH = 0;
                    }
                }

                if (state.Pages.empty() || state.ShelfY + h + 1 > PageSize)
                {
                    // A handful of sizes fit on one page; if a session
                    // somehow fills several, start over rather than grow.
                    if (state.Pages.size() >= 4)
                        ResetAtlas(state);

                    if (!AddPage(state))
                    {
                        stbtt_FreeBitmap(bitmap, nullptr);
                        return nullptr;
                    }
                }

                std::vector<Uint32> pixels(static_cast<std::size_t>(w) * h);

                for (int i = 0; i < w * h; i++)
                    pixels[static_cast<std::size_t>(i)] = (static_cast<Uint32>(bitmap[i]) << 24) | 0x00FFFFFFu;

                const SDL_Rect rect = { state.ShelfX + 1, state.ShelfY + 1, w, h };
                SDL_UpdateTexture(state.Pages.back(), &rect, pixels.data(), w * 4);

                glyph.Page = static_cast<int>(state.Pages.size()) - 1;
                glyph.X = rect.x;
                glyph.Y = rect.y;
                glyph.W = w;
                glyph.H = h;

                state.ShelfX += w + 1;
                state.ShelfH = std::max(state.ShelfH, h + 1);
            }

            if (bitmap)
                stbtt_FreeBitmap(bitmap, nullptr);

            return &state.Glyphs.emplace(key, glyph).first->second;
        }

        // Pen advance (with kerning) between two characters, in view
        // pixels at the given font scale.
        float Advance(const Face& face, float fontScale, int c, int next)
        {
            int advance = 0, bearing = 0;
            stbtt_GetCodepointHMetrics(&face.Info, c, &advance, &bearing);

            float result = static_cast<float>(advance) * fontScale;

            if (next)
                result += static_cast<float>(stbtt_GetCodepointKernAdvance(&face.Info, c, next)) * fontScale;

            return result;
        }

        float MeasureTrueType(const std::string& text, float scale)
        {
            TextState& state = State();
            const Face& face = state.Faces[FaceFor(scale)];
            const float cap = CapHeightFor(scale);
            const float fontScale = cap / face.CapUnits;
            const float tracking = TrackingFor(cap);

            float width = 0.0f;

            for (std::size_t i = 0; i < text.size(); i++)
            {
                const int c = static_cast<unsigned char>(text[i]);
                const int next = i + 1 < text.size() ? static_cast<unsigned char>(text[i + 1]) : 0;
                width += Advance(face, fontScale, c, next) + (next ? tracking : 0.0f);
            }

            return width;
        }

        void DrawTrueType(Window& window, float x, float y, float scale,
            const std::string& text, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
        {
            TextState& state = State();
            const int faceIndex = FaceFor(scale);
            const Face& face = state.Faces[faceIndex];
            const float cap = CapHeightFor(scale);
            const float fontScale = cap / face.CapUnits;
            const float tracking = TrackingFor(cap);

            // Rasterize at the real output size, then draw at 1/pixelScale
            // so each glyph texel lands on one screen pixel.
            const float pixelScale = window.GetPixelScale();
            const int capPixels = std::max(4, static_cast<int>(std::lround(cap * pixelScale)));
            const float texel = cap / static_cast<float>(capPixels);

            auto snap = [&](float v) { return std::round(v / texel) * texel; };

            const float baseline = snap(y + cap);
            const SDL_FColor color = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };

            int page = -1;
            std::vector<SDL_Vertex>& vertices = state.Vertices;
            vertices.clear();

            auto flush = [&]()
            {
                if (page >= 0 && !vertices.empty())
                {
                    SDL_RenderGeometry(window.GetRenderer(), state.Pages[static_cast<std::size_t>(page)],
                        vertices.data(), static_cast<int>(vertices.size()), nullptr, 0);
                }

                vertices.clear();
            };

            float penX = x;

            for (std::size_t i = 0; i < text.size(); i++)
            {
                const int c = static_cast<unsigned char>(text[i]);
                const int next = i + 1 < text.size() ? static_cast<unsigned char>(text[i + 1]) : 0;

                const Glyph* glyph = GetGlyph(state, faceIndex, capPixels, c);

                if (glyph && glyph->Page >= 0)
                {
                    if (glyph->Page != page)
                    {
                        flush();
                        page = glyph->Page;
                    }

                    const float gx = snap(penX) + static_cast<float>(glyph->OffsetX) * texel;
                    const float gy = baseline + static_cast<float>(glyph->OffsetY) * texel;
                    const float gw = static_cast<float>(glyph->W) * texel;
                    const float gh = static_cast<float>(glyph->H) * texel;

                    const float u0 = static_cast<float>(glyph->X) / PageSize;
                    const float v0 = static_cast<float>(glyph->Y) / PageSize;
                    const float u1 = static_cast<float>(glyph->X + glyph->W) / PageSize;
                    const float v1 = static_cast<float>(glyph->Y + glyph->H) / PageSize;

                    const SDL_Vertex tl = { { gx, gy }, color, { u0, v0 } };
                    const SDL_Vertex tr = { { gx + gw, gy }, color, { u1, v0 } };
                    const SDL_Vertex br = { { gx + gw, gy + gh }, color, { u1, v1 } };
                    const SDL_Vertex bl = { { gx, gy + gh }, color, { u0, v1 } };

                    vertices.insert(vertices.end(), { tl, tr, br, tl, br, bl });
                }

                penX += Advance(face, fontScale, c, next) + (next ? tracking : 0.0f);
            }

            flush();
        }
    }

    bool PixelFont::LoadFaces(SDL_Renderer* renderer,
        const std::string& regularPath, const std::string& boldPath)
    {
        TextState& state = State();
        ResetAtlas(state);
        state.Loaded = false;
        state.Renderer = renderer;

        if (!renderer ||
            !LoadFace(state.Faces[0], regularPath) ||
            !LoadFace(state.Faces[1], boldPath))
            return false;

        state.Loaded = true;
        return true;
    }

    void PixelFont::Shutdown()
    {
        TextState& state = State();
        ResetAtlas(state);
        state.Loaded = false;
        state.Renderer = nullptr;
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
        if (State().Loaded && window.GetRenderer() == State().Renderer)
            DrawTrueType(window, x, y, scale, text, r, g, b, a);
        else
            DrawPixelFace(window, x, y, scale, text, r, g, b, a);
    }

    float PixelFont::Measure(const std::string& text, float scale)
    {
        if (State().Loaded)
            return MeasureTrueType(text, scale);

        return MeasurePixelFace(text, scale);
    }
}
