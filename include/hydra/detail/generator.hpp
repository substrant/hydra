#pragma once

#include <coroutine>
#include <optional>

namespace hy::detail {
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

            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() { std::terminate(); }
        };

        std::coroutine_handle<promise_type> coro;

        generator(std::coroutine_handle<promise_type> h) : coro(h) {}
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
            return iterator{coro};
        }

        std::default_sentinel_t end() { return {}; }

        std::optional<T> first() {
            auto it = begin();
            return (it != end()) ? std::optional<T>{ *it } : std::nullopt;
        }
    };
}