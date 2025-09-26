#pragma once

#include <memory>

namespace hy::detail {
    template <class Base, class Derived>
    class enable_shared : public Base {
    public:
        std::shared_ptr<Derived> shared_from_this() {
            return std::static_pointer_cast<Derived>(Base::shared_from_this());
        }
    };
}