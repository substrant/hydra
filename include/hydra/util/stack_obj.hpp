#pragma once

#include <new>
#include <utility>
#include <type_traits>

#include "hydra/dtl.hpp"

namespace hy::util {
    template <typename T>
    class stack_ptr;

    template <typename T, typename U>
    concept IsSmartPtrImpl = requires(T t) {
        { t.reset() } -> std::same_as<void>;
        { t.get() } -> std::same_as<U*>;
        { t.operator->() } -> std::same_as<U*>;
        { t.operator*() } -> std::same_as<U&>;
    };

    template <typename T>
    concept SmartPtr = requires {
        typename dtl::template_param<T, 0>;
    } && IsSmartPtrImpl<T, dtl::template_param<T, 0>>;

    template <typename T>
    class stack_obj {
        constexpr auto size = sizeof(T);

        alignas(T) std::byte m_storage[size];
        bool m_init;

    public:
        explicit stack_obj() : m_init(false) { (void)m_storage; }

        template <typename Derived, typename... Args>
        explicit stack_obj(Args&&... args) { emplace<Derived, Args>(std::forward<Args>(args)...); }

        template <typename Derived, typename... Args>
        void emplace(Args&&... args) {
            static_assert(std::is_base_of_v<T, Derived>, "Must derive from T");
            if (m_init) reset();

            ::new (static_cast<void*>(&m_storage)) Derived(std::forward<Args>(args)...);
            m_init = true;
        }

        void reset() {
            if (!m_init) return;

            get()->~T();
            m_init = false;
        }

        T* get() { return reinterpret_cast<T*>(&m_storage); }
        T* operator->() { return get(); }
        T& operator*() { return *get(); }

        explicit operator bool() const { return m_init; }

        ~stack_obj() {
            reset();
        }

        friend class stack_ptr;
    };

    template <typename T>
    class stack_ptr {
        stack_obj<T>* m_object;
        bool m_destroyed;

    public:
        void reset() {
            if (m_destroyed) return;
        }

        ~stack_ptr() {
            if (!m_destroyed && m_object)
                m_object
        }
    };

    static_assert(SmartPtr<stack_ptr<int>>);
    static_assert(SmartPtr<stack_obj<int>>);
}