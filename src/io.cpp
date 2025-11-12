#include "../../include/hydra/mem/core.hpp"
#include <hydra/io/memory.hpp>
#include <hydra/sys/process.hpp>

namespace hy {
    addr memory_stream::base() const {
        return nullptr;
    }

    std::size_t memory_stream::seek(const std::size_t offset, const io_origin origin) {
        std::size_t new_pos;
        switch (origin) {
            case io_origin::begin:
                new_pos = offset;
                break;
            case io_origin::current:
                new_pos = this->offset + offset;
                break;
            default:
                throw std::runtime_error("Invalid seek origin");
        }

        this->offset = new_pos;
        return this->offset;
    }

    addr local_stream::base() const {
        return m_buffer.base();
    }

    std::size_t local_stream::read_impl(std::uint8_t* base, const std::size_t size) {
        if (!base || !size)
            return 0;

        std::memcpy(base, m_buffer.base() + offset, size);
        return size;
    }

    std::size_t local_stream::write_impl(std::uint8_t* base, const std::size_t size) {
        if (!base || !size)
            return 0;

        std::memcpy(m_buffer.base() + offset, base, size);
        return size;
    }

    const buffer& local_stream::buffer() const {
        return m_buffer;
    }

    addr remote_stream::base() const {
        return m_base;
    }

    std::size_t remote_stream::read_impl(std::uint8_t* base, const std::size_t size) {
        if (!base || !size)
            return 0;

        const auto n_bytes = m_proc->mm_read(base + offset, buffer{base, size});
        return n_bytes;
    }

    std::size_t remote_stream::write_impl(std::uint8_t* base, const std::size_t size) {
        if (!base || !size)
            return 0;

        const auto n_bytes = m_proc->mm_write(m_base + offset, buffer{base, size});
        return n_bytes;
    }
}
