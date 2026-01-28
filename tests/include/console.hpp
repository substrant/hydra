#pragma once

#include <iostream>
#include <cstdio>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <vector>

#include "hydra/memory.hpp"

namespace console {
    extern std::string prefix();

    extern void _print(const std::string&& line);

    template <typename... Args>
    __forceinline std::string get_line(const char* fmt, Args&&... args) {
        const auto pfx = prefix();

        const std::size_t size = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
        if (size <= 0) throw std::runtime_error("Failed to format string");

        std::string buffer(size + 1, '\0');
        std::snprintf(buffer.data(), size + 1, fmt, std::forward<Args>(args)...);

        return buffer;
    }

    template <typename... Args>
    __forceinline void print(const char* fmt, Args&&... args) {
        const auto pfx = prefix();

        const auto line = get_line(fmt, std::forward<Args>(args)...);
        _print(std::string("[+] ") + line);
    }

    template <typename... Args>
    __forceinline int fail(const int code, const char* fmt, Args&&... args) {
        const auto pfx = prefix();

        const auto line = get_line(fmt, std::forward<Args>(args)...);
        _print(std::string("[-] ") + line);

        return code;
    }
}
