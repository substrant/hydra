#include "detail/pch.hpp"

#include "user/process.hpp"
#include "module.hpp"

#include "io/error.hpp"

static constexpr auto map_raw_section = std::views::transform([](const auto& x) { return x->raw(); });

namespace hy {
    pe_image pe_image::load_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw io::error("Could not open file");

        const auto size = file.tellg();
        auto buffer = mem::buffer::create(size);

        file.seekg(0);
        file.read(buffer.base(), size);

        return pe_image{std::move(buffer)};
    }

    pe_image pe_image::load_buffer(const mem::buffer& buffer) {
        return pe_image{buffer};
    }

    pe_image pe_image::load_base(const mem::addr base) {
        pe_image image{ { base, mem::page_size } };
        image.read_header();
        return image;
    }

    std::shared_ptr<remote_module> remote_module::from_header(const std::shared_ptr<process>& proc, const mem::addr base, const std::string_view name, const std::filesystem::path& path) {
        // Load (likely partial header) into memory
        auto buffer = mem::buffer::create(mem::page_size);
        if (!proc->mm_read(base, buffer))
            throw std::runtime_error("bruh");

        // Create PE image from buffer
        auto image = std::make_shared<remote_module>(proc, buffer, name, base, path);

        // Parse PE header
        if (image->read_header() != pe_status::success)
            throw std::runtime_error("invalid PE header");

        // Get sizes
        const auto init_size = buffer.size();
        const auto header_size = image->size(pe_size::header);

        // If header size is larger than the buffer size, resize and re-read
        if (header_size > init_size) {
            buffer.resize(header_size);

            // Read the full region
            if (!proc->mm_read(base, buffer))
                throw std::runtime_error("cant grow");

            // Parse PE header fully
            if (image->read_header() != pe_status::success)
                throw std::runtime_error("invalid PE header");
        }

        // Return the image
        return image;
    }

    std::shared_ptr<remote_module> remote_module::from_remote(const std::shared_ptr<process>& proc,const mem::addr base) {
        char path[MAX_PATH + 1];
        char name[MAX_PATH + 1];

        // Get the path of the module
        std::size_t path_len = GetModuleFileNameExA(*proc, base, path, sizeof(path));
        path[path_len] = '\0';

        // Get the module name only (strip path)
        std::size_t name_len = GetModuleBaseNameA(*proc, base, name, sizeof(name));
        name[name_len] = '\0';

        return from_header(proc, base, name, path);
    }

    pe_status pe_image::read_header() {
        const mem::addr base = m_buffer.base();

        // Pull and validate the DOS header
        if (m_buffer.size() < sizeof(IMAGE_DOS_HEADER))
            return pe_status::buffer_too_small;

        m_dos_header = *static_cast<PIMAGE_DOS_HEADER>(base);
        if (m_dos_header.e_magic != IMAGE_DOS_SIGNATURE)
            return pe_status::bad_dos_signature;

        // Verify e_lfanew and that we can read NT headers
        if (!m_dos_header.e_lfanew)
            return pe_status::bad_nt_offset;

        if (m_buffer.size() < m_dos_header.e_lfanew + sizeof(IMAGE_NT_HEADERS))
            return pe_status::buffer_too_small;

        // Read NT headers at base + e_lfanew (bro what idiot named this)
        const auto nt_headers_buf = static_cast<PIMAGE_NT_HEADERS>(base + m_dos_header.e_lfanew);
        m_nt_headers = *nt_headers_buf;

        if (m_nt_headers.Signature != IMAGE_NT_SIGNATURE)
            return pe_status::bad_nt_signature;

        // Pull section headers
        const auto sections_count = m_nt_headers.FileHeader.NumberOfSections;
        auto* section_headers_buf = IMAGE_FIRST_SECTION(nt_headers_buf); // note how we're reading from the buffer, not _nt_headers

        if (!section_headers_buf)
            return pe_status::bad_sections;
        if (!m_buffer.contains(static_cast<mem::addr>(section_headers_buf) + sizeof(IMAGE_SECTION_HEADER) * sections_count))
            return pe_status::buffer_too_small;

        // Prepare section data
        m_sections_lst.clear();
        m_sections_lst.reserve(sections_count);
        m_sections_map.clear();
        m_sections_map.reserve(sections_count);

        // Fill section data into class
        for (WORD i = 0; i < sections_count; i++) {
            auto* section = &section_headers_buf[i];
            auto name_view = std::string_view(
                reinterpret_cast<const char*>(section->Name),
                strnlen(reinterpret_cast<const char*>(section->Name), IMAGE_SIZEOF_SHORT_NAME)
            );

            // Read section into the heap and store it in the vector and map
            auto section_ptr = std::make_shared<pe_section>(shared_from_this(), *section);
            m_sections_lst.push_back(section_ptr);
            m_sections_map.emplace(std::string(name_view), section_ptr);
        }

        return pe_status::success;
    }

    std::string pe_image::file_type() const {
        if (m_nt_headers.FileHeader.Characteristics & IMAGE_FILE_DLL)
            return "dll";

        if (m_nt_headers.FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)
            return "exe";

        return "bin";
    }

    std::shared_ptr<pe_section> pe_image::section(const std::string_view target_name) const {
        for (const auto& section : m_sections_lst) {
            if (section->name() == target_name)
                return section;
        }
        return nullptr;
    }

    std::size_t pe_image::size(const pe_size size_type) const {
        std::size_t max_end = 0;

        if (size_type == pe_size::header) {
            max_end = m_sections_lst[0]->raw()->PointerToRawData;
        }
        else if (size_type == pe_size::mapped) {
            for (const auto* section : m_sections_lst | map_raw_section) {
                const auto end = section->VirtualAddress + section->Misc.VirtualSize;
                max_end = std::max<size_t>(end, max_end);
            }
        }
        else if (size_type == pe_size::file) {
            for (const auto* section : m_sections_lst | map_raw_section) {
                const auto end = section->PointerToRawData + section->SizeOfRawData;
                max_end = std::max<size_t>(end, max_end);
            }
        }

        return max_end;
    }

    mem::addr pe_image::import(std::string_view symbol) const {
        // Get the data directory for imports
        const auto& import_dir = m_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir.Size == 0 || import_dir.VirtualAddress == 0) {
            return nullptr; // No import directory
        }

        const mem::addr base_addr = m_buffer.base();
        const auto import_descriptor = base_addr + import_dir.VirtualAddress;
        
        // Walk through each imported DLL
        for (auto descriptor = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(import_descriptor); descriptor->Name != 0; descriptor++) {
            const auto thunk = static_cast<PIMAGE_THUNK_DATA>(base_addr + descriptor->FirstThunk);
            auto orig_thunk = static_cast<PIMAGE_THUNK_DATA>(base_addr + descriptor->OriginalFirstThunk);
            
            // Default original thunk to the first think in the list
            if (descriptor->OriginalFirstThunk == 0) orig_thunk = thunk;
            
            // Walk through all imported functions for this DLL
            for (SIZE_T i = 0; orig_thunk[i].u1.AddressOfData != 0; i++) {
                // Check if this is an ordinal import because we can't compare ordinals with string symbols
                if (IMAGE_SNAP_BY_ORDINAL(orig_thunk[i].u1.Ordinal)) continue;

                const auto import_by_name = static_cast<PIMAGE_IMPORT_BY_NAME>(base_addr + orig_thunk[i].u1.AddressOfData);

                // Check if this is the symbol we're looking for
                const std::string_view current_symbol = import_by_name->Name;
                if (current_symbol == symbol) return { &thunk[i].u1.Function };
            }
        }
        
        // Symbol not found
        return nullptr;
    }

    bool remote_module::dump_image(const mem::buffer& buffer, dump_context* ctx) {
        const auto remote_base = remote_buffer().base();
        const auto total_pages = (size(pe_size::mapped) + mem::page_size - 1) / mem::page_size;

        // Grow to full size
        m_buffer.resize(size(pe_size::file));

        // Fill with 0xCC and then copy DOS header into buffer
        std::memset(remote_base, 0xCC, m_buffer.size());
        std::memcpy(remote_base, &m_dos_header, sizeof(IMAGE_DOS_HEADER));

        // Copy NT headers into buffer
        const auto nt_offset = m_dos_header.e_lfanew;
        std::memcpy(remote_base + nt_offset, &m_nt_headers, sizeof(IMAGE_NT_HEADERS));

        // Copy section headers (immediately after NT headers)
        IMAGE_SECTION_HEADER* section_table = remote_base + nt_offset + sizeof(IMAGE_NT_HEADERS);
        for (std::size_t i = 0; i < m_sections_lst.size(); ++i) {
            std::memcpy(
                &section_table[i],
                m_sections_lst[i]->raw(),
                sizeof(IMAGE_SECTION_HEADER)
            );
        }

        // Copy section data from remote process memory
        for (const auto* section : m_sections_lst | map_raw_section) {
            const auto file_addr = remote_base + section->PointerToRawData;
            const auto file_size = section->SizeOfRawData;
            const auto virt_addr = m_base + section->VirtualAddress;
            const auto virt_size = section->Misc.VirtualSize;
            const auto min_size = std::min(file_size, virt_size);

            //cui::out << "[+] Resolving section " << section->Name << "\n";

            // Attempt to read the entire section from the remote process
            if (m_proc->mm_read(virt_addr, file_addr, min_size)) {
                // Success!
                continue;
            }

            // If this is a code section (like .text), we'll try to dump it page-by-page
            if (section->Characteristics & IMAGE_SCN_CNT_CODE) {
                // timestamp
                const auto time_now = std::chrono::system_clock::now();

                // Set up a feedback handler
                ctx->callback = [&](const dump_context* _, const std::size_t pages_read) -> void {
                    const auto ratio = static_cast<double>(pages_read) / static_cast<double>(total_pages);
                    if (ratio > ctx->clear_ratio) ctx->stop = true;

                    // Check if we've been running for too long
                    const auto elapsed = std::chrono::system_clock::now() - time_now;
                    if (ctx->clear_time && elapsed > *ctx->clear_time) {
                        //std::cout << "\n[!] Dump time for " << section->Name << " exceeded.\n";
                        ctx->stop = true;
                    }

                    //progress.update(pages_read);
                };

                // Start dump
                if (!m_proc->mm_dump(virt_addr, buffer + virt_addr, ctx)) {
                    //std::cout << "[-] Failed to dump section " << section->Name << "\n";
                }
            }
            else {
                //std::cout << "[-] Skipping section " << section->Name << "\n";
            }
        }

        return true;
    }
}
