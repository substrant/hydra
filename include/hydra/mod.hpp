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

        using shim::mod::calc_size;

        inline std::size_t calc_size() {
            return calc_size(mod_state::inherit);
        }

        using shim::mod::segments;

        inline std::generator<seg> segments() {
            err error;
            return segments(&error);
        }

        std::expected<seg, err> segment(std::string_view name);
    };
}

