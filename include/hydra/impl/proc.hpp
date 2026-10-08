#pragma once

#include <hydra/blk.hpp>
#include <hydra/err.hpp>
#include <hydra/mem.hpp>

#include <concepts>
#include <generator>

namespace hy {
    struct pid_t;
    class mod;
    class proc;
}

namespace hy::impl {
    template <class T>
    concept ProcImpl = std::default_initializable<T> && requires(
        T& impl,
        hy::proc& owner,
        const pid_t pid,
        const ptr address,
        const std::size_t size,
        const mem_mode mode
    ) {
        { impl.open_pid(pid) } -> std::same_as<err>;
        { impl.mod_enum(owner) } -> std::same_as<std::generator<hy::mod&>>;
        { impl.mm_read(address, address, size) } -> std::same_as<std::size_t>;
        { impl.mm_write(address, address, size) } -> std::same_as<std::size_t>;
        { impl.mm_protect(address, size, mode) } -> std::same_as<bool>;
        { impl.mm_alloc(address, size, mode) } -> std::same_as<blk>;
        { impl.mm_free(address) } -> std::same_as<bool>;
    };
}
