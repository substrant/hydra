#pragma once

#include <optional>
#include <memory>
#include <coroutine>

namespace hy::detail {
    /* Concepts */

    template <class T>
    concept Void = std::is_same_v<void, T>;

    template <class T>
    concept Primitive =
        std::default_initializable<std::remove_cvref_t<T>>;

    template <class T>
    concept Byte =
        std::same_as<std::remove_cvref_t<T>, std::int8_t> ||
        std::same_as<std::remove_cvref_t<T>, std::uint8_t>;

    template <class T>
    concept IntegralPointer =
        std::integral<std::remove_cvref_t<T>> ||
        std::convertible_to<std::remove_cvref_t<T>, std::uintptr_t>;

    template <class T>
    concept NativePointer =
        std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
        std::same_as<std::remove_cvref_t<T>, std::uintptr_t> ||
        std::is_pointer_v<std::remove_cvref_t<T>>;

    /* Helper classes */

    template <class Base, class Derived>
    class enable_shared : public Base {
    public:
        std::shared_ptr<Derived> shared_from_this() {
            return std::static_pointer_cast<Derived>(Base::shared_from_this());
        }
    };

    template <class T>
    struct ctor_shim : T {
        template <class ...U>
        ctor_shim(U&& ...x) : T(std::forward<U>(x)...) { } // NOLINT: Expects implicit
    };

    struct noncopyable {
        noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    };

    template <typename T>
    struct generator {
        struct promise_type {
            T current;
            std::suspend_always yield_value(T value) {
                current = std::move(value);
                return {};
            }

            generator get_return_object() {
                return generator{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_always initial_suspend() { return { }; }        // NOLINT
            std::suspend_always final_suspend() noexcept { return { }; } // NOLINT
            void return_void() { }                                       // NOLINT
            void unhandled_exception() { std::terminate(); }             // NOLINT
        };

        std::coroutine_handle<promise_type> coro;

        explicit generator(std::coroutine_handle<promise_type> h) : coro(h) { }
        ~generator() { if (coro) coro.destroy(); }

        struct iterator {
            std::coroutine_handle<promise_type> coro;

            iterator& operator++() {
                coro.resume();
                return *this;
            }

            T operator*() const { return coro.promise().current; }

            bool operator==(std::default_sentinel_t) const {
                return !coro || coro.done();
            }
        };

        iterator begin() {
            if (coro) coro.resume();
            return iterator{ coro };
        }

        std::default_sentinel_t end() { return { }; } // NOLINT

        std::optional<T> first() {
            auto it = begin();
            return (it != end()) ? std::optional<T>{ *it } : std::nullopt;
        }
    };

    /// Base structure for extracting function traits
    template <class R, class ...A>
    struct func_traits;

    /// Template override for func_traits that implements logic for basic
    /// function declaration types
    template <class R, class ...A>
    struct func_traits<auto(A...) -> R> {
        using return_t = R;
        using args_t = std::tuple<A...>;

        template <int N>
        using arg_t = std::tuple_element_t<N, args_t>;

        using sig_t = R(A...);
        static constexpr int arity = sizeof...(A);
    };

    /// Template override for func_traits that implements logic for function
    /// pointer types
    template <class R, class ...A>
    struct func_traits<R(*)(A...)> : func_traits<auto(A...) -> R> { };
}
