#pragma once

#include <hydra/blk.hpp>
#include <functional>

namespace hy {
    /// Represents the origin position for a stream operation.
    /// Used for seeking within a stream.
    enum class stm_origin : std::uint8_t {
        begin,
        current,
        end
    };

    /// Represents the mode of offset handling in a stream.
    /// Absolute mode uses fixed offsets, relative mode advances the offset.
    enum class stm_mode : std::uint8_t {
        absolute,
        relative
    };

    /// Base abstract stream class for read/write operations.
    /// Provides fundamental streaming operations similar to .NET Stream.
    class stm : public dtl::clone_moveable<stm> {
    public:
        std::size_t position = 0;
        stm_mode mode = stm_mode::relative;

        /// Default constructor for stream.
        /// Initializes stream in valid state.
        stm() = default;

        /// Virtual destructor for stream.
        /// Ensures proper cleanup of derived classes.
        virtual ~stm() = default;

        /// Reads data from stream into buffer without changing offset.
        /// Returns number of bytes actually read.
        [[nodiscard]] virtual std::size_t read(std::int8_t* dst, std::size_t size) {
            return 0;
        }

        /// Writes data from buffer to stream without changing offset.
        /// Returns number of bytes actually written.
        [[nodiscard]] virtual std::size_t write(const std::int8_t* src, std::size_t size) {
            return 0;
        }

        /// Reads data from stream into buffer and advances offset.
        /// Returns number of bytes actually read.
        [[nodiscard]] inline std::size_t read(const blk& dst) {
            return read(dst.base, dst.size);
        }

        /// Writes data from buffer to stream and advances offset.
        /// Returns number of bytes actually written.
        [[nodiscard]] inline std::size_t write(const blk& src) {
            return write(src.base, src.size);
        }

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        virtual std::size_t seek(stm_origin origin, std::make_signed_t<std::size_t> offset = 0) {
            return -1;
        }

        /// Gets the length of the stream.
        /// Returns stream length.
        virtual std::size_t length() {
            const auto old_offset = position;
            const auto size = seek(stm_origin::end, 0);

            position = old_offset;
            return size;
        }

        /// Jump forwards/backwards by a specified number of bytes in stream.
        /// Returns new position after skipping.
        std::size_t advance(const std::make_signed_t<std::size_t> offset) {
            return seek(stm_origin::current, offset);
        }

        template <dtl::Primitive T>
        std::size_t read(T* ref) {
            const auto n = read(ptr(ref), sizeof(T));

            if (mode == stm_mode::relative)
                position += n;

            return n;
        }

        template <dtl::Primitive Element, std::size_t Size>
        std::size_t read(Element(&arr)[Size]) {
            const auto n_bytes = read(ptr(arr), sizeof(Element) * Size);

            if (mode == stm_mode::relative)
                position += n_bytes;

            return n_bytes;
        }

        template <class T, std::size_t Size = sizeof(T)>
        std::size_t write(const T& object) {
            const auto n_bytes = write(ptr(&object), Size);
            if (mode == stm_mode::relative) position += n_bytes;
            return n_bytes;
        }

        template <dtl::Primitive Element, std::size_t Size>
        std::size_t write(const Element(&arr)[Size]) {
            const auto n_bytes = write(ptr(arr), sizeof(Element) * Size);

            if (mode == stm_mode::relative)
                position += n_bytes;

            return n_bytes;
        }
    };

    namespace dtl {
        template <typename T>
        concept StmLike = std::derived_from<T, stm> && !std::is_abstract_v<T>;
    }
}
