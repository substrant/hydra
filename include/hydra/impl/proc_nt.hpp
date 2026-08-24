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
    };
}

namespace hy::shim {
    class proc : public impl::proc {
    protected:
        pid_t pid;
    public: // todo: remove
        nt::hnd hnd;

        err claim() override;
        
    public:
        proc() : pid(0ul) { }

        static err open_hnd(HANDLE handle, hy::proc* proc);

        std::size_t mm_read(ptr local_dst, ptr remote_src, std::size_t size) override;

        std::size_t mm_write(ptr remote_dst, ptr local_src, std::size_t size) override;

        bool mm_protect(ptr remote_base, std::size_t size, mem_mode mode) override;

        blk mm_alloc(ptr remote_base, std::size_t size, mem_mode mode) override;

        bool mm_free(ptr remote_base) override;
    };
}
