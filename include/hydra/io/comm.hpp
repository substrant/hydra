#pragma once

#include <array>
#include <queue>
#include <unordered_set>

#include "hydra/memory.hpp"
#include "hydra/handle.hpp"

#include "hydra/remote/stream.hpp"
#include "hydra/local/stream.hpp"

namespace hy::io {
    // comm uses raw byte stream to communicate between server and client.
    // i recommend that we use a stream cipher + ring buffer for this.

    /*
     * Hydra I/O Communication Standard (hy::io::comm_peer)
     * ----------------------------------------------------
     *
     * The communication system is based on a shared-memory circular buffer architecture, allowing multiple peers to
     * exchange raw byte streams efficiently. Each peer can both send and receive data, and all synchronization is
     * achieved using atomic operations to ensure lock-free, deterministic behavior.
     *
     * 1. Peer Model
     *    1.1. There is exactly one master peer. The master is responsible for:
     *         - Creating the shared memory region (context + buffer)
     *         - Initializing and maintaining the peer registry
     *         - Allocating global indices and buffer metadata
     *         - Implementing round-robin scheduling for fair buffer access
     *    1.2. All other peers are considered clients.
     *         - Clients map existing shared regions created by the master.
     *         - Clients must validate the magic values in the master and peer contexts before participating.
     *
     * 2. Memory Layout
     *    2.1. The shared memory is composed of two regions:
     *         | Region          | Description                                      |
     *         |-----------------|--------------------------------------------------|
     *         | master_ctx      | Master metadata block                            |
     *         | first_peer      | Master peer context                              |
     *    2.2. Alignment Requirements:
     *         - master_ctx: aligned to 8 bytes
     *         - peer_ctx:   aligned to 8 bytes
     *         - buffer:     aligned to PAGE_SIZE
     *
     * 3. Master Context
     *    3.1. The master_ctx structure contains the following fields:
     *         | Field          | Type          | Description                                            |
     *         |----------------|---------------|--------------------------------------------------------|
     *         | magic          | u32           | Magic value for validation ('cMyH' -> 'hy master ctx') |
     *         | max_peers      | u32           | Maximum number of peers supported                      |
     *         | peers          | HANDLE        | Handle to peer context shared memory                   |
     *    3.2. Peers will connect to the master peer using the 'master' handle to retrieve the peer contexts.
     *    3.3. The zeroth index of peer_ctx region is always the master peer itself.
     *
     * 3. Peer Context
     *    3.1. Each peer_ctx structure contains the following fields:
     *         | Field        | Type          | Description                                              |
     *         |--------------|---------------|----------------------------------------------------------|
     *         | magic        | u32           | Magic value for validation ('cPyH' -> 'hy peer ctx')     |
     *         | id           | u32           | Unique peer identifier (reflects master block)           |
     *         | buffer       | HANDLE        | Handle to shared memory                                  |
     *         | size         | u64           | Size of the circular buffer                              |
     *         | read_index   | u64           | Global read index for the peer                           |
     *         | write_index  | u64           | Global write index for the peer                          |
     *         | flags        | u32           | Peer status flags (e.g., 'ready'')                       |
     *         | target       | u32           | Target peer ID for sending data (UINT32_MAX init/none)   |
     *    3.2. The read_index and write_index are used to manage the circular buffer.
     *         - read_index:  Next byte to read from the buffer.
     *         - write_index: Next byte to write to the buffer.
     *    3.3. Both indices are global across all peers, ensuring consistent data ordering.
     *
     * 6. Round-Robin Scheduling
     *    6.1. To ensure fair access to the shared buffer, a round-robin scheduling mechanism is implemented on the
     *         master peer. Each peer maintains a local queue of pending send operations and processes them in order.
     *    6.2. When a peer attempts to send data:
     *         - It signals that the queue is non-empty by setting the 'ready' flag.
     *         - The master peer will scan through peers repeatedly looking for ready peers.
     *    6.3. When the master peer finds a ready peer:
     *         - The master loads the target peer's buffer into virtual memory lazily.
     *         - It checks if there is sufficient space in the buffer by ensuring that write_index != read_index.
     *           - If write_index == read_index, the buffer is full, and the master skips to the next peer.
     *
     *
     * 5. Data Transfer
     *    5.1. Finding existing peers:
     *         - A peer connects to the master context to retrieve the list of active peers.
     *         - It then decides which peer to communicate with based on application logic.
     *    5.1. Sending Data to peers:
     *         - A peer maps the buffer to virtual memory.
     *         - It checks that write_index != read_index to determine available space. If write_index equals
     *           read_index, the buffer is full and the peer must wait its turn.
     */


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
