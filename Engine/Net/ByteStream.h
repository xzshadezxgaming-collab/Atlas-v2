#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Atlas
{
    // Little-endian binary writer for network messages. Floats are
    // written as their exact bit patterns: lockstep peers must apply
    // bit-identical inputs.
    class ByteWriter
    {
    public:
        void U8(std::uint8_t v) { m_Data.push_back(v); }
        void I8(std::int8_t v) { U8(static_cast<std::uint8_t>(v)); }

        void U16(std::uint16_t v)
        {
            U8(static_cast<std::uint8_t>(v));
            U8(static_cast<std::uint8_t>(v >> 8));
        }

        void U32(std::uint32_t v)
        {
            U16(static_cast<std::uint16_t>(v));
            U16(static_cast<std::uint16_t>(v >> 16));
        }

        void U64(std::uint64_t v)
        {
            U32(static_cast<std::uint32_t>(v));
            U32(static_cast<std::uint32_t>(v >> 32));
        }

        void F32(float v)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            U32(bits);
        }

        void String(const std::string& text)
        {
            U32(static_cast<std::uint32_t>(text.size()));
            m_Data.insert(m_Data.end(), text.begin(), text.end());
        }

        const std::vector<std::uint8_t>& Data() const { return m_Data; }
        std::vector<std::uint8_t>& Data() { return m_Data; }

    private:
        std::vector<std::uint8_t> m_Data;
    };

    // Reader counterpart. Reading past the end yields zeros and clears
    // Ok(), so a truncated or hostile packet can't read out of bounds -
    // callers check Ok() once after decoding.
    class ByteReader
    {
    public:
        ByteReader(const std::uint8_t* data, std::size_t size)
            : m_Data(data), m_Size(size)
        {
        }

        std::uint8_t U8()
        {
            if (m_Pos >= m_Size)
            {
                m_Ok = false;
                return 0;
            }

            return m_Data[m_Pos++];
        }

        std::int8_t I8() { return static_cast<std::int8_t>(U8()); }

        std::uint16_t U16()
        {
            const std::uint16_t lo = U8();
            const std::uint16_t hi = U8();
            return static_cast<std::uint16_t>(lo | (hi << 8));
        }

        std::uint32_t U32()
        {
            const std::uint32_t lo = U16();
            const std::uint32_t hi = U16();
            return lo | (hi << 16);
        }

        std::uint64_t U64()
        {
            const std::uint64_t lo = U32();
            const std::uint64_t hi = U32();
            return lo | (hi << 32);
        }

        float F32()
        {
            const std::uint32_t bits = U32();
            float v = 0.0f;
            std::memcpy(&v, &bits, sizeof(v));
            return v;
        }

        std::string String()
        {
            const std::uint32_t size = U32();

            if (size > m_Size - m_Pos)
            {
                m_Ok = false;
                m_Pos = m_Size;
                return std::string();
            }

            std::string text(
                reinterpret_cast<const char*>(m_Data + m_Pos), size);
            m_Pos += size;
            return text;
        }

        bool Ok() const { return m_Ok; }
        bool AtEnd() const { return m_Pos >= m_Size; }

    private:
        const std::uint8_t* m_Data;
        std::size_t m_Size;
        std::size_t m_Pos = 0;
        bool m_Ok = true;
    };
}
