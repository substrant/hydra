#pragma once

#include <phnt_windows.h>

#include "detail/noncopyable.hpp"

namespace hy {
    using handle_closer = BOOL(WINAPI*)(HANDLE);

    template <handle_closer CloseFn>
    class unique_handle : public detail::noncopyable {
        HANDLE m_handle = nullptr;
        bool m_owner = nullptr;

    public:
        explicit unique_handle(const bool no_dispose = false) : m_owner(!no_dispose) { }

        explicit unique_handle(const HANDLE handle, const bool no_dispose = false) : m_handle(handle), m_owner(!no_dispose) { }

        unique_handle& operator=(const HANDLE handle) {
            // Close old handle
            if (is_valid())
                CloseFn(m_handle);

            // Set new handle
            m_handle = handle;
            return *this;
        };

        bool is_valid() const { return m_handle != nullptr; }

        HANDLE get() const { return m_handle; }

        HANDLE operator*() const { return m_handle; }

        operator HANDLE() const { return m_handle; }

        bool close() {
            if (!is_valid())
                return false;
            
            const bool success = m_owner ? CloseFn(m_handle) : true;
            if (success) m_handle = nullptr;

            return success;
        }

        ~unique_handle() {
            close();
        }
    };
}
