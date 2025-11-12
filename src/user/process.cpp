#include <hydra/detail/pch.hpp>

#include <psapi.h>

#include <deque>
#include <unordered_set>
#include <ranges>

#include <hydra/sys/toolhelp.hpp>
#include <hydra/sys/process.hpp>

namespace hy {
    using hw_clock = std::chrono::high_resolution_clock;

    void process::init() {
        if (!m_handle.is_valid())
            throw std::runtime_error("Invalid handle provided");
        m_this = shared_from_this();
    }

    bool process::scan_linked_modules() {
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

            // Get the module's information
            if (!GetModuleInformation(m_handle, module_handle, &module_info, sizeof(module_info)))
                continue;

            // Get the path of the module
            if ((path_len = GetModuleFileNameExA(m_handle, module_handle, path_buf, sizeof(path_buf))) == 0)
                continue;

            // Get the module name only (strip path)
            if ((name_len = GetModuleBaseNameA(m_handle, module_handle, name_buf, sizeof(name_buf))) == 0)
                continue;

            // Prepare information
            const auto path = std::string(path_buf, path_len);
            const auto name = std::string(name_buf, name_len);
            const auto base = reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll);
            const auto ptr = remote_module::from_header(m_this, base, name, path);

            // Push module to list and dict
            m_module_list.push_back(ptr);
            m_modules.insert(std::make_pair(base, ptr));
        }

        return !m_module_list.empty();
    }

    std::shared_ptr<process> process::open(const HANDLE handle) {
        std::shared_ptr<process> proc(new process(handle, true));
        proc->init();
        return proc;
    }

    std::shared_ptr<process> process::open(const DWORD id) {
        std::shared_ptr<process> proc(new process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, id)));
        proc->init();
        return proc;
    }

    std::shared_ptr<process> process::open(const std::string_view name) {
        for (const auto entry : toolhelp::get_processes()) {
            if (entry.szExeFile == name)
                return open(entry.th32ProcessID);
        }
        return nullptr;
    }

    bool process::is_valid() const {
        return m_handle.is_valid();
    }

    process::operator HANDLE() const {
        return m_handle;
    }

    process::operator DWORD() const {
        return m_handle.is_valid() ? GetProcessId(m_handle) : -1;
    }

    bool process::kill(const LONG exit_code, NTSTATUS* p_status) {
        NTSTATUS dummy_status;
        if (!p_status) p_status = &dummy_status;

        *p_status = NtTerminateProcess(m_handle, exit_code);
        m_handle = nullptr;

        return NT_SUCCESS(*p_status);
    }

    detail::generator<std::shared_ptr<thread>> process::threads() const {
        for (const auto&& entry : toolhelp::get_threads(*this))
            co_yield std::shared_ptr(thread::from_id(entry.th32ThreadID));
    }

    void process::suspend() const {
        for (auto&& thread : threads())
            thread->suspend();
    }

    void process::resume() const {
        for (auto&& thread : threads())
            thread->resume();
    }

    detail::generator<std::shared_ptr<remote_module>> process::linked_modules() {
        if (m_modules.empty() && !scan_linked_modules())
            throw std::runtime_error("Failed to enumerate linked modules");

        for (auto&& v : m_modules | std::views::values)
            co_yield v;
    }

    detail::generator<std::shared_ptr<remote_module>> process::unlinked_modules() {
        std::unordered_set<void*> linked_bases;

        // Scan for linked modules
        if (m_modules.empty() && !scan_linked_modules())
            throw std::runtime_error("Failed to enumerate linked modules");

        // Allocate space and fill bases
        linked_bases.reserve(m_modules.size());
        for (auto&& v : m_modules | std::views::values)
            linked_bases.insert(v->buffer().base());

        for (const auto mbi : mm_pages(um_bounds)) {
            // Is this memory accessible?
            if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS || mbi.Protect & PAGE_GUARD))
                continue;

            // Allocate data for page and read it
            const auto buffer = buffer::create(page_size);
            if (!mm_read(mbi.BaseAddress, buffer)) continue;

            // If bound to PEB, skip
            if (!linked_bases.contains(mbi.BaseAddress))
                continue;

            // This is an unlinked module
            co_yield remote_module::from_header(m_this, mbi.BaseAddress, "unknown");
        }
    }

    std::shared_ptr<remote_module> process::module(const std::optional<std::string>& name) {
        // Scan for linked modules
        if (m_modules.empty() && !scan_linked_modules())
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

    std::size_t process::mm_read(const addr base, const buffer& buffer, std::size_t size) const {
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

    detail::generator<MEMORY_BASIC_INFORMATION> process::mm_pages(const buffer& buffer) const {
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

    // Returns amount of pages read
    std::size_t process::mm_dump(const addr base, const buffer& buffer, dump_context* ctx) const {
        MEMORY_BASIC_INFORMATION mbi;
        std::deque<std::uintptr_t> page_queue;
        std::size_t pages_read = 0;

        // Fill buffer range with sentinel byte
        std::memset(buffer.base(), ctx->sentinel, buffer.size());

        // Helper function to read page and track progress
        const auto read_page = [&](const std::uintptr_t va) -> bool {
            const auto offset = va - base.i;
            const auto success = mm_read(va, buffer.base() + offset, page_size);

            if (success) pages_read++;
            return success;
        };

        // Helper function for status reports
        auto last_tick = hw_clock::now();
        const auto signal_report = [&]() -> void {
            const auto current_tick = hw_clock::now();
            if (current_tick - last_tick < std::chrono::milliseconds(100))
                return;

            if (ctx->callback) (*ctx->callback)(ctx, pages_read);
            last_tick = current_tick;
        };

        const auto end_address = base + buffer.size();

        // Phase 1:
        //   Go through each page in the region and attempt to read
        //   If we can't read the page, queue it for the 2nd phase
        for (std::uintptr_t page_base = base; page_base < end_address; page_base += page_size) {
            if (!mm_query(page_base, mbi)) {
                // Skip page
                continue;
            }
            
            // if NOACCESS or we cannot read the page, queue it
            if (mbi.Protect & PAGE_NOACCESS || !read_page(page_base))
                page_queue.push_back(page_base);
        }

        // Phase 2:
        //   Loop queue until all pages are able to be read or adr
        //   cancellation is requested
        while (!page_queue.empty() && !ctx->stop) {
            signal_report();

            auto page = page_queue.front();
            page_queue.pop_front();

            if (!read_page(page)) {
                // Requeue after querying the other pages
                page_queue.push_back(page);
            }
        }

        // Revert stop flag if set
        ctx->stop = false;

        return pages_read;
    }

    std::vector<addr> process::scan_heap(const std::uint8_t* pattern, const char* mask) const {
        std::vector<addr> results;
        MEMORY_BASIC_INFORMATION mbi;
        auto time = std::chrono::system_clock::now();

        for (std::uintptr_t addr = um_bounds.base(); addr < um_bounds.end(); ) {
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

            const auto dump = buffer::create(mbi.RegionSize);
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

    addr process::mm_alloc(const addr base, const std::size_t size, const DWORD flags, const DWORD protect) const {
        return VirtualAllocEx(m_handle, base, size, flags, protect);
    }

    addr process::mm_alloc(const std::size_t size, const DWORD flags, const DWORD protect) const {
        return mm_alloc(nullptr, size, flags, protect);
    }

    addr process::mm_alloc(const std::size_t size, const DWORD protect) const {
        return mm_alloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protect);
    }

    std::size_t process::mm_write(const addr base, const buffer& buffer, std::size_t size) const {
        if (size == 0) size = buffer.size();
        if (size > buffer.size()) return 0;

        SIZE_T written;
        if (!WriteProcessMemory(m_handle, base, buffer.base(), size, &written))
            return 0;

        return written;
    }

    addr process::mm_inject(const buffer& source, const DWORD protect) const {
        const auto base = mm_alloc(nullptr, source.size(), MEM_COMMIT | MEM_RESERVE, protect);
        if (base == nullptr) return nullptr;

        if (!mm_write(base, source)) {
            (void)mm_free(base);
            return nullptr;
        }

        return base;
    }

    bool process::mm_free(const addr base, const DWORD flags) const {
        return VirtualFreeEx(m_handle, base, 0, flags);
    }

    map_status process::pe_mmap(pe_image& pe, mapping_info* info_out) const {
        // Allocate memory in the target process for the module
        info_out->size = pe.size(pe_size::mapped);
        info_out->header_size = pe.size(pe_size::header);
        info_out->base = mm_alloc(info_out->size, PAGE_EXECUTE_READWRITE);

        if (info_out->base == nullptr) {
            //std::cout << "[-] Failed to allocate memory in target process.\n";
            return map_status::failed_allocation;
        }

        //std::cout << "[+] Base Address: 0x" << std::hex << info_out->base << "\n";

        // Copy header information to the target process
        //if (!mm_write(info_out->base, pe.m_stream, info_out->header_size))
        //    return map_status::failed_write;

        // Write sections to the target process
        for (const auto& section : pe.sections()) {
            const auto section_size = section->size();

            // Skip empty sections
            if (section_size < 1) continue;

            // Calculate the destination in the target process
            const auto dest_address = reinterpret_cast<std::uintptr_t>(info_out->base) + section->raw()->VirtualAddress;

            // Write the section data to the target process
            if (!mm_write(dest_address, pe.m_stream.base() + section->raw()->PointerToRawData, section_size)) {
                //std::cout << "[-] Failed to write section to target process.\n";
                return map_status::failed_write;
            }

           // std::cout << "[+] Mapped " << section->name() << " => 0x" << std::hex << dest_address << " (" << std::dec << section_size << " bytes)\n";
        }

        return map_status::success;
    }
}
