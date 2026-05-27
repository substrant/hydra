#include <hydra/syscall.hpp>

namespace hy::syscall {
    NTSTATUS NtClose(HANDLE Handle) {
        return detail::inline_syscall(idx::NtClose, Handle);
    }
}