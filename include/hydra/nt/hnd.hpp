#pragma once

#include <optional>

#include <hydra/dtl.hpp>
#include <hydra/ost.hpp>

namespace hy::dtl {
    template <typename R>
    concept HndRet = std::same_as<R, void> || std::same_as<R, BOOL> || std::same_as<R, bool> || std::same_as<R, NTSTATUS>;

    template <typename F>
    concept HndCal = requires(typename func_traits<std::remove_cvref_t<F>>::template arg_t<0> h) {
        { std::invoke(std::declval<F>(), h) } -> HndRet;
    };

    template <HndCal auto CloseFn, auto SuccessVal = std::nullopt> /* nullopt for void ret */
    class impl_hnd : noncopyable {
    public:
        using type = func_traits<std::remove_cvref_t<decltype(CloseFn)>>::template arg_t<0>;
        type value = nullptr;

    private:
        bool m_owner = false;

    protected:
        constexpr bool impl_close() {
            if constexpr (std::is_same_v<std::invoke_result_t<decltype(CloseFn), type>, void>) {
                std::invoke(CloseFn, value);
                return true;
                
            }
            else {
                auto raw_result = std::invoke(CloseFn, value);
                static_assert(std::is_same_v<std::invoke_result_t<decltype(CloseFn), type>, decltype(SuccessVal)>, "SuccessVal type does not match close function return type");
                return raw_result == SuccessVal;
            }
        }

    public:
        explicit impl_hnd(const bool no_dispose = false) : m_owner(!no_dispose) { }

        explicit impl_hnd(const type handle, const bool no_dispose = false) : value(handle), m_owner(!no_dispose) { }

        [[nodiscard]] bool not_null() const { return value != nullptr; }

        ACCESS_MASK access() {
            OBJECT_BASIC_INFORMATION info{};
            DWORD info_written;

            if (!NT_SUCCESS(NtQueryObject(
                value,
                ObjectBasicInformation,
                &info,
                sizeof(info),
                &info_written
            )) || info_written != sizeof(info)) return 0;

            return info.GrantedAccess;
        }

        void close() {
            if (!m_owner || !not_null() || !impl_close())
                return;

            value = nullptr;
        }

        void reset(type new_value, const bool no_dispose = false) {
            if (m_owner && not_null())
                impl_close();

            m_owner = !no_dispose;
            value = new_value;
        }

        type operator*() const { return value; }

        operator type() const { return value; } // NOLINT: Expected implicit conversion

        ~impl_hnd() { close(); }
    };

}

namespace hy::nt {
    using hnd = dtl::impl_hnd<NtClose, STATUS_SUCCESS>;
}
