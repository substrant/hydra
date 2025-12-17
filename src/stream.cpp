#include "hydra/stream.hpp"
#include "hydra/memory.hpp"

namespace hy {
    std::size_t stream::read(const region &dst) {
        const auto n_bytes = read(dst.base(), dst.size());
        if (m_mode == stream_mode::relative)
            m_offset += n_bytes;
        return n_bytes;
    }

    std::size_t stream::write(const region &src) {
        const auto n_bytes = write(src.base(), src.size());
        if (m_mode == stream_mode::relative)
            m_offset += n_bytes;
        return n_bytes;
    }

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

    std::size_t local_stream::read(std::int8_t* dest, const std::size_t size) const {
        if (!dest || !size)
            return 0;

        std::memcpy(dest, m_buffer.base() + m_offset, size);
        return size;
    }

    std::size_t local_stream::write(std::int8_t* dest, const std::size_t size) const {
        if (!dest || !size)
            return 0;

        std::memcpy(m_buffer.base() + m_offset, dest, size);
        return size;
    }

    const region& local_stream::buffer() const {
        return m_buffer;
    }
}
