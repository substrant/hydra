#pragma once

#include "memory.hpp"

namespace hy {
    class process;

    /// Memory-based stream implementation using hydra::process.
    /// Provides in-memory streaming operations.
    class remote_stream final : public memory_stream {
    protected:
        process* m_proc;
        addr m_base;

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read(std::int8_t* dst, std::size_t size) const override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write(std::int8_t* src, std::size_t size) const override;

    public:
        explicit remote_stream(process* proc, const addr base)
            : m_proc(proc), m_base(base) { }

        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~remote_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        [[nodiscard]] addr base() const override { return m_base; }
    };
}