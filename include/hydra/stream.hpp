#pragma once

#include <functional>
#include <vector>

#include <hydra/detail.hpp>
#include <hydra/memory.hpp>

namespace hy {
    /// Represents the origin position for a stream operation.
    /// Used for seeking within a stream.
    enum class stream_origin : std::uint8_t {
        begin,
        current,
        end
    };

    /// Represents the mode of offset handling in a stream.
    /// Absolute mode uses fixed offsets, relative mode advances the offset.
    enum class stream_mode : std::uint8_t {
        absolute,
        relative
    };

    /// Base abstract stream class for read/write operations.
    /// Provides fundamental streaming operations similar to .NET Stream.
    class stream {
    protected:
        std::size_t m_offset = 0;
        stream_mode m_mode = stream_mode::relative;

    public:
        /// Default constructor for stream.
        /// Initializes stream in valid state.
        stream() = default;

        /// Virtual destructor for stream.
        /// Ensures proper cleanup of derived classes.
        virtual ~stream() = default;

        /// Gets the current position in the stream.
        /// Returns size_t.
        [[nodiscard]] std::size_t offset() const noexcept { return m_offset; }

        /// Sets the current position in the stream.
        /// Returns size_t.
        void offset(const std::size_t offset) noexcept { m_offset = offset; }

        /// Reads data from stream into buffer without changing offset.
        /// Returns number of bytes actually read.
        virtual std::size_t read(std::int8_t* dst, std::size_t size) const = 0;

        /// Writes data from buffer to stream without changing offset.
        /// Returns number of bytes actually written.
        virtual std::size_t write(std::int8_t* src, std::size_t size) const = 0;

        /// Reads data from stream into buffer and advances offset.
        /// Returns number of bytes actually read.
        std::size_t read(const region& dst);

        /// Writes data from buffer to stream and advances offset.
        /// Returns number of bytes actually written.
        std::size_t write(const region& src);

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        virtual std::size_t seek(std::int64_t offset, stream_origin origin) = 0;

        /// Jump forwards/backwards by a specified number of bytes in stream.
        /// Returns new position after skipping.
        std::size_t jump(const std::int64_t offset) {
            return seek(offset, stream_origin::current);
        }

        /* Binary IO functions */

        template <detail::Primitive T, std::size_t Size = sizeof(T)>
        std::size_t read_obj(T& object) {
            return read(region{(addr)&object, Size});
        }

        template <detail::Primitive T, std::size_t Size = sizeof(T)>
        T read_obj() {
            std::uint8_t object[Size];
            const auto n_bytes = read_obj(object);

            if (!n_bytes)       throw std::runtime_error("Read failure in stream");
            if (n_bytes < Size) throw std::runtime_error("Partial read in stream");

            return *(T*)(std::uintptr_t)&object;
        }

        auto read_u8() { return read_obj<std::uint8_t>(); }

        auto read_i8() { return read_obj<std::int8_t>(); }

        auto read_u16() { return read_obj<std::uint16_t>(); }

        auto read_i16() { return read_obj<std::int16_t>(); }

        auto read_u32() { return read_obj<std::uint32_t>(); }

        auto read_i32() { return read_obj<std::int32_t>(); }

        auto read_u64() { return read_obj<std::uint64_t>(); }

        auto read_i64() { return read_obj<std::int64_t>(); }

        auto read_f32() { return read_obj<float>(); }

        auto read_f64() { return read_obj<double>(); }

        auto read_ptr() { return addr(read_obj<std::uintptr_t>()); }

        template <detail::Primitive Element>
        auto read_arr(std::size_t count) {
            std::vector<Element> vec;
            vec.resize(count);

            const auto n_bytes = read(region{vec.data(), sizeof(Element) * count});
            if (n_bytes < sizeof(Element) * count)
                throw std::runtime_error("Partial array read in stream");

            return vec;
        }

        template <detail::Primitive Element, std::size_t Size>
        std::size_t read_arr(Element (&arr)[Size]) {
            const auto n_bytes = read(region{(addr)arr, sizeof(Element) * Size});
            return n_bytes;
        }

        template <class T, std::size_t Size = sizeof(T)>
        std::size_t write_obj(const T& object) {
            const auto n_bytes = write((addr)&object, Size);
            if (m_mode == stream_mode::relative)
                m_offset += n_bytes;
            return n_bytes;
        }

        auto write_u8(auto value) { return write_obj<std::uint8_t>(value); }

        auto write_i8(auto value) { return write_obj<std::int8_t>(value); }

        auto write_u16(auto value) { return write_obj<std::uint16_t>(value); }

        auto write_i16(auto value) { return write_obj<std::int16_t>(value); }

        auto write_u32(auto value) { return write_obj<std::uint32_t>(value); }

        auto write_i32(auto value) { return write_obj<std::int32_t>(value); }

        auto write_u64(auto value) { return write_obj<std::uint64_t>(value); }

        auto write_i64(auto value) { return write_obj<std::int64_t>(value); }

        auto write_f32(auto value) { return write_obj<float>(value); }

        auto write_f64(auto value) { return write_obj<double>(value); }

        auto write_ptr(auto value) { return write_obj<std::uintptr_t>(value); }

        template <detail::Primitive Element>
        std::size_t write_arr(const std::vector<Element>& vec) {
            const auto n_bytes = write(region{(addr)vec.data(), sizeof(Element) * vec.size()});
            return n_bytes;
        }

        template <detail::Primitive Element, std::size_t Size>
        std::size_t write_arr(const Element (&arr)[Size]) {
            const auto n_bytes = write(region{(addr)arr, sizeof(Element) * Size});
            return n_bytes;
        }

        /// Derail off the linear course of the stream to jump to another offset
        /// temporarily. Exceptions will not be handled in the wrapper.
        template <detail::Primitive Ret>
        Ret derail(const std::int64_t offset, const stream_origin origin, const std::function<Ret()>& cb) noexcept {
            const auto saved_offset = m_offset;
            seek(offset, origin); // Seek to intended offset

            const auto ret = cb(); // Derail to callback
            m_offset = saved_offset;

            return ret;
        }

        /// Derail off the linear course of the stream offsetted from the
        /// current stream position temporarily.
        template <detail::Primitive Ret>
        Ret derail(const std::int64_t offset, const std::function<Ret()>& cb) noexcept {
            return derail(offset, stream_origin::current, cb);
        }

        /// Derail off the linear course of the stream to jump to another offset
        /// temporarily.
        template <detail::Void Ret>
        Ret derail(const std::int64_t offset, const stream_origin origin, const std::function<Ret()>& cb) noexcept {
            return derail(offset, origin, [&]() -> int { cb(); return 0; });
        }

        /// Derail off the linear course of the stream offsetted from the
        /// current stream position temporarily.
        template <detail::Void Ret>
        Ret derail(const std::int64_t offset, const std::function<Ret()>& cb) noexcept {
            return derail<void>(offset, stream_origin::current, cb);
        }
    };

    namespace detail {
        template <typename T>
        concept StreamLike = std::derived_from<T, stream> && !std::is_abstract_v<T>;
    }

    class memory_stream : public stream, public detail::noncopyable {
    protected:
        explicit memory_stream() = default;

        /// Reads data from stream into buffer without changing offset.
        /// Returns number of bytes actually read.
        std::size_t read(std::int8_t* dst, std::size_t size) const override {
            throw std::runtime_error("not implemented");
        }

        /// Writes data from buffer to stream without changing offset.
        /// Returns number of bytes actually written.
        std::size_t write(std::int8_t* src, std::size_t size) const override {
            throw std::runtime_error("not implemented");
        }

    public:
        /// Get the base address of the underlying memory.
        /// This is an abstract method to be implemented by derived classes.
        [[nodiscard]] virtual addr base() const;

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        std::size_t seek(std::int64_t offset, stream_origin origin) override;
    };

    namespace detail {
        template <typename T>
        concept MemoryStreamLike = std::derived_from<T, memory_stream> && !std::is_abstract_v<T>;
    }

    /// Memory-based stream implementation using hydra::buffer.
    /// Provides in-memory streaming operations.
    class local_stream final : public memory_stream {
    protected:
        region m_buffer;

    public:
        explicit local_stream(const region& buffer) : m_buffer(buffer) { }

        explicit local_stream(region&& buffer) : m_buffer(std::move(buffer)) { }

        std::size_t read(std::int8_t* dest, std::size_t size) const override;

        std::size_t write(std::int8_t* dest, std::size_t size) const override;

        // Destructor for memory_stream.
        /// Cleans up stream resources.
        ~local_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always local.
        [[nodiscard]] addr base() const override;

        /// Gets the underlying buffer.
        /// Returns reference to internal buffer.
        [[nodiscard]] const region& buffer() const;
    };
}
