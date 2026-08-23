#pragma once

#include <hydra/blk_stm.hpp>
#include <hydra/proc.hpp>

namespace hy {
    class proc_stm final : public blk_stm {
        proc* m_proc;
        bool m_owner;

    public:
        explicit proc_stm(proc& proc, const blk& block) :
            blk_stm(block),
            m_proc(&proc),
            m_owner(false) { }

        explicit proc_stm(proc& proc, blk&& block) :
            blk_stm(std::move(block)),
            m_proc(&proc),
            m_owner(false) { }

        template <typename T> requires std::derived_from<std::remove_cvref_t<T>, proc> && (!std::is_lvalue_reference_v<T>)
        explicit proc_stm(T&& proc, const blk& block) :
            blk_stm(block),
            m_proc(new std::remove_cvref_t<T>(std::forward<T>(proc))),
            m_owner(true) { }

        template <typename T> requires std::derived_from<std::remove_cvref_t<T>, proc> && (!std::is_lvalue_reference_v<T>)
        explicit proc_stm(T&& proc, blk&& block) :
            blk_stm(std::move(block)),
            m_proc(new std::remove_cvref_t<T>(std::forward<T>(proc))),
            m_owner(true) { }

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

        ~proc_stm() override {
            if (m_owner && m_proc)
                delete m_proc;
        }
    };
}
