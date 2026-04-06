/*****************************************************************//**
 * \file   BinaryIO.h
 * \brief  Little-endian binary writer/reader
 *
 * Targets little-endian platforms only, which covers every platform Vulkan runs on
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

static_assert(std::endian::native == std::endian::little,
    "BinaryIO assumes a little-endian platform. Endian swapping is not implemented.");

namespace Radis
{
    class BinaryWriterLE
    {
    public:
        explicit BinaryWriterLE(std::ostream& os) : m_os(os) {}

        void U32(uint32_t v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void I32(int32_t v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void U64(uint64_t v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void F32(float v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

        void Vec2(const glm::vec2& v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void Vec3(const glm::vec3& v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void Vec4(const glm::vec4& v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
        void Mat4(const glm::mat4& v) { m_os.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

        // Writes: uint32 length + bytes (no null terminator)
        void String(const std::string& s)
        {
            U32(static_cast<uint32_t>(s.size()));
            if (!s.empty())
                m_os.write(s.data(), static_cast<std::streamsize>(s.size()));
        }

        template<typename T>
        void PODArray(const T* data, size_t count)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            m_os.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
        }

        bool Good() const { return !!m_os; }

    private:
        std::ostream& m_os;
    };

    // Reads little-endian binary data from either an std::istream or a raw
    // memory pointer (e.g. an mio memory-mapped file). Construct with whichever
    // source is available; all read methods work identically in both modes.
    class BinaryReaderLE
    {
    public:
        // Stream-based construction (e.g. std::ifstream)
        explicit BinaryReaderLE(std::istream& is)
            : m_is(&is), m_cursor(nullptr), m_end(nullptr) {
        }

        // Pointer-based construction (e.g. mio::mmap_source).
        // data must remain valid for the lifetime of this reader.
        explicit BinaryReaderLE(const char* data, size_t size)
            : m_is(nullptr), m_cursor(data), m_end(data + size) {
        }

        uint32_t U32() { uint32_t v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        int32_t  I32() { int32_t  v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        uint64_t U64() { uint64_t v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        float    F32() { float    v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }

        glm::vec2 Vec2() { glm::vec2 v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        glm::vec3 Vec3() { glm::vec3 v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        glm::vec4 Vec4() { glm::vec4 v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
        glm::mat4 Mat4() { glm::mat4 v; ReadBytes(reinterpret_cast<char*>(&v), sizeof(v)); return v; }

        std::string String()
        {
            uint32_t len = U32();
            std::string s(len, '\0');
            if (len > 0)
                ReadBytes(s.data(), len);
            return s;
        }

        template<typename T>
        void PODArray(T* out, size_t count)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            ReadBytes(reinterpret_cast<char*>(out), sizeof(T) * count);
        }

        bool Good() const
        {
            if (m_cursor)
                return m_cursor <= m_end;
            return !!(*m_is);
        }

    private:
        void ReadBytes(char* out, size_t n)
        {
            if (m_cursor)
            {
                std::memcpy(out, m_cursor, n);
                m_cursor += n;
            }
            else
            {
                m_is->read(out, static_cast<std::streamsize>(n));
            }
        }

        std::istream* m_is;      // stream mode
        const char* m_cursor;  // pointer mode — current position
        const char* m_end;     // pointer mode — one past last byte
    };

    // Allocator that skips default construction for trivially copyable types.
// Use with ResizeUninitialized() to avoid zero-init before bulk overwrite.
    template<typename T>
    struct NoInitAllocator : std::allocator<T>
    {
        using Base = std::allocator<T>;
        using Base::Base;

        template<typename U>
        struct rebind { using other = NoInitAllocator<U>; };

        // Skip default construction — caller must overwrite before reading
        void construct(T* p) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>,
                "NoInitAllocator only valid for trivially copyable types");
        }

        // Forward any other construction normally (e.g. copy/move)
        template<typename... Args>
        void construct(T* p, Args&&... args)
        {
            ::new(p) T(std::forward<Args>(args)...);
        }
    };

    // Resizes a vector without constructing new elements.
    // Safe only when all new elements will be overwritten before being read.
    template<typename T>
    void ResizeUninitialized(std::vector<T>& vec, size_t n)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        // Reinterpret as a vector with the no-init allocator
        using NoInitVec = std::vector<T, NoInitAllocator<T>>;
        auto& noInitVec = reinterpret_cast<NoInitVec&>(vec);
        noInitVec.resize(n); // skips construction of new elements
    }
}