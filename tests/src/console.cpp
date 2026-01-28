#include "console.hpp"

#include <chrono>
#include <queue>
#include <unordered_set>

static std::unordered_set<std::uint32_t> active_tids;
static std::mutex tid_mutex;

class console_ctx {
    std::uint32_t m_tid = -1u;

public:
    console_ctx() {
        std::uint32_t tid;
        std::scoped_lock lock(tid_mutex);

        for (tid = 0;; tid++) { // TODO: not performant
            if (!active_tids.contains(tid))
                break;
        }

        m_tid = tid;
        active_tids.insert(m_tid);
    }

    ~console_ctx() {
        std::scoped_lock lock(tid_mutex);
        active_tids.erase(m_tid);
    }

    auto tid() const noexcept { return m_tid; }
};

template <bool Busy = false>
struct spinlock {
    std::atomic_flag contended = ATOMIC_FLAG_INIT;

    __forceinline void lock() {
        while (contended.test_and_set(std::memory_order_acquire)) {
            if constexpr (!Busy) _mm_pause();
        }
    }

    __forceinline void unlock() { contended.clear(std::memory_order_release); }
};

template <bool Busy>
struct scoped_spinlock {
    spinlock<Busy>* lock;

    explicit scoped_spinlock(spinlock<Busy>& lock) : lock(&lock) { lock.lock(); }
    ~scoped_spinlock() { lock->unlock(); }
};

namespace console {
    thread_local console_ctx context;
    static spinlock io_lock;

    std::string prefix() {
        using namespace std::chrono;

        const auto now = system_clock::now();
        const auto hms = hh_mm_ss{ floor<seconds>(now.time_since_epoch()) % 24h };

        return std::format("[{:02}] [{:02}:{:02}:{:02}] ",
            context.tid(),
            hms.hours().count(),
            hms.minutes().count(),
            hms.seconds().count()
        );
    }

    void _print(const std::string&& line) {
        const auto pfx = prefix();

        scoped_spinlock lock(io_lock);
        std::cout << pfx << line << "\n";
    }
}
