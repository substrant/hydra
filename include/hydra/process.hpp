#pragma once

#include <functional>
#include <chrono>

#include <hydra/detail.hpp>
#include <hydra/handle.hpp>
#include <hydra/module.hpp>
#include <hydra/thread.hpp>
#include <hydra/window.hpp>

namespace hy {
    // Forward-decl
    class process;

    struct mapping_info {
        void* base = nullptr;
        size_t size = 0;
        size_t header_size = 0;
    };

    struct dump_context {
        std::uint8_t sentinel = 0;
        std::optional<std::function<void(const dump_context*, std::size_t)>> callback = std::nullopt;
        std::atomic<bool> stop = false;
        double clear_ratio = 1.0;
        std::optional<std::chrono::milliseconds> clear_time;
    };

    class process : public detail::noncopyable, public std::enable_shared_from_this<process> {
        std::shared_ptr<process> m_this = nullptr;
        handle<CloseHandle> m_handle;
        
        std::unordered_map<std::uintptr_t, std::shared_ptr<remote_module>> m_modules;
        std::vector<std::shared_ptr<remote_module>> m_module_list;

        /* Unsafe constructor */
        explicit process(const HANDLE handle, const bool no_dispose = false) : m_handle(handle, no_dispose) { }

        void init();

        bool scan_linked_modules();

    public:
        // Open a process from an existing handle. The handle will not close on destruction.
        static std::shared_ptr<process> open(HANDLE handle);

        // Open a process from a process ID.
        static std::shared_ptr<process> open(DWORD id);

        // Open a process from a module name.
        static std::shared_ptr<process> open(std::string_view name);

        // Determines if the process handle is valid.
        bool is_valid() const;

        // Implicit cast for HANDLE (get process handle)
        operator HANDLE() const; // NOLINT

        // Implicit cast for DWORD (get process ID)
        operator DWORD() const; // NOLINT

        /* Execution functions */

        detail::generator<std::shared_ptr<thread>> threads() const;

        void suspend() const;

        void resume() const;

        bool kill(LONG exit_code = 0, NTSTATUS* p_status = nullptr);

        /* Instrumentation functions */

        detail::generator<std::shared_ptr<remote_module>> linked_modules();

        detail::generator<std::shared_ptr<remote_module>> unlinked_modules();

        std::shared_ptr<remote_module> module(const std::optional<std::string>& name = std::nullopt);

        std::shared_ptr<window> main_window() const;

        /* Memory functions */

        std::size_t mm_read(addr base, const region& buffer, std::size_t size = 0) const;

        DWORD mm_protect(addr base, std::size_t size, DWORD new_prot) const;

        std::size_t mm_write(addr base, const region& buffer, std::size_t size = 0) const;

        bool mm_query(addr base, MEMORY_BASIC_INFORMATION& mbi) const;

        detail::generator<MEMORY_BASIC_INFORMATION> mm_pages(const region& buffer) const;

        std::size_t mm_dump(addr base, const region& buffer, dump_context* ctx) const;

        addr mm_alloc(addr base, std::size_t size, DWORD flags, DWORD protect) const;

        addr mm_alloc(std::size_t size, DWORD flags, DWORD protect) const;

        addr mm_alloc(std::size_t size, DWORD protect) const;

        addr mm_inject(const region& buffer, DWORD protect) const;

        bool mm_free(addr base, DWORD flags = MEM_RELEASE) const;

        /* PE functions */

        map_status pe_mmap(pe_image& pe, mapping_info* info_out) const;

        /* Scanning functions */

        std::vector<addr> scan_heap(const std::uint8_t* pattern, const char* mask) const;
    };

    class process_stream final : public detail::mem_stream_impl {
    protected:
        std::shared_ptr<process> m_proc;
        addr m_base;

        explicit process_stream(const std::shared_ptr<process>& proc, const addr base)
            : m_proc(proc), m_base(base) { }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read_impl(std::uint8_t* base, std::size_t size) override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write_impl(std::uint8_t* base, std::size_t size) override;

    public:
        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~process_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        addr base() const override;
    };

    enum class page_action : std::uint8_t {
        stop,
        next,
    };

    using page_cb = std::function<page_action(MEMORY_BASIC_INFORMATION&)>;

    /// Memory-based stream implementation using hydra::process.
    /// Provides in-memory streaming operations.
    class remote_stream final : public detail::mem_stream_impl {
    protected:
        std::shared_ptr<process> m_proc;
        addr m_base;

        explicit remote_stream(const std::shared_ptr<process>& proc, const addr base)
            : m_proc(proc), m_base(base) { }

        /// Reads data from stream into buffer.
        /// Returns number of bytes actually read.
        std::size_t read_impl(std::uint8_t* base, std::size_t size) override;

        /// Writes data from buffer to stream.
        /// Returns number of bytes actually written.
        std::size_t write_impl(std::uint8_t* base, std::size_t size) override;

    public:
        /// Destructor for memory_stream.
        /// Cleans up stream resources.
        ~remote_stream() override = default;

        /// Get the base address of the underlying memory.
        /// This memory is always remote.
        addr base() const override;
    };
}