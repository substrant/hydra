#pragma once

#include "detail/pch.hpp"

#include "module.hpp"
#include "memory.hpp"
#include "detail/generator.hpp"
#include "handle.hpp"
#include "detail/noncopyable.hpp"
#include "user/window.hpp"

#include "thread.hpp"

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
        unique_handle<CloseHandle> m_handle;
        
        std::unordered_map<std::uintptr_t, std::shared_ptr<remote_module>> m_modules;
        std::vector<std::shared_ptr<remote_module>> m_module_list;

        /* Unsafe constructor */
        explicit process(const HANDLE handle, const bool no_dispose = false) : m_handle(handle, no_dispose) { }

        void init();

        bool scan_linked_modules();

    public:
        // Open a process from an existing handle. The handle will not close on destruction.
        static std::shared_ptr<process> from_handle(HANDLE handle);

        // Open a process from a process ID.
        static std::shared_ptr<process> from_id(DWORD id);

        // Open a process from an existing window.
        static std::shared_ptr<process> from_window(std::string_view name);

        // Open a process from a module name.
        static std::shared_ptr<process> from_module(std::string_view name);

        // Determines if the process handle is valid.
        bool is_valid() const;

        // Implicit cast for HANDLE (get process handle)
        operator HANDLE() const;

        // Implicit cast for DWORD (get process ID)
        operator DWORD() const;

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

        std::size_t mm_read(mem::addr base, const mem::buffer& buffer, std::size_t size = 0) const;

        DWORD mm_protect(mem::addr base, std::size_t size, DWORD new_prot) const;

        std::size_t mm_write(mem::addr base, const mem::buffer& buffer, std::size_t size = 0) const;

        bool mm_query(mem::addr base, MEMORY_BASIC_INFORMATION& mbi) const;

        detail::generator<MEMORY_BASIC_INFORMATION> mm_pages(const mem::buffer& buffer) const;

        std::size_t mm_dump(mem::addr base, const mem::buffer& buffer, dump_context* ctx) const;

        mem::addr mm_alloc(mem::addr base, std::size_t size, DWORD flags, DWORD protect) const;

        mem::addr mm_alloc(std::size_t size, DWORD flags, DWORD protect) const;

        mem::addr mm_alloc(std::size_t size, DWORD protect) const;

        mem::addr mm_inject(const mem::buffer& source, DWORD protect) const;

        bool mm_free(mem::addr base, DWORD flags = MEM_RELEASE) const;

        /* PE functions */

        map_status pe_mmap(pe_image& pe, mapping_info* info_out) const;

        /* Scanning functions */

        std::vector<mem::addr> scan_heap(const std::uint8_t* pattern, const char* mask) const;
    };

    enum class page_action : std::uint8_t {
        stop,
        next,
    };

    using page_cb = std::function<page_action(MEMORY_BASIC_INFORMATION&)>;
}