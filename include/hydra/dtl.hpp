#pragma once

#include <memory>

#ifdef HY_LIBRARY
#    define HYDRA_INTERNAL(x)
#else
#    define HYDRA_INTERNAL(x) [[deprecated("Internal Hydra symbol: " x)]]
#endif

namespace hy::dtl {
    /* Concepts */

    template <class T>
    concept Void = std::is_same_v<void, T>;

    template <class T>
    concept Primitive =
        std::default_initializable<std::remove_cvref_t<T>>;

    template <class T>
    concept Byte =
        std::same_as<std::remove_cvref_t<T>, std::int8_t> ||
        std::same_as<std::remove_cvref_t<T>, std::uint8_t> ||
        std::same_as<std::remove_cvref_t<T>, char>;

    /* Helper classes */

    template <class Base, class Derived>
    class enable_shared : public Base {
    public:
        std::shared_ptr<Derived> shared_from_this() {
            return std::dynamic_pointer_cast<Derived>(Base::shared_from_this());
        }
    };

    template <class T>
    struct ctor_shim : T {
        template <class ...U>
        ctor_shim(U&& ...x) : T(std::forward<U>(x)...) { } // NOLINT: Expects implicit
    };

    struct noncopyable { // NOLINT
        noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
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

    template <class T>
    struct clone_moveable {
        virtual std::unique_ptr<T> clone_move() = 0;
    };

    template <typename T, std::size_t N>
    struct template_param_impl;

    template <template <typename...> class C, typename... Args, std::size_t N>
    struct template_param_impl<C<Args...>, N> {
        using type = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    template <typename T, std::size_t N>
    using template_param = typename template_param_impl<T, N>::type;
}
