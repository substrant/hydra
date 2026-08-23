#pragma once

#include <cstdint>
#include <utility>

namespace hy {
    enum class mem_mode : std::uint8_t {
        none    = 0,
        read    = 1 << 0,
        write   = 1 << 1,
        exec    = 1 << 2,
        guard   = 1 << 3
    };

    constexpr mem_mode operator|(const mem_mode lhs, const mem_mode rhs) {
        return static_cast<mem_mode>(std::to_underlying(lhs) | std::to_underlying(rhs));
    }

    constexpr mem_mode operator&(const mem_mode lhs, const mem_mode rhs) {
        return static_cast<mem_mode>(std::to_underlying(lhs) & std::to_underlying(rhs));
    }

    constexpr bool operator==(const mem_mode lhs, const std::uint8_t rhs) {
        return static_cast<mem_mode>(std::to_underlying(lhs) & rhs) == rhs;
    }
}
