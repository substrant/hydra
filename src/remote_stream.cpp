#include "hydra/process.hpp"

namespace hy {
    std::size_t remote_stream::read(std::int8_t* dst, const std::size_t size) const {
        if (!dst || !size)
            return 0;

        const auto n_bytes = m_proc->mm_read(m_base + m_offset, region{ dst, size });
        return n_bytes;
    }

    std::size_t remote_stream::write(std::int8_t* src, const std::size_t size) const {
        if (!src || !size)
            return 0;

        const auto n_bytes = m_proc->mm_write(m_base + m_offset, region{ src, size });
        return n_bytes;
    }
}
