#pragma once

#include <hydra/ost.hpp>
#include <hydra/err.hpp>

#ifdef HY_OS_NT
#   include <hydra/impl/proc_nt.hpp>
#else
#   error Unsupported platform
#endif

namespace hy {
    class proc final {
        shim::proc m_impl;

    public:
        static err open_pid(const pid_t pid, proc* proc) {
            return proc->m_impl.open_pid(pid);
        }

#ifdef HY_OS_NT
        static err open_hnd(const HANDLE handle, proc* proc) {
            return proc->m_impl.open_hnd(handle);
        }

        [[nodiscard]] HANDLE native_handle() const { return m_impl.native_handle(); }
#endif

        std::generator<mod&> mod_enum() { return m_impl.mod_enum(*this); }

        std::size_t mm_read(const ptr local_dst, const ptr remote_src, const std::size_t size) {
            return m_impl.mm_read(local_dst, remote_src, size);
        }

        template <typename T>
        std::size_t mm_read(T* local_dst, const ptr remote_src) {
            return mm_read(ptr(local_dst), remote_src, sizeof(T));
        }

        std::size_t mm_write(const ptr remote_dst, const ptr local_src, const std::size_t size) {
            return m_impl.mm_write(remote_dst, local_src, size);
        }

        bool mm_protect(const ptr remote_base, const std::size_t size, const mem_mode mode) {
            return m_impl.mm_protect(remote_base, size, mode);
        }

        blk mm_alloc(const ptr remote_base, const std::size_t size, const mem_mode mode) {
            return m_impl.mm_alloc(remote_base, size, mode);
        }

        inline blk mm_alloc(const std::size_t size, const mem_mode mode) {
            return mm_alloc(nullptr, size, mode);
        }

        bool mm_free(const ptr remote_base) { return m_impl.mm_free(remote_base); }
    };

    static_assert(impl::ProcImpl<shim::proc>);
}
