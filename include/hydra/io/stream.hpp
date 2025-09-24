#pragma once

#include <algorithm>
#include <stdexcept>
#include <algorithm>
#include <memory>

#include "hydra/util/memory.hpp"
#include "hydra/detail/noncopyable.hpp"
#include "hydra/io/error.hpp"

namespace hydra::io {
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
        virtual std::size_t read(std::uint8_t* buffer, std::size_t count) = 0;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        virtual std::size_t write(const std::uint8_t* buffer, std::size_t count) = 0;

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        virtual std::size_t seek(std::size_t offset, int origin = 0) = 0;

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

        /// Gets shared pointer to this stream.
        /// Returns shared_ptr reference to self.
        std::shared_ptr<stream> get_shared() const {
            return m_self;
        }
    };

    /// Memory-based stream implementation using hydra::mem::buffer.
    /// Provides in-memory streaming operations.
    class memory_stream : public stream {
    private:
        mem::buffer m_buffer;
        std::size_t m_position = 0;
        std::size_t m_length = 0;
        bool m_writable = true;
        bool m_expandable = true;

        /// Private constructor for factory methods.
        /// Prevents direct construction.
        explicit memory_stream(std::size_t capacity, bool expandable) 
            : m_buffer(capacity), m_expandable(expandable) { }

        /// Private constructor from buffer.
        /// Prevents direct construction.
        explicit memory_stream(const mem::buffer& buffer, bool writable, bool expandable)
            : m_buffer(buffer), m_length(buffer.size()), m_writable(writable), m_expandable(expandable) { }

        /// Private constructor from raw data.
        /// Prevents direct construction.
        explicit memory_stream(const std::uint8_t* data, std::size_t size, bool writable, bool expandable)
            : m_buffer(mem::addr(const_cast<std::uint8_t*>(data)), size), m_length(size), m_writable(writable), m_expandable(expandable) { }

        void ensure_capacity(std::size_t required) {
            if (required > m_buffer.size()) {
                if (!m_expandable) {
                    throw error("Cannot expand fixed-size memory stream");
                }
                // Grow buffer by 50% or to required size, whichever is larger
                const auto new_size = std::max(required, m_buffer.size() + m_buffer.size() / 2);
                m_buffer.resize(new_size);
            }
        }

    public:
        /// Creates empty expandable memory stream.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<memory_stream> create() {
            auto stream = std::shared_ptr<memory_stream>(new memory_stream(0, true));
            stream->init();
            return stream;
        }

        /// Creates expandable memory stream with initial capacity.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<memory_stream> create(std::size_t capacity) {
            auto stream = std::shared_ptr<memory_stream>(new memory_stream(capacity, true));
            stream->init();
            return stream;
        }

        /// Creates memory stream wrapping existing buffer.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<memory_stream> from_buffer(const mem::buffer& buffer, bool writable = true) {
            auto stream = std::shared_ptr<memory_stream>(new memory_stream(buffer, writable, false));
            stream->init();
            return stream;
        }

        /// Creates memory stream from raw data pointer.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<memory_stream> from_data(const std::uint8_t* data, std::size_t size, bool writable = false) {
            auto stream = std::shared_ptr<memory_stream>(new memory_stream(data, size, writable, false));
            stream->init();
            return stream;
        }

        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~memory_stream() override = default;

        /// Checks if stream can be read from.
        /// Returns true (memory streams always readable).
        bool can_read() const override {
            check_disposed();
            return true;
        }

        /// Checks if stream can be written to.
        /// Returns true if stream is writable.
        bool can_write() const override {
            check_disposed();
            return m_writable;
        }

        /// Checks if stream supports seeking.
        /// Returns true (memory streams always seekable).
        bool can_seek() const override {
            check_disposed();
            return true;
        }

        /// Gets the length of the stream.
        /// Returns current stream length.
        std::size_t length() const override {
            check_disposed();
            return m_length;
        }

        /// Gets the current position in stream.
        /// Returns current read/write position.
        std::size_t position() const override {
            check_disposed();
            return m_position;
        }

        /// Sets the current position in stream.
        /// Updates read/write position.
        void set_position(std::size_t pos) override {
            check_disposed();
            m_position = std::min(pos, m_length);
        }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read(std::uint8_t* buffer, std::size_t count) override {
            check_disposed();
            if (!buffer || count == 0) return 0;

            const auto available = m_length - m_position;
            const auto to_read = std::min(count, available);
            
            if (to_read > 0) {
                std::memcpy(buffer, m_buffer.base().p + m_position, to_read);
                m_position += to_read;
            }
            
            return to_read;
        }

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write(const std::uint8_t* buffer, std::size_t count) override {
            check_disposed();
            if (!m_writable) {
                throw error("Stream is not writable");
            }
            if (!buffer || count == 0) return 0;

            const auto end_pos = m_position + count;
            ensure_capacity(end_pos);

            std::memcpy(m_buffer.base().p + m_position, buffer, count);
            m_position = end_pos;
            m_length = std::max(m_length, end_pos);

            return count;
        }

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        std::size_t seek(std::size_t offset, int origin = 0) override {
            check_disposed();
            
            std::size_t new_pos = 0;
            switch (origin) {
                case 0: // Beginning
                    new_pos = offset;
                    break;
                case 1: // Current
                    new_pos = m_position + offset;
                    break;
                case 2: // End
                    new_pos = m_length + offset;
                    break;
                default:
                    throw error("Invalid seek origin");
            }

            m_position = std::min(new_pos, m_length);
            return m_position;
        }

        /// Flushes any buffered data to underlying storage.
        /// No-op for memory streams.
        void flush() override {
            check_disposed();
            // No-op for memory streams
        }

        /// Gets the underlying buffer.
        /// Returns reference to internal buffer.
        const mem::buffer& get_buffer() const {
            check_disposed();
            return m_buffer;
        }

        /// Gets pointer to stream data.
        /// Returns pointer to beginning of stream data.
        const std::uint8_t* data() const {
            check_disposed();
            return m_buffer.base().p;
        }

        /// Sets stream length, truncating or expanding as needed.
        /// Updates stream size.
        void set_length(std::size_t new_length) {
            check_disposed();
            if (!m_writable) {
                throw error("Stream is not writable");
            }

            if (new_length > m_buffer.size()) {
                ensure_capacity(new_length);
            }

            m_length = new_length;
            m_position = std::min(m_position, m_length);
        }

        /// Converts stream contents to buffer.
        /// Returns copy of stream data as buffer.
        mem::buffer to_buffer() const {
            check_disposed();
            mem::buffer result(m_length);
            if (m_length > 0) {
                std::memcpy(result.base().p, m_buffer.base().p, m_length);
            }
            return result;
        }

        /// Gets shared pointer to this memory stream.
        /// Returns shared_ptr with correct type.
        std::shared_ptr<memory_stream> get_shared_memory_stream() const {
            return std::static_pointer_cast<memory_stream>(m_self);
        }
    };
}
