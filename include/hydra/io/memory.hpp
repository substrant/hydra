#pragma once

#include <cstdint>
#include <memory>

#include "hydra/stream.hpp"

namespace hy::detail {
    /// Base class for implementing memory streams.
    class mem_stream_impl : public stream {
    protected:
        explicit mem_stream_impl() { }

    public:
        /// Get the base address of the underlying memory.
        /// This is an abstract method to be implemented by derived classes.
        virtual addr base() const;

        /// Seeks to specified position in stream.
        /// Returns new position after seek.
        std::size_t seek(std::int64_t offset, stream_origin origin) override;
    };
}
