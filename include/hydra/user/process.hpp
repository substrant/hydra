#pragma once

#include <phnt_windows.h>
#include <phnt.h>
#include <Psapi.h>

#include <stdexcept>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <ranges>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>

#include "hydra/util/module.hpp"
#include "hydra/util/memory.hpp"
#include "hydra/detail/generator.hpp"
#include "hydra/util/handle.hpp"
#include "hydra/detail/noncopyable.hpp"
#include "hydra/user/window.hpp"

#include "phnt_ntdef.h"
#include <ntpsapi.h>

#include "mapping.h"
#include "thread.hpp"

// Forward-decl
namespace hydra {
    class process;
    class memdump_ctx;

    template <class T>
    concept is_process = std::is_class_v<T>; 

    class process : public detail::noncopyable, public std::enable_shared_from_this<process> {
        std::shared_ptr<process> _self = nullptr;
        unique_handle<CloseHandle> _handle;

        std::unordered_map<std::uintptr_t, std::shared_ptr<pe_module>> _modules;
        std::vector<std::shared_ptr<pe_module>> _modules_list;

        /* Unsafe constructor */
        explicit process(const HANDLE handle, const bool no_dispose = false) : _handle(handle, no_dispose) { }

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

        detail::generator<std::shared_ptr<pe_module>> linked_modules();

        detail::generator<std::shared_ptr<pe_module>> unlinked_modules();

        std::shared_ptr<pe_module> module(const std::optional<std::string>& name = std::nullopt);

        std::shared_ptr<window> main_window() const;

        /* Memory functions */

        std::size_t mm_read(mem::addr base, const mem::buffer& buffer, std::size_t size = 0) const;

        std::size_t mm_read(mem::addr base, mem::addr buffer, std::size_t size = 0) const;

        bool mm_query(mem::addr base, MEMORY_BASIC_INFORMATION& mbi) const;

        detail::generator<MEMORY_BASIC_INFORMATION> mm_pages(const mem::buffer& buffer) const;

        DWORD mm_protect(mem::addr base, std::size_t size, DWORD new_prot) const;

        std::size_t mm_dump(memdump_ctx params) const;

        std::vector<mem::addr> mm_scan_heap(const std::uint8_t* pattern, const char* mask) const;

        mem::addr mm_alloc(mem::addr base, std::size_t size, DWORD flags, DWORD protect) const;

        mem::addr mm_alloc(std::size_t size, DWORD flags, DWORD protect) const;

        mem::addr mm_alloc(std::size_t size, DWORD protect) const;

        std::size_t mm_write(mem::addr base, const mem::buffer& buffer, std::size_t size = 0) const;

        mem::addr mm_inject(const mem::buffer& source, DWORD protect) const;

        bool mm_free(mem::addr base, DWORD flags = MEM_RELEASE) const;

        /* PE functions */

        map_status pe_mmap(pe_image& pe, mapping_info* info_out) const;
    };

    enum class page_action : std::uint8_t {
        stop,
        next,
    };

    using page_cb = std::function<page_action(MEMORY_BASIC_INFORMATION&)>;

    class memdump_ctx {
    public:
        friend class process;

        // Required information for dumping
        mem::addr base;
        mem::addr dest;
        std::size_t size;
        std::uint8_t sentinel = 0xCC; // Fill unread memory with this byte. int 3 (0xCC) is good for decoder.
        std::atomic<bool>* stop_flag = const_cast<std::atomic<bool>*>(&dummy_flag);

        explicit memdump_ctx(const mem::addr base, const mem::buffer& buffer)
            : base(base), dest(buffer.data()), size(buffer.size()) {

            if (base.i % mem::page_size != 0)
                throw std::runtime_error("Misaligned base address");
        }

        ~memdump_ctx() {
            // Reset stop flag
            *stop_flag = false;
        }

        std::size_t page_count() const { return size / mem::page_size; }

        mem::addr end_addr() const { return base + size; }

        bool stop_requested() const { return *stop_flag; }

        void reset_flag() const { *stop_flag = false; }

        void set_handler(const std::function<void(memdump_ctx&, std::size_t)>& handler, const std::chrono::milliseconds interval) {
            if (_report_handler.has_value())
                throw std::logic_error("Cannot overwrite immutable handler");

            _report_handler = handler;
            _report_interval = interval;
        }

    private:
        // Feedback and debugging
        std::chrono::milliseconds _report_interval = std::chrono::milliseconds::max();
        std::optional<std::function<void(memdump_ctx&, std::size_t)>> _report_handler = std::nullopt;

        static const std::atomic<bool> dummy_flag;
    };
}