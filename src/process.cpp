#include <deque>
#include <unordered_set>
#include <ranges>
#include <hydra/toolhelp.hpp>
#include <chrono>
#include <hydra/process.hpp>
#include <phnt_windows.h>
#include <phnt.h>
#include <psapi.h>

namespace hy {
    using hw_clock = std::chrono::high_resolution_clock;

    std::shared_ptr<process> process::init() {
        if (!m_handle.is_valid()) throw std::runtime_error("Invalid handle provided");
        return m_this = shared_from_this();
    }

    std::shared_ptr<process> process::open(const HANDLE handle) {
        OBJECT_BASIC_INFORMATION info{};
        DWORD info_written;

        if (!NT_SUCCESS(NtQueryObject(
            handle,
            ObjectBasicInformation,
            &info,
            sizeof(info),
            &info_written
        )) || info_written != sizeof(info)) return nullptr;

        const auto proc = std::make_shared<detail::ctor_shim<process>>(handle, info.GrantedAccess, true);
        return proc->init();
    }

    std::shared_ptr<process> process::open(const DWORD id, const DWORD access) {
        HANDLE h_proc;

        CLIENT_ID cid;
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(id)); // shitty microsoft design 420
        cid.UniqueThread = nullptr;

        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);

        // Obtain handle to process using NTAPI
        if (!NT_SUCCESS(NtOpenProcess(&h_proc, access, &attr, &cid)))
            return nullptr;

        return open(h_proc);
    }

    std::shared_ptr<process> process::open(const std::string_view name, const DWORD access) {
        for (const auto& entry : toolhelp::get_processes()) {
            if (entry.szExeFile == name)
                return open(entry.th32ProcessID, access);
        }

        return nullptr;
    }

    PROCESS_BASIC_INFORMATION process::get_info() const {
        PROCESS_BASIC_INFORMATION pbi{};

        // Ignore response we zero out
        NtQueryInformationProcess(m_handle, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr);

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

        char path_buf[MAX_PATH + 1]{};
        char name_buf[MAX_PATH + 1]{};

        DWORD path_len;
        DWORD name_len;

        // Preallocate modules
        m_modules.reserve(module_count);

        // Probe for module info
        for (auto i = 0u; i < module_count; i++) {
            const auto module_handle = modules_raw[i];

            // Get the module's information
            if (!GetModuleInformation(m_handle, module_handle, &module_info, sizeof(module_info)))
                continue;

            // Get the path of the module
            if ((path_len = GetModuleFileNameExA(m_handle, module_handle, path_buf, MAX_PATH)) == 0)
                continue;

            // Get the module name only (strip path)
            if ((name_len = GetModuleBaseNameA(m_handle, module_handle, name_buf, MAX_PATH)) == 0)
                continue;

            // Prepare information
            const auto base = reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll);
            const auto ptr = remote_module::from_header(
                m_this,
                base,
                std::string(name_buf, name_len),
                std::string(path_buf, path_len)
            );

            // Push module to list and dict
            m_module_list.push_back(ptr);
            m_modules.insert(std::make_pair(base, ptr));
        }

        return !m_module_list.empty();
    }

    bool process::kill(const LONG exit_code, NTSTATUS* p_status) {
        NTSTATUS dummy_status;
        if (!p_status) p_status = &dummy_status;

        *p_status = NtTerminateProcess(m_handle, exit_code);
        m_handle = nullptr;

        return NT_SUCCESS(*p_status);
    }

    std::generator<std::shared_ptr<thread>> process::threads() const {
        for (const auto&& entry : toolhelp::get_threads(*this))
            co_yield std::shared_ptr(thread::from_id(entry.th32ThreadID));
    }

    bool process::suspend() const {
        return NT_SUCCESS(NtSuspendProcess(m_handle));
    }

    bool process::resume() const {
        return NT_SUCCESS(NtResumeProcess(m_handle));
    }

    std::generator<std::shared_ptr<remote_module>> process::linked_modules() {
        if (m_modules.empty() && !eumerate_modules())
            throw std::runtime_error("Failed to enumerate linked modules");

        for (auto&& v : m_module_list)
            co_yield v;
    }

    std::generator<std::shared_ptr<remote_module>> process::unlinked_modules() {
        std::unordered_set<void*> linked_bases;

        // Scan for linked modules
        if (m_modules.empty() && !eumerate_modules())
            throw std::runtime_error("Failed to enumerate linked modules");

        // Allocate space and fill bases
        linked_bases.reserve(m_modules.size());
        for (auto&& v : m_modules | std::views::values)
            linked_bases.insert(v->buffer().base());

        for (const auto mbi : mm_pages(um_region)) {
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
            co_yield remote_module::from_header(m_this, mbi.BaseAddress);
        }
    }

    std::shared_ptr<remote_module> process::module(const std::optional<std::string>& name) {
        // Scan for linked modules
        if (m_modules.empty() && !eumerate_modules())
            throw std::runtime_error("Failed to enumerate linked modules");

        if (name == std::nullopt)
            return m_module_list[0]; // first always

        // Find our module
        for (const auto& v : m_module_list)
            if (v->file_name() == name) return v;

        return nullptr;
    }

    std::shared_ptr<window> process::main_window() const {
        const HWND handle = window::find(window::match_owner, *this);
        return handle != nullptr ? std::make_shared<window>(handle) : nullptr;
    }

    std::size_t process::mm_read(const addr base, const region& buffer, std::size_t size) const {
        size = (size == 0) ? buffer.size() : size;
        if (size == 0 || size > buffer.size()) return false;

        SIZE_T written;
        if (!ReadProcessMemory(m_handle, base, buffer.base(), size, &written))
            return 0;

        return written;
    }

    bool process::mm_query(const addr base, MEMORY_BASIC_INFORMATION& mbi) const {
        return VirtualQueryEx(m_handle, base, &mbi, sizeof(mbi));
    }

    std::generator<MEMORY_BASIC_INFORMATION> process::mm_pages(const region& buffer) const {
        MEMORY_BASIC_INFORMATION mbi;
        std::uintptr_t at = buffer.base();

        while (at < buffer.end() && mm_query(at, mbi)) {
            co_yield mbi;
            at += mbi.RegionSize;
        }
    }

    DWORD process::mm_protect(const addr base, const std::size_t size, const DWORD new_prot) const {
        DWORD old_prot;
        return VirtualProtectEx(m_handle, base, size, new_prot, &old_prot) ? new_prot : 0;
    }

    addr process::mm_alloc(addr base, std::size_t size, const DWORD flags, const DWORD protect) const {
        return NT_SUCCESS(NtAllocateVirtualMemory(
            m_handle,
            &base.u,
            0, // Anywhere in user VA space
            &size,
            flags,
            protect
        )) ? base : nullptr;
    }

    addr process::mm_alloc(const std::size_t size, const DWORD flags, const DWORD protect) const {
        return mm_alloc(nullptr, size, flags, protect);
    }

    addr process::mm_alloc(const std::size_t size, const DWORD protect) const {
        return mm_alloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
    }

    std::size_t process::mm_write(const addr base, const region& buffer, std::size_t size) const {
        if (size == 0) size = buffer.size();
        if (size > buffer.size()) return 0;

        SIZE_T written;
        return NT_SUCCESS(NtWriteVirtualMemory(
            m_handle,
            base,
            buffer.base(),
            size,
            &written)
        ) ? written : 0;
    }

    bool process::mm_free(const addr base) const {
        return NT_SUCCESS(NtFreeVirtualMemory(m_handle, base, nullptr, MEM_RELEASE));
    }

    bool process::mm_decommit(const region& region) const {
        SIZE_T size = region.size();
        return NT_SUCCESS(NtFreeVirtualMemory(m_handle, region.base(), &size, MEM_DECOMMIT));
    }

    addr process::mm_inject(const region& buffer, const DWORD protect) const {
        const auto base = mm_alloc(nullptr, buffer.size(), MEM_COMMIT | MEM_RESERVE, protect);
        if (base == nullptr) return nullptr;

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

        for (std::uintptr_t addr = um_region.base(); addr < um_region.end(); ) {
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
