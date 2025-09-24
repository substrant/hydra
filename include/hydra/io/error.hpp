#pragma once

#include "hydra/detail/pch.hpp"

namespace hydra::io {
    /// I/O specific exception class for stream operations.
    /// Derived from std::runtime_error for I/O related errors.
    class error final : public std::runtime_error {
    public:
        /// Constructor taking error message.
        /// Creates I/O error with specified message.
        explicit error(const std::string& message) : std::runtime_error(message) {}

        /// Constructor taking C-string error message.
        /// Creates I/O error with specified message.
        explicit error(const char* message) : std::runtime_error(message) {}
    };
}
