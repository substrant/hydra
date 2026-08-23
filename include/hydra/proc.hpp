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

        ~proc() override { }
    };
}

namespace hy::impl {
    inline proc::~proc() = default;
}
