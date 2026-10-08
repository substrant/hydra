#pragma once

#include <hydra/blkstm.hpp>
#include <hydra/proc.hpp>

namespace hy {
    class procstm final : public blkstm {
        proc* m_proc;

    public:
        explicit procstm(proc& proc, const blk& block) :
            blkstm(block),
            m_proc(&proc) { }

        explicit procstm(proc& proc, blk&& block) :
            blkstm(std::move(block)),
            m_proc(&proc) { }

        std::unique_ptr<stm> clone_move() override;

        [[nodiscard]] std::size_t read(std::int8_t* dst, const std::size_t n) override {
            const auto end = (base + size).i;
            const auto remaining = end - std::min(end, position);

            const auto count = std::min(n, remaining);
            return m_proc->mm_read(dst, ptr(position), count);
        }

        [[nodiscard]] std::size_t write(const std::int8_t* src, const std::size_t n) override {
            const auto end = (base + size).i;
            const auto remaining = end - std::min(end, position);

            const auto count = std::min(n, remaining);
            return m_proc->mm_write(ptr(position), src, count);
        }
    };
}
