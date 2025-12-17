#pragma once

#include "memory.hpp"

namespace hy {
    class process;

    /// Memory-based stream implementation using hydra::process.
    /// Provides in-memory streaming operations.
    class remote_stream final : public memory_stream {
    protected:
        std::shared_ptr<process> m_proc;
        addr m_base;

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read(std::int8_t* dst, std::size_t size) const override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write(std::int8_t* src, std::size_t size) const override;

    public:
        HYDRA_INTERNAL("Use 'remote_stream::from' to create remote streams.")
        explicit remote_stream(const std::shared_ptr<process>& proc, const addr base)
            : m_proc(proc), m_base(base) { }

        std::shared_ptr<remote_stream> static from(const std::shared_ptr<process>& proc, const addr base) {
            return std::make_shared<remote_stream>(proc, base);
        }

        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~remote_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        [[nodiscard]] addr base() const override { return m_base; }
    };
}