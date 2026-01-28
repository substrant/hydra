#pragma once

#include <array>
#include <queue>

#include "hydra/memory.hpp"
#include "hydra/handle.hpp"

namespace hy::io {
    
#pragma pack(push, 8)
    // Base properties for circular buffer implementation
    struct peer_ctx {
        std::uintptr_t buffer;              // 0x00
        std::uint64_t size;                 // 0x08
        std::uint64_t write_index = 0ull;   // 0x10
        std::uint64_t read_index = 0ull;    // 0x18

        explicit peer_ctx(const void* p_buffer, const std::size_t p_size)
            : buffer(addr(p_buffer)), size(p_size) { }
    };
#pragma pack(pop)

    class comm_peer {
        handle<CloseHandle> m_shmem;


        std::unique_ptr<peer_ctx> m_context;
        std::queue<region> m_queue;

    protected:
        explicit comm_peer(const std::size_t page_count) {
            const auto buffer_size = page_size * page_count;
            m_shmem = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, buffer_size, nullptr);

            const auto buffer_addr = MapViewOfFile(m_shmem, FILE_MAP_ALL_ACCESS, 0, 0, buffer_size);
            m_context = std::make_unique<peer_ctx>(buffer_addr, buffer_size);
        }

    public:
        static auto create(const std::size_t page_count) {
            return std::make_unique<detail::ctor_shim<comm_peer>>(page_count);
        }

        void send(const region& data) {
            const auto ctx = m_context.get();

            const auto base = reinterpret_cast<std::uint8_t*>(ctx->buffer);
            const auto available = ctx->size - (ctx->write_index - ctx->read_index);

            if (data.size() > available) {
                m_queue.push(data);
                return;
            }

            const std::size_t write_offset = ctx->write_index % ctx->size;
            const std::size_t contiguous = ctx->size - write_offset;

            if (data.size() <= contiguous) {
                std::memcpy(base + write_offset, data.base(), data.size());
            }
            else {
                const std::size_t first_part = contiguous;
                const std::size_t second_part = data.size() - first_part;

                std::memcpy(base + write_offset, data.base(), first_part);
                std::memcpy(base, data.base() + first_part, second_part);
            }

            ctx->write_index += data.size();
        }
    };
}
