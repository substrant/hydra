#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include "hydra/detail.hpp"
#include "hydra/syscall.hpp"

namespace hy {
    namespace detail {
        template <typename R>
        concept ValidHandleReturn = std::same_as<R, void> || std::same_as<R, BOOL> || std::same_as<R, bool> || std::same_as<R, NTSTATUS>;

        template <typename F>
        concept HandleCallable = requires(typename func_traits<std::remove_cvref_t<F>>::template arg_t<0> h) {
            { std::invoke(std::declval<F>(), h) } -> ValidHandleReturn;
        };
    }

    template <detail::HandleCallable auto CloseFn>
    class handle {
    public:
        using type = detail::func_traits<std::remove_cvref_t<decltype(CloseFn)>>::template arg_t<0>;

    private:
        type m_handle = nullptr;
        bool m_owner = false;
        ACCESS_MASK m_access = 0;

    protected:
        constexpr bool close_internal() {
            auto result = std::invoke(CloseFn, m_handle);
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(CloseFn), type>, void>)
                return true;

            return result;
        }

    public:
        explicit handle(const bool no_dispose = false) : m_owner(!no_dispose) { }

        explicit handle(const type handle, const bool no_dispose = false) : m_handle(handle), m_owner(!no_dispose) { }

        handle& operator=(const type handle) {
            // Close old handle
            if (is_valid()) close_internal();

            // Set new handle
            m_handle = handle;
            return *this;
        };

        [[nodiscard]] bool is_valid() const { return m_handle != nullptr; }

        type get() const { return m_handle; }

        type operator*() const { return m_handle; }

        operator type() const { return m_handle; } // NOLINT: Expected implicit conversion

        ACCESS_MASK access() {
            if (m_access) return m_access;

            OBJECT_BASIC_INFORMATION info{};
            DWORD info_written;

            if (!NT_SUCCESS(syscall::NtQueryObject(
                m_handle,
                ObjectBasicInformation,
                &info,
                sizeof(info),
                &info_written
            )) || info_written != sizeof(info)) return 0;

            return m_access = info.GrantedAccess;
        }

        bool close() {
            if (!is_valid())
                return false;

            const bool success = m_owner ? close_internal() : true;
            if (success) m_handle = nullptr;

            return success;
        }

        ~handle() { close(); }
    };

    using nt_handle = handle<syscall::NtClose>;

    template <typename T>
    struct _impl_handle_t;

    template <auto CloseFn>
    struct _impl_handle_t<handle<CloseFn>> {
        using type = handle<CloseFn>::type;
    };

    template <typename T>
    using handle_t = _impl_handle_t<T>::type;
}
