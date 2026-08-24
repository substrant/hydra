#pragma once

#include <hydra/ost.hpp>
#include <hydra/err.hpp>

#ifdef HY_OS_NT
#   include <hydra/impl/proc_nt.hpp>
#else
#   error Unsupported platform
#endif

namespace hy {
    class proc final : public shim::proc {
    public:
        static err open_pid(const pid_t pid, proc* proc) {
            proc->pid = pid;
            return proc->claim();
        }

        using proc::mm_alloc;

        inline blk mm_alloc(const std::size_t size, const mem_mode mode) {
            return mm_alloc(nullptr, size, mode);
        }

        ~proc() override { }
    };
}

namespace hy::impl {
    inline proc::~proc() = default;
}
