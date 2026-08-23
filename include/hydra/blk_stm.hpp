#pragma once

#include <cstring>
#include <algorithm>

#include <hydra/blk.hpp>
#include <hydra/stm.hpp>

namespace hy {
    class blk_stm : public blk, public stm {
        inline void construct() {
            position = base;
        }

    public:
        using stm::read;
        using stm::write;

        explicit blk_stm(const blk& block)  : blk(block)            { construct(); }
        explicit blk_stm(blk&& block)       : blk(std::move(block)) { construct(); }

        explicit blk_stm(auto&&... args) requires std::constructible_from<blk, decltype(args)...>
            : blk(std::forward<decltype(args)>(args)...) { construct(); }

        [[nodiscard]] std::size_t read(std::int8_t* dst, const std::size_t n) override {
            const auto remaining = size - std::min((base + size).i, position);
            const auto count = std::min(n, remaining);

            std::memcpy(dst, ptr(position), count);
            return count;
        }

        [[nodiscard]] std::size_t write(const std::int8_t* src, const std::size_t n) override {
            const auto remaining = size - std::min((base + size).i, position);
            const auto count = std::min(n, remaining);

            std::memcpy(ptr(position), src, count);
            return count;
        }

        std::size_t seek(const stm_origin origin, const std::make_signed_t<std::size_t> offset = 0) override {
            std::size_t anchor = 0;

            switch (origin) {
            case stm_origin::begin:
                anchor = position = base;
                break;
            case stm_origin::current:
                anchor = position;
                break;
            case stm_origin::end:
                anchor = size;
                break;
            }

            position = anchor + offset;
            return position;
        }
    };
}
