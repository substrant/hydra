#include <hydra/memory.hpp>
#include <hydra/process.hpp>

namespace hy {
    addr memory_stream::base() const {
        return nullptr;
    }

    std::size_t memory_stream::seek(const std::int64_t offset, const stream_origin origin) {
        std::size_t new_pos;

        switch (origin) {
            case stream_origin::begin:
                new_pos = offset;
                break;
            case stream_origin::current:
                new_pos = m_offset + offset;
                break;
            default:
                throw std::runtime_error("Invalid seek origin");
        }

        m_offset = new_pos;
        return m_offset;
    }

    addr local_stream::base() const {
        return m_buffer.base();
    }

    std::size_t local_stream::read_impl(std::uint8_t* base, const std::size_t size) const {
        if (!base || !size)
            return 0;

        std::memcpy(base, m_buffer.base() + m_offset, size);
        return size;
    }

    std::size_t local_stream::write_impl(std::uint8_t* base, const std::size_t size) const {
        if (!base || !size)
            return 0;

        std::memcpy(m_buffer.base() + m_offset, base, size);
        return size;
    }

    const region& local_stream::buffer() const {
        return m_buffer;
    }

    addr remote_stream::base() const {
        return m_base;
    }

    std::size_t remote_stream::read_impl(std::uint8_t* base, const std::size_t size) const {
        if (!base || !size)
            return 0;

        const auto n_bytes = m_proc->mm_read(base + m_offset, region{base, size});
        return n_bytes;
    }

    std::size_t remote_stream::write_impl(std::uint8_t* base, const std::size_t size) const {
        if (!base || !size)
            return 0;

        const auto n_bytes = m_proc->mm_write(m_base + m_offset, region{base, size});
        return n_bytes;
    }
}
