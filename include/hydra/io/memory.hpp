#pragma once

#include "io/stream.hpp"
#include "user/process.hpp"

namespace hy::mem {
    struct stream_attrs {
        std::size_t m_capacity = 0;
        bool m_writable = true;
        bool m_expandable = true;
        bool m_remote = false;
    };

    namespace detail {
        class stream : public io::stream, protected stream_attrs {
        protected:
            std::size_t m_position = 0;
            std::size_t m_length = 0;

            explicit stream(const stream_attrs& attrs) : stream_attrs(attrs) {}

        public:
            /// Destructor for memory_stream.
            /// Cleans up stream resources.
            ~stream() override = default;

            /// Get the base address of the underlying memory.
            /// This is an abstract method to be implemented by derived classes.
            virtual addr base() const = 0;

            /// Get a buffer to the underlying memory.
            /// This buffer may or may not be local and is not owned.
            buffer get_buffer() const {
                check_disposed();
                return { base(), m_length };
            }

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
            void set_position(const std::size_t pos) override {
                check_disposed();
                m_position = std::min(pos, m_length);
            }

            /// Seeks to specified position in stream.
            /// Returns new position after seek.
            std::size_t seek(const std::size_t offset, const io::origin origin = io::origin::begin) override {
                check_disposed();

                std::size_t new_pos;
                switch (origin) {
                case io::origin::begin:
                    new_pos = offset;
                    break;
                case io::origin::current:
                    new_pos = m_position + offset;
                    break;
                case io::origin::end:
                    new_pos = m_length + offset;
                    break;
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

            /// Sets stream length, truncating or expanding as needed.
            /// Updates stream size.
            virtual void set_length(const std::size_t new_length) {
                check_disposed();

                if (!m_writable)
                    throw io::error("Stream is not writable");

                m_length = new_length;
                m_position = std::min(m_position, m_length);
            }
        };
    }

    /// Memory-based stream implementation using hydra::mem::buffer.
    /// Provides in-memory streaming operations.
    class local_stream final : public detail::stream {
        buffer m_buffer;

        explicit local_stream(buffer buffer, const stream_attrs& attrs)
            : stream(attrs), m_buffer(std::move(buffer)) { }

        void ensure_capacity(const std::size_t required) {
            // Already enough space
            if (required <= m_buffer.size()) return;
            if (!m_expandable) throw io::error("Cannot expand fixed-size memory stream");

            // Grow buffer by 50% or to required size, whichever is larger
            const auto new_size = std::max(required, m_buffer.size() + m_buffer.size() / 2);
            m_buffer.resize(new_size);
        }

    public:
        /// Creates memory stream wrapping existing buffer.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<local_stream> load(const buffer& buffer, const stream_attrs attrs) {
            auto stream = std::make_shared<local_stream>(buffer, attrs);
            stream->init();
            return stream;
        }

        // Destructor for memory_stream.
        /// Cleans up stream resources.
        ~local_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always local.
        addr base() const override {
            check_disposed();
            return m_buffer.base();
        }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read(const buffer& buffer, const std::size_t count) override {
            check_disposed();
            if (!buffer || count == 0) return 0;

            const auto available = m_length - m_position;
            const auto to_read = std::min(count, available);

            if (to_read > 0) {
                std::memcpy(buffer.base(), m_buffer.base() + m_position, to_read);
                m_position += to_read;
            }

            return to_read;
        }

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write(const buffer& buffer, const std::size_t count) override {
            check_disposed();

            if (!m_writable) throw io::error("Stream is not writable");
            if (!buffer || count == 0) return 0;

            const auto end_pos = m_position + count;
            ensure_capacity(end_pos);

            std::memcpy(m_buffer.base() + m_position, buffer.base(), count);
            m_position = end_pos;
            m_length = std::max(m_length, end_pos);

            return count;
        }

        /// Gets the underlying buffer.
        /// Returns reference to internal buffer.
        const buffer& buffer() const {
            check_disposed();
            return m_buffer;
        }

        /// Sets stream length, truncating or expanding as needed.
        /// Updates stream size.
        void set_length(const std::size_t new_length) override {
            if (new_length > m_buffer.size()) ensure_capacity(new_length);
            stream::set_length(new_length);
        }
    };

    /// Memory-based stream implementation using hydra::process.
    /// Provides in-memory streaming operations.
    class remote_stream final : public detail::stream {
    protected:
        std::shared_ptr<process> m_proc;
        addr m_base;

        explicit remote_stream(const std::shared_ptr<process>& proc, const addr base, const stream_attrs& attrs)
            : stream(attrs), m_proc(proc), m_base(base) { }

    public:
        /// Creates memory stream wrapping remote memory.
        /// Returns shared pointer to new stream.
        static std::shared_ptr<remote_stream> load(const std::shared_ptr<process>& proc, const addr base, const stream_attrs attrs) {
            auto stream = std::make_shared<remote_stream>(proc, base, attrs);
            stream->init();
            return stream;
        }

        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~remote_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        addr base() const override {
            check_disposed();
            return m_base;
        }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read(const buffer& buffer, const std::size_t count) override {
            check_disposed();
            if (!buffer || count == 0) return 0;

            const auto available = m_length - m_position;
            auto bytes_read = std::min(count, available);

            if (!bytes_read)
                return 0;

            bytes_read = m_proc->mm_read(buffer.base() + m_position, buffer, bytes_read);
            if (!bytes_read) throw io::error("Failed to read process memory");

            return bytes_read;
        }

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write(const buffer& buffer, const std::size_t count) override {
            check_disposed();

            if (!m_writable) throw io::error("Stream is not writable");
            if (!buffer || count == 0) return 0;

            const auto bytes_written = m_proc->mm_write(m_base + m_position, buffer, count);
            if (!bytes_written) throw io::error("Failed to write process memory");

            m_position += bytes_written;
            m_length = std::max(m_length, m_position);

            return bytes_written;
        }

        /// Dumps the underlying buffer to a local stream.
        /// Returns reference to internal buffer.
        std::shared_ptr<local_stream> dump(dump_context* ctx) const {
            check_disposed();

            const auto buffer = buffer::create(m_length);
            (void)m_proc->mm_dump(m_base, buffer, ctx);

            const auto stream = local_stream::load(buffer, {
                .m_capacity = m_length,
                .m_writable = false,
                .m_expandable = false,
            });

            stream->set_position(m_position);
            return stream;
        }
    };
}
