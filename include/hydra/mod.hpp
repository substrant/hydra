#pragma once

#include <hydra/ost.hpp>

#ifdef HY_OS_NT
#   include <hydra/impl/mod_nt.hpp>
#else
#   error Unsupported platform
#endif

#include <expected>

namespace hy {
    class mod final : public shim::mod {
    public:
        using shim::mod::mod;

        using shim::mod::segments;

        inline std::generator<seg> segments() {
            err error;
            return segments(&error);
        }

        std::expected<seg, err> segment(std::string_view name);
    };
}

