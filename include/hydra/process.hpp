#pragma once

#include <functional>

#include <hydra/detail.hpp>
#include <hydra/handle.hpp>
#include <hydra/module.hpp>
#include <hydra/thread.hpp>
#include <hydra/window.hpp>

#include "hydra/remote_stream.hpp" // NOLINT

namespace hy {
    // Forward-decl
    class process;
   
    class process : public detail::noncopyable, public std::enable_shared_from_this<process> {
        std::shared_ptr<process> m_this = nullptr;

        handle<CloseHandle> m_handle;
        ACCESS_MASK m_access;
        
        std::unordered_map<std::uintptr_t, remote_module*> m_modules;
        std::list<remote_module> m_module_list;
        
        // Initialize the process object
        std::shared_ptr<process> init();

        // Enumerate PEB for linked modules to the process
        bool eumerate_modules();

        // Query basic information about the process using NTAPI
        PROCESS_BASIC_INFORMATION get_info() const;

    protected:
        HYDRA_INTERNAL("Use 'process::open' to open processes.")
        explicit process(const HANDLE handle, const bool no_dispose = false)
            : m_handle(handle, no_dispose), m_access(m_handle.access()) { }

    public:
        // Open a process from an existing handle. The handle will not close on destruction.
        static std::shared_ptr<process> from_handle(HANDLE handle);

        // Open a process from a process ID.
        static std::shared_ptr<process> open(DWORD id, ACCESS_MASK access = PROCESS_ALL_ACCESS);

        // Open a process from a module name.
        static std::shared_ptr<process> open(const std::string& name, ACCESS_MASK access = PROCESS_ALL_ACCESS);

        // Determines if the process handle is valid.
        bool is_valid() const { return m_handle.is_valid(); }

        // Get handle to process
        HANDLE handle() const { return m_handle; }      // NOLINT

        // Implicit cast for HANDLE (get process handle)
        operator HANDLE() const { return handle(); }    // NOLINT

        // Get process ID from PBI using NTAPI
        DWORD id() const {
            return static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(get_info().UniqueProcessId));
        }

        // Implicit cast for DWORD (get process ID)
        operator DWORD() const {                        // NOLINT
            return m_handle.is_valid() ? GetProcessId(m_handle) : -1;
        }

        /* Execution functions */

        std::generator<std::shared_ptr<thread>> threads() const;

        bool suspend() const;

        bool resume() const;

        bool kill(LONG exit_code = 0, NTSTATUS* p_status = nullptr);

        /* Instrumentation functions */

        // Returns modules in PEB load order
        std::generator<remote_module&> linked_modules();

        // Returns in memory order
        std::generator<remote_module> unlinked_modules();

        remote_module& module();

        remote_module* module(std::string_view name);

        std::shared_ptr<window> owner_window() const;

        /* Low-level memory management */

        std::size_t mm_read(addr base, const region& buffer, std::size_t size = 0) const;

        DWORD mm_protect(addr base, std::size_t size, DWORD new_prot) const;

        std::size_t mm_write(addr base, const region& buffer, std::size_t size = 0) const;

        bool mm_query(addr base, MEMORY_BASIC_INFORMATION& mbi) const;

        std::generator<MEMORY_BASIC_INFORMATION> mm_regions(const region& buffer) const;

        addr mm_alloc(addr base, std::size_t size, DWORD flags, DWORD protect) const;

        addr mm_alloc(std::size_t size, DWORD flags, DWORD protect) const;

        addr mm_alloc(std::size_t size, DWORD protect) const;

        bool mm_free(addr base) const;

        bool mm_decommit(const region& region) const;

        /* Advanced memory management */

        addr mm_inject(const region& buffer, DWORD protect) const;

        std::vector<addr> mm_heapscan(const std::uint8_t* pattern, const char* mask) const;
    };
}