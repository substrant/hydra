#pragma once

#include <hydra/dtl.hpp>
#include <hydra/ptr.hpp>

namespace hy::scn {

    /// Match a specific pattern given a pattern and mask.
    /// Returns boolean that determines if match succeeded.
    static bool match_aob(const ptr at, const dtl::Byte auto* in_pattern, const dtl::Byte auto* in_mask, const std::size_t size) {
        const auto pattern = reinterpret_cast<const char*>(in_pattern);
        const auto mask = reinterpret_cast<const char*>(in_mask);

        for (std::size_t i = 0; i < size; ++i) {
            if (mask[i] == '?') continue;
            if (at.p[i] != pattern[i]) return false;
        }

        return true;
    }

    /// Match a specific pattern given a pattern and mask.
    /// Returns generator of matching addresses.
    static std::generator<ptr> scan_aob(const ptr base, std::size_t size, const dtl::Byte auto* in_pattern, const dtl::Byte auto* in_mask) {
        if (!in_pattern || !in_mask || !base || !size)
            co_return;

        if (!size) {
            const auto mask = reinterpret_cast<const char*>(in_mask);
            size = std::strlen(mask);
        }

        const auto end = base + size;
        for (ptr at = base; at <= end; ++at) {
            // TODO: Let's have fun here someday. Multithreading? SIMD? Sick of scanning huge binaries.
            if (match_aob(at, in_pattern, in_mask, size))
                co_yield at;
        }
    }

}