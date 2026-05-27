#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include "hydra/detail.hpp"
#include "hydra/memory.hpp"
#include "hydra/handle.hpp"
#include "hydra/syscall.hpp"

namespace hy {
    class thread : public detail::noncopyable, public std::enable_shared_from_this<thread> {
        std::shared_ptr<thread> m_this = nullptr;

        handle<CloseHandle> m_handle;
        ACCESS_MASK m_access;

        std::shared_ptr<thread> init() {
            return m_this = shared_from_this();
        }

    public:
        HYDRA_INTERNAL("Use 'thread::open' to open threads.")
        explicit thread(const HANDLE handle, const bool no_dispose = false)
            : m_handle(handle, no_dispose), m_access(m_handle.access()) { }

        static std::shared_ptr<thread> open(HANDLE handle) {
            const auto obj = std::make_shared<thread>(handle);
            return obj->init();
        }

        static std::shared_ptr<thread> open(DWORD id, ACCESS_MASK access = THREAD_ALL_ACCESS) {
            HANDLE h_thrd;

            CLIENT_ID cid;
            cid.UniqueProcess = nullptr;
            cid.UniqueThread = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(id)); // shitty microsoft design 420

            OBJECT_ATTRIBUTES attr;
            InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);

            if (!NT_SUCCESS(NtOpenThread(&h_thrd, access, &attr, &cid)))
                return nullptr;

            const auto obj = std::make_shared<thread>(h_thrd);
            return open(h_thrd);
        }

        DWORD id() const { return GetThreadId(m_handle); }
        
        DWORD pid() const { return GetProcessIdOfThread(m_handle); }

        addr entry() const {
            std::uintptr_t info{};
            (void)syscall::NtQueryInformationThread(m_handle, ThreadQuerySetWin32StartAddress, &info, sizeof(info), nullptr);
            return info;
        }

        ULONG suspend() const {
            ULONG prev_depth;
            if (!NT_SUCCESS(NtSuspendThread(m_handle, &prev_depth)))
                return -1;

            return prev_depth + 1;
        }

        ULONG resume() const {
            ULONG prev_depth;
            if (!NT_SUCCESS(syscall::NtResumeThread(m_handle, &prev_depth)))
                return -1;

            return prev_depth - 1;
        }

        BOOL kill(const LONG status = 0, NTSTATUS* p_status = nullptr) const {
            NTSTATUS dummy_status;
            if (!p_status) p_status = &dummy_status;
            
            *p_status = syscall::NtTerminateThread(m_handle, status);
            return NT_SUCCESS(*p_status);
        }

        BOOL get_context(const PCONTEXT ctx, const DWORD scope = CONTEXT_FULL) const {
            ctx->ContextFlags = scope;
            return NT_SUCCESS(NtGetContextThread(m_handle, ctx));
        }

        BOOL set_context(const PCONTEXT ctx) const {
            return NT_SUCCESS(NtSetContextThread(m_handle, ctx));
        }
    };
}
