#pragma once

namespace Radis
{
    // Little-endian binary writer/reader with automatic endian swap
    struct BinaryEndian
    {
        static constexpr bool NeedsSwap = (std::endian::native != std::endian::little);

        static constexpr uint32_t Swap32(uint32_t v)
        {
            return ((v >> 24) & 0x000000FFu) |
                ((v >> 8) & 0x0000FF00u) |
                ((v << 8) & 0x00FF0000u) |
                ((v << 24) & 0xFF000000u);
        }

        static constexpr uint64_t Swap64(uint64_t v)
        {
            return ((v >> 56) & 0x00000000000000FFull) |
                ((v >> 40) & 0x000000000000FF00ull) |
                ((v >> 24) & 0x0000000000FF0000ull) |
                ((v >> 8) & 0x00000000FF000000ull) |
                ((v << 8) & 0x000000FF00000000ull) |
                ((v << 24) & 0x0000FF0000000000ull) |
                ((v << 40) & 0x00FF000000000000ull) |
                ((v << 56) & 0xFF00000000000000ull);
        }

        static float SwapFloat(float v)
        {
            uint32_t tmp;
            static_assert(sizeof(tmp) == sizeof(v));
            std::memcpy(&tmp, &v, sizeof(v));
            tmp = Swap32(tmp);
            std::memcpy(&v, &tmp, sizeof(v));
            return v;
        }
    };

    class BinaryWriterLE
    {
    public:
        explicit BinaryWriterLE(std::ostream& os) : m_os(os) {}

        void U32(uint32_t v)
        {
            if constexpr (BinaryEndian::NeedsSwap)
            {
                v = BinaryEndian::Swap32(v);
            }
            m_os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        }

        void I32(int32_t v)
        {
            if constexpr (BinaryEndian::NeedsSwap)
            {
                auto u = BinaryEndian::Swap32(static_cast<uint32_t>(v));
                m_os.write(reinterpret_cast<const char*>(&u), sizeof(u));
            }
            else
            {
                m_os.write(reinterpret_cast<const char*>(&v), sizeof(v));
            }
        }

        void U64(uint64_t v)
        {
            if constexpr (BinaryEndian::NeedsSwap)
            {
                v = BinaryEndian::Swap64(v);
            }
            m_os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        }

        void F32(float v)
        {
            if constexpr (BinaryEndian::NeedsSwap)
            {
                v = BinaryEndian::SwapFloat(v);
            }
            m_os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        }

        void Vec2(const glm::vec2& v)
        {
            glm::vec2 tmp = v;
            if constexpr (BinaryEndian::NeedsSwap)
            {
                tmp.x = BinaryEndian::SwapFloat(tmp.x);
                tmp.y = BinaryEndian::SwapFloat(tmp.y);
            }
            m_os.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        }

        void Vec3(const glm::vec3& v)
        {
            glm::vec3 tmp = v;
            if constexpr (BinaryEndian::NeedsSwap)
            {
                tmp.x = BinaryEndian::SwapFloat(tmp.x);
                tmp.y = BinaryEndian::SwapFloat(tmp.y);
                tmp.z = BinaryEndian::SwapFloat(tmp.z);
            }
            m_os.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        }

        void Mat4(const glm::mat4& m)
        {
            glm::mat4 tmp = m;
            if constexpr (BinaryEndian::NeedsSwap)
            {
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j)
                        tmp[i][j] = BinaryEndian::SwapFloat(tmp[i][j]);
            }
            m_os.write(reinterpret_cast<const char*>(&tmp), sizeof(tmp));
        }

        // Writes: uint32 length + bytes (no null terminator)
        void String(const std::string& s)
        {
            U32(static_cast<uint32_t>(s.size()));
            if (!s.empty())
                m_os.write(s.data(), s.size());
        }

        template<typename T>
        void PODArray(const T* data, size_t count)
        {
            static_assert(std::is_trivially_copyable_v<T>);

            // NOTE: this assumes T itself is already in little-endian form.
            m_os.write(reinterpret_cast<const char*>(data), sizeof(T) * count);
        }

        bool Good() const { return !!m_os; }

    private:
        std::ostream& m_os;
    };

    class BinaryReaderLE
    {
    public:
        explicit BinaryReaderLE(std::istream& is) : m_is(is) {}

        uint32_t U32()
        {
            uint32_t v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
                v = BinaryEndian::Swap32(v);
            return v;
        }

        int32_t I32()
        {
            uint32_t v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
                v = BinaryEndian::Swap32(v);
            return static_cast<int32_t>(v);
        }

        uint64_t U64()
        {
            uint64_t v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
                v = BinaryEndian::Swap64(v);
            return v;
        }

        float F32()
        {
            float v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
                v = BinaryEndian::SwapFloat(v);
            return v;
        }

        glm::vec2 Vec2()
        {
            glm::vec2 v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
            {
                v.x = BinaryEndian::SwapFloat(v.x);
                v.y = BinaryEndian::SwapFloat(v.y);
            }
            return v;
        }

        glm::vec3 Vec3()
        {
            glm::vec3 v;
            m_is.read(reinterpret_cast<char*>(&v), sizeof(v));
            if constexpr (BinaryEndian::NeedsSwap)
            {
                v.x = BinaryEndian::SwapFloat(v.x);
                v.y = BinaryEndian::SwapFloat(v.y);
                v.z = BinaryEndian::SwapFloat(v.z);
            }
            return v;
        }

        glm::mat4 Mat4()
        {
            glm::mat4 m;
            m_is.read(reinterpret_cast<char*>(&m), sizeof(m));
            if constexpr (BinaryEndian::NeedsSwap)
            {
                for (int i = 0; i < 4; ++i)
                    for (int j = 0; j < 4; ++j)
                        m[i][j] = BinaryEndian::SwapFloat(m[i][j]);
            }
            return m;
        }

        std::string String()
        {
            uint32_t len = U32();
            std::string s(len, '\0');
            if (len > 0)
                m_is.read(s.data(), len);
            return s;
        }

        template<typename T>
        void PODArray(T* out, size_t count)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            m_is.read(reinterpret_cast<char*>(out), sizeof(T) * count);
            // NOTE: if T has multi-byte fields that need endian swap,
            // you still need to handle that per-field afterwards.
        }

        bool Good() const { return !!m_is; }

    private:
        std::istream& m_is;
    };
}
