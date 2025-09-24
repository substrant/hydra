#pragma once

#include <memory>

#include "hydra/util/handle.hpp"
#include "hydra/util/memory.hpp"
#include "hydra/detail/noncopyable.hpp"
#include "hydra/user/toolhelp.hpp"

#include <phnt_ntdef.h>
#include <ntpsapi.h>

namespace hydra {
    class thread : public detail::noncopyable, public std::enable_shared_from_this<thread> {
        std::shared_ptr<thread> _self = nullptr;
        unique_handle<CloseHandle> _handle{};
        DWORD _suspension_depth = 0;
        CONTEXT _context{};

        void init() {
            _self = shared_from_this();
        }

    public:
        explicit thread(const DWORD id) : _handle(OpenThread(THREAD_ALL_ACCESS, FALSE, id)) { }

        explicit thread(const HANDLE handle) : _handle(handle, true) { }

        static std::shared_ptr<thread> from_handle(HANDLE handle) {
            const auto obj = std::make_shared<thread>(handle);
            obj->init();
            return obj;
        }

        static std::shared_ptr<thread> from_id(DWORD id) {
            const auto obj = std::make_shared<thread>(id);
            obj->init();
            return obj;
        }

        DWORD id() const { return GetThreadId(_handle); }
        
        DWORD pid() const { return GetProcessIdOfThread(_handle); }

        mem::addr start_address() const {
            std::uintptr_t info{};
            NtQueryInformationThread(_handle, ThreadQuerySetWin32StartAddress, &info, sizeof(info), nullptr);
            return info;
        }

        ULONG suspend() {
            ULONG prev_depth;
            if (!NT_SUCCESS(NtSuspendThread(_handle, &prev_depth)))
                return -1;
            return _suspension_depth = prev_depth + 1;
        }

        ULONG resume() {
            ULONG prev_depth;
            if (!NT_SUCCESS(NtResumeThread(_handle, &prev_depth)))
                return -1;
            return _suspension_depth = prev_depth - 1;
        }

        BOOL kill(const LONG status = 0, NTSTATUS* p_status = nullptr) const {
            NTSTATUS dummy_status;
            if (!p_status) p_status = &dummy_status;
            
            *p_status = NtTerminateThread(_handle, status);
            return NT_SUCCESS(*p_status);
        }

        CONTEXT* get_context(const DWORD scope = CONTEXT_FULL) {
            _context.ContextFlags = scope;
            return GetThreadContext(_handle, &_context) ? &_context : nullptr;
        }

        BOOL set_context() const {
            return SetThreadContext(_handle, &_context);
        }
    };
}
