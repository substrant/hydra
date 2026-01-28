#include "dumper.hpp"
#include "console.hpp"

#include <hydra/memory.hpp>

bool dump_worker::dump_page(const dump_info& info) const {
    MEMORY_BASIC_INFORMATION mbi;

    const auto page_base = info.src.base();
    const auto to_read = info.src.size();

    if (!m_ctx->proc->mm_query(page_base, mbi))
        return false;

    if (mbi.Protect & PAGE_NOACCESS) // todo: check comitted
        return false;

    const auto n_bytes = m_ctx->proc->mm_read(page_base, info.dst);

    if (n_bytes != to_read) {
        console::print("Failed to dump page: src=%p dst=%p size=%zu read=%zu",
            page_base, info.dst.base(), to_read,n_bytes
        );

        return false;
    }

    return true;
}

void dump_ctx::print_status() const {
    std::size_t unresolved_pages = 0;

    for (auto& worker : workers)
        unresolved_pages += worker.queue_size();

    const auto resolved_pages = total_pages - unresolved_pages;
    const auto percent_complete = static_cast<double>(resolved_pages) / static_cast<double>(total_pages) * 100;

    console::print("Dumped %.2f%% (%zu/%zu pages)", percent_complete, resolved_pages, total_pages);
}

bool dump_worker::next_page() {
    std::scoped_lock lock(*m_mutex);

    if (m_queue.empty())
        return true; // done

    const auto info = m_queue.front();
    m_queue.pop_front();

    if (!dump_page(info))
        m_queue.push_back(info);

    return false; // not done
}

void dump_ctx::update_status() {
    const auto cur_time = std::chrono::steady_clock::now();
    if (cur_time - last_print >= std::chrono::milliseconds(500)) {
        print_status();
        last_print = cur_time;
    }
}

void dump_worker::entry(const std::stop_token token) {
    // Authority worker
    if (m_id == 1) {
        while (!next_page() && !token.stop_requested()) {
            m_ctx->update_status();
        }
    }

    // Other workers
    else {
        while (!next_page() && !token.stop_requested()) { }
    }
}

void dump_worker::start() {
    // MSVC can fucking suck my cock
    m_thread = std::jthread([this](std::stop_token token) { entry(token); });
}

void dump_worker::request_stop() {
    m_thread.request_stop();
}

bool dump_worker::join() {
    if (!m_thread.joinable())
        return false;

    m_thread.join();
    return true;
}

void dump_worker::enqueue_page(const hy::region& src, const hy::region& dst) {
    m_queue.emplace_back(src, dst);
    m_ctx->total_pages += 1;
}

std::size_t dump_worker::queue_size() const {
    std::scoped_lock lock(*m_mutex);
    return m_queue.size();
}

// todo: not memory efficient but looks nice, make this run in the worker threads
void dump_ctx::queue_pages() {
    auto tid = 0u;

    const auto n_threads = workers.size();
    const auto next_tid = [&] { return tid == n_threads ? tid = 0u : tid++; }; // round-robin

    for (const auto& region_info : proc->mm_regions(src)) {
        const auto region_size = std::min(region_info.RegionSize, (src.end() - region_info.BaseAddress).i);
        const auto region_offset = (region_info.BaseAddress - src.base()).i;

        const hy::region src_bounds(region_info.BaseAddress, region_size);
        const hy::region dst_bounds(dst.base() + region_offset, region_size);

        for (auto chunk_offset = 0ull; chunk_offset < region_size; chunk_offset += hy::page_size) {
            const auto chunk_size = std::min(hy::page_size, region_size - chunk_offset);

            hy::region src_chunk(src_bounds.base() + chunk_offset, chunk_size);
            hy::region dst_chunk(dst_bounds.base() + chunk_offset, chunk_size);

            /*if (chunk_size < hy::page_size) {
                console::print("Partial chunk: src=%p dst=%p size=%zu", src_chunk.base(), dst_chunk.base(), chunk_size);
            }*/

            // sanity check
            if (src_chunk.base() > src.end() || dst_chunk.base() > dst.end())
                throw std::runtime_error("queued page out of source or destination bounds");

            //console::print("Queue page: src=%p dst=%p size=%zu src_end=%p", page_src, page_dst, page_size, src.end());
            workers[next_tid()].enqueue_page(src_chunk, dst_chunk);
        }
    }

    console::print("Total pages: %zu", total_pages);
}

void dump_ctx::begin_dump(std::uint32_t n_threads) {
    // Default to half of hardware concurrency if zero
    if (n_threads == 0)
        n_threads = std::thread::hardware_concurrency() / 2;

    // Initialize worker thread contexts
    for (auto i = 1u; i <= n_threads; i++)
        workers.emplace_back(this, i);

    // Queue pages for dumping
    queue_pages();

    // Start all workers
    for (auto& worker : workers)
        worker.start();
}

void dump_ctx::cancel_dump() {
    for (auto& worker : workers)
        worker.request_stop();
}

void dump_ctx::wait_dump() {
    for (auto& worker : workers)
        if (!worker.join()) throw std::runtime_error("can't join worker thread - duplicate call?");

    print_status();
}
