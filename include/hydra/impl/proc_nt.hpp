#pragma once

#include <hydra/impl/proc.hpp>

#include <hydra/nt/hnd.hpp>
#include <hydra/nt/err.hpp>
#include <type_traits>

namespace hy {
    struct pid_t {
        union {
            HANDLE nt;
            DWORD win32;
        };

        explicit pid_t() { nt = nullptr; }

        template <typename T> requires std::is_same_v<HANDLE, T> || std::is_same_v<DWORD, T>
        explicit pid_t(const T id) { nt = (HANDLE)id; }

        operator HANDLE() const { return nt; }    // NOLINT
        operator DWORD()  const { return win32; } // NOLINT

        operator HANDLE*() { return &nt; }    // NOLINT
        operator DWORD*()  { return &win32; } // NOLINT
    };
}

namespace hy::shim {
    class proc {
        pid_t m_pid;
        nt::hnd m_hnd;

        err claim();
        
    public:
        proc() = default;

        err open_pid(pid_t pid);
        err open_hnd(HANDLE handle);

        [[nodiscard]] HANDLE native_handle() const { return m_hnd; }

        std::generator<hy::mod&> mod_enum(hy::proc& owner);

        std::size_t mm_read(ptr local_dst, ptr remote_src, std::size_t size);
        std::size_t mm_write(ptr remote_dst, ptr local_src, std::size_t size);
        bool mm_protect(ptr remote_base, std::size_t size, mem_mode mode);
        blk mm_alloc(ptr remote_base, std::size_t size, mem_mode mode);
        bool mm_free(ptr remote_base);
    };
}
