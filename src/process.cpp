#include <deque>
#include <unordered_set>
#include <ranges>
#include <hydra/toolhelp.hpp>
#include <chrono>
#include <expected>
#include <hydra/process.hpp>
#include <hydra/syscall.hpp>
#include <phnt_windows.h>
#include <phnt.h>
#include <psapi.h>

namespace hy {
    using hw_clock = std::chrono::high_resolution_clock;

    process process::from_handle(const HANDLE handle) {
        return process(handle, true);
    }

    std::expected<process, NTSTATUS> process::open(const DWORD process_id, const ACCESS_MASK access_mask) {
        HANDLE h_proc;

        CLIENT_ID cid;
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(process_id)); // shitty microsoft design
        cid.UniqueThread = nullptr;

        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);

        const auto status = syscall::NtOpenProcess(&h_proc, access_mask, &attr, &cid);
        if (!NT_SUCCESS(status)) return std::unexpected{ status };

        return std::expected<process, NTSTATUS>{ std::in_place, h_proc };
    }

    std::expected<process, NTSTATUS> process::open(const std::string& name, const ACCESS_MASK access) {
        for (const auto entry : toolhelp::get_processes()) {
            if (entry.szExeFile == name)
                return open(entry.th32ProcessID, access);
        }

        return std::unexpected{ STATUS_OBJECT_NAME_NOT_FOUND };
    }

    PROCESS_BASIC_INFORMATION process::get_info() const {
        PROCESS_BASIC_INFORMATION pbi{};

        // Ignore response we zero out
        syscall::NtQueryInformationProcess(m_handle, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

        return pbi;
    }

    bool process::eumerate_modules() {
        constexpr auto max_modules = 2048u;
        const auto modules_raw = std::make_unique<HMODULE[]>(max_modules);

        DWORD modules_bytes;
        MODULEINFO module_info;

        // Enumerate the modules into modules_raw
        if (!EnumProcessModulesEx(m_handle, modules_raw.get(), sizeof(HMODULE) * max_modules, &modules_bytes, LIST_MODULES_ALL))
            return false;

        const auto module_count = modules_bytes / sizeof(HMODULE);

        char path_buf[MAX_PATH];
        char name_buf[MAX_PATH];

        DWORD path_len;
        DWORD name_len;

        // Preallocate modules
        m_modules.reserve(module_count);

        // Probe for module info
        for (auto i = 0u; i < module_count; i++) {
            const auto module_handle = modules_raw[i];

            if (!GetModuleInformation(m_handle, module_handle, &module_info, sizeof(module_info)))
                continue;

            if ((path_len = GetModuleFileNameExA(m_handle, module_handle, path_buf, MAX_PATH)) == 0) continue;
            if ((name_len = GetModuleBaseNameA(m_handle, module_handle, name_buf, MAX_PATH)) == 0) continue;

            // Prepare information
            const auto base = reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll);

            auto* ptr = &m_module_list.emplace_back(
                this,
                base,
                std::string(name_buf, name_len),
                std::string(path_buf, path_len)
            );

            m_modules.insert(std::make_pair(base, ptr));
        }

        return !m_module_list.empty();
    }

    bool process::kill(const LONG exit_code, NTSTATUS* p_status) {
        NTSTATUS dummy_status;
        if (!p_status) p_status = &dummy_status;

        *p_status = syscall::NtTerminateProcess(m_handle, exit_code);
        m_handle = nullptr;

        return NT_SUCCESS(*p_status);
    }

    std::generator<std::shared_ptr<thread>> process::threads() const {
        for (const auto&& entry : toolhelp::get_threads(*this))
            co_yield thread::open(entry.th32ThreadID);
    }

    bool process::suspend() const {
        return NT_SUCCESS(NtSuspendProcess(m_handle));
    }

    bool process::resume() const {
        return NT_SUCCESS(NtResumeProcess(m_handle));
    }

    std::generator<remote_module&> process::linked_modules() {
        if (!m_modules.empty() || eumerate_modules()) {
            for (auto&& mod : m_module_list)
                co_yield mod;
        }
    }

    std::generator<remote_module> process::unlinked_modules() {
        std::unordered_set<void*> linked_bases;

        // Scan for linked modules
        if (m_modules.empty() && !eumerate_modules())
            co_return;

        // Allocate space and fill bases
        linked_bases.reserve(m_modules.size());
        for (auto&& v : m_modules | std::views::values)
            linked_bases.insert(v->buffer().base());

        for (const auto mbi : mm_regions(umregion)) {
            // Is this memory accessible?
            if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS || mbi.Protect & PAGE_GUARD))
                continue;

            // Allocate data for page and read it
            const auto buffer = region::alloc_local(page_size);
            if (!mm_read(mbi.BaseAddress, buffer)) continue;

            // If bound to PEB, skip
            if (!linked_bases.contains(mbi.BaseAddress))
                continue;

            // This is an unlinked module
            co_yield remote_module(this, mbi.BaseAddress);
        }
    }

    remote_module* process::module() {
        if (m_modules.empty() && !eumerate_modules())
            return nullptr;

        return &m_module_list.front();
    }

    remote_module* process::module(const std::string_view name) {
        if (m_modules.empty() && !eumerate_modules())
            return nullptr;

        // Find our module
        for (auto& v : m_module_list)
            if (v.file_name() == name) return &v;

        return nullptr;
    }

    std::shared_ptr<window> process::owner_window() const {
        const HWND handle = window::find(window::match_owner, *this);
        return handle != nullptr ? std::make_shared<window>(handle) : nullptr;
    }

    std::size_t process::mm_read(const addr base, const region& buffer, std::size_t size) const {
        size = (size == 0) ? buffer.size() : size;
        if (size == 0 || size > buffer.size()) return false;

        SIZE_T read;
        NTSTATUS status = syscall::NtReadVirtualMemory(m_handle, base, buffer.base(), size, &read);

        if (!NT_SUCCESS(status))
            return 0;

        return read;
    }

    bool process::mm_query(const addr base, MEMORY_BASIC_INFORMATION& mbi) const {
        SIZE_T mbi_len;
        return NT_SUCCESS(syscall::NtQueryVirtualMemory(m_handle, base, MemoryBasicInformation, &mbi, sizeof(mbi), &mbi_len));
    }

    std::generator<MEMORY_BASIC_INFORMATION> process::mm_regions(const region& buffer) const {
        MEMORY_BASIC_INFORMATION mbi;
        std::uintptr_t at = buffer.base();

        while (at < buffer.end() && mm_query(at, mbi)) {
            co_yield mbi;
            at += mbi.RegionSize;
        }
    }

    DWORD process::mm_protect(addr base, std::size_t size, const DWORD new_prot) const {
        DWORD old_prot;
        if (!(m_access & PROCESS_VM_OPERATION)) return 0;
        return NT_SUCCESS(syscall::NtProtectVirtualMemory(
            m_handle,
            &base.u,
            &size,
            new_prot,
            &old_prot
        )) ? old_prot : 0;
    }

    std::optional<region> process::mm_alloc(addr base, std::size_t size, const DWORD flags, const DWORD protect) const {
        if (!(m_access & PROCESS_VM_OPERATION)) return std::nullopt;
        return NT_SUCCESS(syscall::NtAllocateVirtualMemory(
            m_handle,
            &base.u,
            0, // Anywhere in user VA space
            &size,
            flags,
            protect
        )) ? std::make_optional<region>(base, size) : std::nullopt;
    }

    std::optional<region> process::mm_alloc(const std::size_t size, const DWORD flags, const DWORD protect) const {
        return mm_alloc(nullptr, size, flags, protect);
    }

    std::optional<region> process::mm_alloc(const std::size_t size, const DWORD protect) const {
        return mm_alloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
    }

    std::size_t process::mm_write(const addr base, const region& buffer, std::size_t size) const {
        if (size == 0)
            size = buffer.size();

        if (buffer.size() != 0 && size > buffer.size())
            return 0;

        if (!(m_access & PROCESS_VM_WRITE) || !(m_access & PROCESS_VM_OPERATION))
            return 0;

        SIZE_T written;
        NTSTATUS status = syscall::NtWriteVirtualMemory(
            m_handle,
            base,
            buffer.base(),
            size,
            &written);

        return NT_SUCCESS(status) ? written : 0;
    }

    bool process::mm_free(const addr base) const {
        if (!(m_access & PROCESS_VM_OPERATION)) return false;

        SIZE_T size = 0;
        const auto status = syscall::NtFreeVirtualMemory(m_handle, const_cast<PPVOID>(&base.u), &size, MEM_RELEASE);

        return NT_SUCCESS(status);
    }

    bool process::mm_decommit(const region& region) const {
        SIZE_T size = region.size();
        return NT_SUCCESS(syscall::NtFreeVirtualMemory(m_handle, const_cast<PPVOID>(&region.m_base.u), &size, MEM_DECOMMIT));
    }

    addr process::mm_inject(const region& buffer, const DWORD protect) const {
        const auto base = mm_alloc(nullptr, buffer.size(), MEM_COMMIT | MEM_RESERVE, protect).value_or({ nullptr });
        if (base.base() == nullptr) return nullptr;

        if (!mm_write(base, buffer)) {
            (void)mm_free(base);
            return nullptr;
        }

        return base;
    }

    // todo: might not be the best spot to put this. perhaps make a memory manager class?
    std::vector<addr> process::mm_heapscan(const std::uint8_t* pattern, const char* mask) const {
        std::vector<addr> results;
        MEMORY_BASIC_INFORMATION mbi;
        auto time = std::chrono::system_clock::now();

        for (std::uintptr_t addr = umregion.base(); addr < umregion.end(); ) {
            if (!mm_query(addr, mbi)) {
                addr += page_size;
                continue;
            }

            const auto elapsed = std::chrono::system_clock::now() - time;
            if (elapsed > std::chrono::milliseconds(1000))
                time = std::chrono::system_clock::now();

            const bool heap_like =
                mbi.State == MEM_COMMIT &&
                mbi.Type & MEM_PRIVATE &&
                (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE);

            if (!heap_like) {
                addr = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
                continue;
            }

            const auto dump = region::alloc_local(mbi.RegionSize);
            if (mm_read(mbi.BaseAddress, dump)) {
                for (const auto match : dump.scan_aob(pattern, mask)) {
                    const auto offset = match - dump.base();
                    results.emplace_back(hy::addr(mbi.BaseAddress) + offset);
                }
            }

            addr = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }

        return results;
    }
}
