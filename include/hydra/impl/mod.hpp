#pragma once

#include <hydra/err.hpp>

#include <concepts>
#include <cstdint>
#include <generator>

namespace hy {
    enum class mod_state : std::uint8_t {
        header = 0,
        flat,
        mapped,
        inherit = -1
    };

    struct seg;
    class mod;
}

namespace hy::impl {
    template <class T>
    concept ModImpl = std::default_initializable<T> && requires(
        T& impl,
        hy::mod& owner,
        const mod_state state,
        err* error
    ) {
        { impl.parse(owner, state) } -> std::same_as<err>;
        { impl.segments(owner, error) } -> std::same_as<std::generator<hy::seg>>;
        { impl.calc_size(owner, state) } -> std::same_as<std::size_t>;
    };
}
