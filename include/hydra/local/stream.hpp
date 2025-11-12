#pragma once

#include "hydra/io/memory.hpp"

namespace hy {
    class process;
}

namespace hy::local {
    /// Memory-based stream implementation using hydra::buffer.
    /// Provides in-memory streaming operations.
    class mem_stream final : public detail::mem_stream_impl {
    protected:
        region m_buffer;

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read_impl(std::uint8_t* base, std::size_t size) override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write_impl(std::uint8_t* base, std::size_t size) override;

    public:
        explicit mem_stream(const region& buffer) : m_buffer(buffer) { }

        explicit mem_stream(region&& buffer) : m_buffer(std::move(buffer)) { }

        // Destructor for memory_stream.
        /// Cleans up stream resources.
        ~mem_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always local.
        addr base() const override;

        /// Gets the underlying buffer.
        /// Returns reference to internal buffer.
        const region& buffer() const;
    };
}
