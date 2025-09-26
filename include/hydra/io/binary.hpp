#pragma once

#include "io/stream.hpp"
#include "memory.hpp"
#include "detail/noncopyable.hpp"

namespace hy::io {
    class binary : public detail::noncopyable {
        std::shared_ptr<stream> m_stream;

    public:
        /// Constructor taking stream shared pointer.
        /// Reader shares ownership of the stream.
        explicit binary(std::shared_ptr<stream> stream) : m_stream(std::move(stream)) { }

        /// Constructor taking stream reference.
        /// Reader creates shared pointer from stream's self-reference.
        explicit binary(const stream& stream) : m_stream(stream.get_shared()) { }

        /// Destructor for stream_reader.
        /// Automatically releases shared stream reference.
        ~binary() = default;

        /// Move constructor for stream_reader.
        /// Transfers stream ownership.
        binary(binary&& other) noexcept
            : m_stream(std::move(other.m_stream)) { }

        /// Move assignment for stream_reader.
        /// Transfers stream ownership.
        binary& operator=(binary&& other) noexcept {
            if (this != &other) {
                m_stream = std::move(other.m_stream);
            }
            return *this;
        }

        template <mem::detail::PrimitiveObject T, std::size_t Size = sizeof(T)>
        std::size_t read(T& object) {
            return m_stream
                ? m_stream->read(mem::ref(&object), Size)
                : throw error("Binary interface has no associated stream");
        }

        template <mem::detail::PrimitiveObject T, std::size_t Size = sizeof(T)>
        T read_v() {
            std::uint8_t object[Size];
            const auto n_bytes = m_stream->read((mem::addr)&object, Size);

            if (!n_bytes)       throw error("Read failure in binary interface");
            if (n_bytes < Size) throw error("Partial read in binary interface");

            return *(T*)(std::uintptr_t)&object;
        }

        auto read_u8() { return read_v<std::uint8_t>(); }

        auto read_i8() { return read_v<std::int8_t>(); }

        auto read_u16() { return read_v<std::uint16_t>(); }

        auto read_i16() { return read_v<std::int16_t>(); }

        auto read_u32() { return read_v<std::uint32_t>(); }

        auto read_i32() { return read_v<std::int32_t>(); }

        auto read_u64() { return read_v<std::uint64_t>(); }

        auto read_i64() { return read_v<std::int64_t>(); }

        auto read_f32() { return read_v<float>(); }

        auto read_f64() { return read_v<double>(); }

        auto read_ptr() { return mem::addr(read_v<std::uintptr_t>()); }

        template <class T, std::size_t Size = sizeof(T)>
        std::size_t write(const T& object) {
            return m_stream
                ? m_stream->write(mem::ref(&object), Size)
                : throw error("Binary interface has no associated stream");
        }

        auto write_u8(auto value) { return write<std::uint8_t>(value); }

        auto write_i8(auto value) { return write<std::int8_t>(value); }

        auto write_u16(auto value) { return write<std::uint16_t>(value); }

        auto write_i16(auto value) { return write<std::int16_t>(value); }

        auto write_u32(auto value) { return write<std::uint32_t>(value); }

        auto write_i32(auto value) { return write<std::int32_t>(value); }

        auto write_u64(auto value) { return write<std::uint64_t>(value); }

        auto write_i64(auto value) { return write<std::int64_t>(value); }

        auto write_f32(auto value) { return write<float>(value); }

        auto write_f64(auto value) { return write<double>(value); }

        auto write_ptr(auto value) { return write<std::uintptr_t>(value); }

        /// Gets the underlying stream.
        /// Returns shared pointer to associated stream.
        std::shared_ptr<stream> get_stream() const {
            return m_stream;
        }

        /// Checks if reader has a valid stream.
        /// Returns true if stream is available.
        bool is_valid() const {
            return m_stream != nullptr;
        }
    };
}
