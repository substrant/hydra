#pragma once

#include <hydra/memory.hpp>
#include <hydra/process.hpp>

#include <thread>
#include <queue>
#include <mutex>
#include <chrono>
#include <future>

struct dump_ctx;

struct dump_params {
    std::shared_ptr<hy::process> proc;
    hy::region src;
    hy::region dst;
};

struct dump_info {
    hy::region src;   // Foreign address ("test.exe")  -> VA 0xA1000-0xA1FFF
    hy::region dst;   // Local address   ("hydra.exe") -> VA 0xB2000-0xB2FFF

    explicit dump_info(hy::region s, hy::region d) : src(std::move(s)), dst(std::move(d)) { }
};

class dump_worker {
    dump_ctx* m_ctx;
    std::uint32_t m_id;

    std::jthread m_thread;

    std::unique_ptr<std::mutex> m_mutex = std::make_unique<std::mutex>();
    std::deque<dump_info> m_queue;

    bool dump_page(const dump_info& info) const;

    bool next_page();

    void entry(std::stop_token token);

public:
    explicit dump_worker(dump_ctx* ctx, const std::uint32_t id) : m_ctx(ctx), m_id(id) {}

    void start();

    void request_stop();

    bool join();

    void enqueue_page(const hy::region& src, const hy::region& dst);

    std::size_t queue_size() const;
};

struct dump_ctx : dump_params {
    std::chrono::steady_clock::time_point last_print = std::chrono::steady_clock::now();

    std::size_t total_pages = 0;
    std::vector<dump_worker> workers;

    explicit dump_ctx(dump_params in_params) {
        // Move parameters into context
        proc = std::move(in_params.proc);
        src = std::move(in_params.src);
        dst = std::move(in_params.dst);
    }

    void print_status() const;

    void update_status();

    void queue_pages();

    void begin_dump(uint32_t n_threads = 0);

    void cancel_dump();

    void wait_dump();
};
