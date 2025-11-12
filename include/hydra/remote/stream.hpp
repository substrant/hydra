#pragma once

#include "hydra/io/memory.hpp"

namespace hy {
    class process;
}

namespace hy::remote {
    /// Memory-based stream implementation using hydra::process.
    /// Provides in-memory streaming operations.
    class mem_stream final : public detail::mem_stream_impl {
    protected:
        std::shared_ptr<process> m_proc;
        addr m_base;

        explicit mem_stream(const std::shared_ptr<process>& proc, const addr base)
            : m_proc(proc), m_base(base) { }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read_impl(std::uint8_t* base, std::size_t size) override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write_impl(std::uint8_t* base, std::size_t size) override;

    public:
        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~mem_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        addr base() const override;
    };
}
