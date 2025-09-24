#pragma once

namespace hydra::detail {
    struct noncopyable {
        noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    };
}