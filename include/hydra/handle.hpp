#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include "hydra/detail.hpp"

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

    protected:
        constexpr bool _close() {
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(CloseFn), type>, void>) {
                std::invoke(CloseFn, m_handle);
                return true;
            }
            return std::invoke(CloseFn, m_handle);
        }

    public:
        explicit handle(const bool no_dispose = false) : m_owner(!no_dispose) { }

        explicit handle(const type handle, const bool no_dispose = false) : m_handle(handle), m_owner(!no_dispose) { }

        handle& operator=(const type handle) {
            // Close old handle
            if (is_valid()) _close();

            // Set new handle
            m_handle = handle;
            return *this;
        };

        [[nodiscard]] bool is_valid() const { return m_handle != nullptr; }

        type get() const { return m_handle; }

        type operator*() const { return m_handle; }

        operator type() const { return m_handle; } // NOLINT: Expected implicit conversion

        bool close() {
            if (!is_valid())
                return false;

            const bool success = m_owner ? _close() : true;
            if (success) m_handle = nullptr;

            return success;
        }

        ~handle() {
            close();
        }
    };

    template <typename T>
    struct _impl_handle_t;

    template <auto CloseFn>
    struct _impl_handle_t<handle<CloseFn>> {
        using type = handle<CloseFn>::type;
    };

    template <typename T>
    using handle_t = _impl_handle_t<T>::type;
}
