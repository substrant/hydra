#pragma once

#include <hydra/detail/noncopyable.hpp>
#include <hydra/io/error.hpp>

namespace hy::mem {
    class buffer;
}

namespace hy::io {
    /// Represents the origin position for a stream operation.
    /// Used for seeking within a stream.
    enum class origin : std::uint8_t {
        begin,
        current,
        end
    };

    /// Base abstract stream class for read/write operations.
    /// Provides fundamental streaming operations similar to .NET Stream.
    class stream : public detail::noncopyable, public std::enable_shared_from_this<stream> {
    protected:
        bool m_disposed = false;
        std::shared_ptr<stream> m_self = nullptr;

        void check_disposed() const {
            if (m_disposed) throw error("Stream has been disposed");
        }

        /// Initializes the shared_ptr self-reference.
        /// Must be called after construction in derived classes.
        void init() {
            m_self = shared_from_this();
        }

    public:
        /// Default constructor for stream.
        /// Initializes stream in valid state.
        stream() = default;

        /// Virtual destructor for stream.
        /// Ensures proper cleanup of derived classes.
        virtual ~stream() = default;

        /// Move constructor for stream.
        /// Transfers stream state.
        stream(stream&& other) noexcept
            : m_disposed(std::exchange(other.m_disposed, true))
            , m_self(std::exchange(other.m_self, nullptr)) { }

        /// Move assignment for stream.
        /// Transfers stream state.
        stream& operator=(stream&& other) noexcept {
            if (this != &other) {
                m_disposed = std::exchange(other.m_disposed, true);
                m_self = std::exchange(other.m_self, nullptr);
            }
            return *this;
        }

        /// Gets a shared pointer to the stream.
        /// Useful for IO constructors.
        auto get_shared() const { return m_self; }

        /// Checks if stream can be read from.
        /// Returns true if reading is supported.
        virtual bool can_read() const = 0;

        /// Checks if stream can be written to.
        /// Returns true if writing is supported.
        virtual bool can_write() const = 0;

        /// Checks if stream supports seeking.
        /// Returns true if seeking is supported.
        virtual bool can_seek() const = 0;

        /// Gets the length of the stream.
        /// Returns total stream length in bytes.
        virtual std::size_t length() const = 0;

        /// Gets the current position in stream.
        /// Returns current read/write position.
        virtual std::size_t position() const = 0;

        /// Sets the current position in stream.
        /// Updates read/write position.
        virtual void set_position(std::size_t pos) = 0;

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        virtual std::size_t read(const mem::buffer& buffer, std::size_t count) = 0;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        virtual std::size_t write(const mem::buffer& buffer, std::size_t count) = 0;

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        virtual std::size_t seek(std::size_t offset, origin origin = origin::begin) = 0;

        /// Flushes any buffered data to underlying storage.
        /// Ensures data is written.
        virtual void flush() = 0;

        /// Closes the stream and releases resources.
        /// Stream becomes unusable after close.
        virtual void close() {
            m_disposed = true;
            m_self.reset();
        }

        /// Checks if stream is disposed.
        /// Returns true if stream has been disposed.
        bool is_disposed() const {
            return m_disposed;
        }
    };
}
