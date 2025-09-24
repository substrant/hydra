#pragma once

#include "hydra/util/stream.hpp"
#include "hydra/util/memory.hpp"
#include "hydra/detail/noncopyable.hpp"

namespace hydra::io {
    /// Stream reader for reading data from streams.
    /// Separates data source from encoding logic.
    class stream_reader : public detail::noncopyable {
        std::shared_ptr<stream> m_stream;

    public:
        /// Constructor taking stream shared pointer.
        /// Reader shares ownership of the stream.
        explicit stream_reader(std::shared_ptr<stream> stream) : m_stream(std::move(stream)) { }

        /// Constructor taking stream reference.
        /// Reader creates shared pointer from stream's self-reference.
        explicit stream_reader(stream& stream) : m_stream(stream.get_shared()) { }

        /// Destructor for stream_reader.
        /// Automatically releases shared stream reference.
        ~stream_reader() = default;

        /// Move constructor for stream_reader.
        /// Transfers stream ownership.
        stream_reader(stream_reader&& other) noexcept 
            : m_stream(std::move(other.m_stream)) { }

        /// Move assignment for stream_reader.
        /// Transfers stream ownership.
        stream_reader& operator=(stream_reader&& other) noexcept {
            if (this != &other) {
                m_stream = std::move(other.m_stream);
            }
            return *this;
        }

        /// Reads a single byte from stream.
        /// Returns byte value read from stream.
        std::uint8_t read_byte() {
            if (!m_stream) {
                throw error("Stream reader has no associated stream");
            }

            std::uint8_t byte;
            const auto bytes_read = m_stream->read(&byte, 1);
            
            if (bytes_read != 1) {
                throw error("Failed to read byte from stream");
            }

            return byte;
        }

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

    /// Stream writer for writing data to streams.
    /// Separates data destination from encoding logic.
    class stream_writer : public detail::noncopyable {
    private:
        std::shared_ptr<stream> m_stream;

    public:
        /// Constructor taking stream shared pointer.
        /// Writer shares ownership of the stream.
        explicit stream_writer(std::shared_ptr<stream> stream) : m_stream(std::move(stream)) { }

        /// Constructor taking stream reference.
        /// Writer creates shared pointer from stream's self-reference.
        explicit stream_writer(stream& stream) : m_stream(stream.get_shared()) { }

        /// Destructor for stream_writer.
        /// Automatically releases shared stream reference.
        ~stream_writer() = default;

        /// Move constructor for stream_writer.
        /// Transfers stream ownership.
        stream_writer(stream_writer&& other) noexcept 
            : m_stream(std::move(other.m_stream)) { }

        /// Move assignment for stream_writer.
        /// Transfers stream ownership.
        stream_writer& operator=(stream_writer&& other) noexcept {
            if (this != &other) {
                m_stream = std::move(other.m_stream);
            }
            return *this;
        }

        /// Writes a single byte to stream.
        /// Writes byte value to stream.
        void write_byte(std::uint8_t byte) {
            if (!m_stream) {
                throw error("Stream writer has no associated stream");
            }

            const auto bytes_written = m_stream->write(&byte, 1);
            
            if (bytes_written != 1) {
                throw error("Failed to write byte to stream");
            }
        }

        /// Gets the underlying stream.
        /// Returns shared pointer to associated stream.
        std::shared_ptr<stream> get_stream() const {
            return m_stream;
        }

        /// Checks if writer has a valid stream.
        /// Returns true if stream is available.
        bool is_valid() const {
            return m_stream != nullptr;
        }
    };
}
