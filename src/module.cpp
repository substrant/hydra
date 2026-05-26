#include <hydra/module.hpp>
#include <hydra/memory.hpp>
#include <hydra/process.hpp>

#include <filesystem>
#include <fstream>
#include <psapi.h>
#include <ranges>

static constexpr auto map_raw_section = std::views::transform([](const std::pair<std::string, hy::pe_section>& x) { return x.second.raw(); });
static constexpr auto map_section_base = std::views::transform([](const std::pair<std::string, hy::pe_section>& x) { return x.second.offset(); });

namespace hy {
    pe_image pe_image::load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Could not open file");

        const auto size = file.tellg();
        auto buffer = region::alloc_local(size);

        file.seekg(0);
        file.read(buffer.base(), size);

        auto stm = std::make_unique<local_stream>(std::move(buffer));
        return load(std::move(stm), pe_source::file);
    }

    pe_image pe_image::load_base(const addr base) {
        auto stm = std::make_unique<local_stream>(hy::region(base, page_size));
        return load(std::move(stm), pe_source::mapped);
    }

    addr pe_section::offset(pe_source source) const {
        source = pe_image::canonical_source(source, m_image);

        switch (source) {
            case pe_source::mapped:
                /* section RVA */
                return static_cast<std::uintptr_t>(m_header.VirtualAddress);
            case pe_source::file:
                /* section FOA */
                return static_cast<std::uintptr_t>(m_header.PointerToRawData);
            case pe_source::header:
                throw std::runtime_error("cannot get section offset without stream access");
            default:
                throw std::runtime_error("what the fuck are you feeding this function bruh");
        }
    }

    addr pe_section::base() const {
        return m_image->base() + offset(pe_source::inherit);
    }

    region pe_section::buffer(const pe_source source) const {
        if (!m_image) throw std::runtime_error("section not bound to image");
        return { m_image->base() + offset(source), size(source) };
    }

    std::string pe_section::name() const {
        const auto* name_cstr = reinterpret_cast<const char*>(m_header.Name);
        return { name_cstr, strnlen(name_cstr, IMAGE_SIZEOF_SHORT_NAME) };
    }

    std::size_t pe_section::size(pe_source source) const {
        source = pe_image::canonical_source(source, m_image);

        switch (source) {
        case pe_source::mapped:
            return m_header.Misc.VirtualSize;
        case pe_source::file:
            return m_header.SizeOfRawData;
        default:
            throw std::runtime_error("what the fuck are you feeding this function bruh");
        }
    }

    pe_status pe_image::read(const pe_scope scope) {
        LONG nt_offset, sections_offset;

        // Pull and validate the DOS header
        if (static_cast<bool>(scope & pe_scope::dos_header)) {
            if (m_stream->read_obj(m_dos_header) != sizeof(IMAGE_DOS_HEADER))
                return pe_status::buffer_too_small;

            if (m_dos_header.e_magic != IMAGE_DOS_SIGNATURE)
                return pe_status::bad_dos_signature;
        }

        if (static_cast<bool>(scope & pe_scope::nt_headers)) {
            if (!static_cast<bool>(scope & pe_scope::dos_header))
                throw std::runtime_error("can't read NT headers without DOS information");

            // Exract the NT offset
            nt_offset = m_dos_header.e_lfanew;
            if (!nt_offset) return pe_status::bad_nt_offset;

            // Ensure that we have a large enough buffer for NT headers
            m_stream->seek(nt_offset, stream_origin::begin);

            // Read NT headers at base + e_lfanew (bro what idiot named this)
            if (m_stream->read_obj(m_nt_headers) != sizeof(IMAGE_NT_HEADERS))
                return pe_status::buffer_too_small;

            if (m_nt_headers.Signature != IMAGE_NT_SIGNATURE)
                return pe_status::bad_nt_signature;
        }

        if (static_cast<bool>(scope & pe_scope::sections)) { // todo: prob better way to handle 'scopes' because each depends on the other but we need to suppoprt scopes still so we can choose what to write back
            if (!static_cast<bool>(scope & pe_scope::dos_header))
                throw std::runtime_error("can't read sections without DOS information");

            if (!static_cast<bool>(scope & pe_scope::nt_headers))
                throw std::runtime_error("can't read sections without NT headers information");

            // Get section infomration
            const auto n_sections = m_nt_headers.FileHeader.NumberOfSections;

            // Prepare section data
            m_sections.clear();
            m_sections.clear();
            m_sections.reserve(n_sections);

            // Get offset to sections and verify
            sections_offset = nt_offset + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + m_nt_headers.FileHeader.SizeOfOptionalHeader;
            if (!sections_offset)
                return pe_status::bad_sections;
            m_stream->seek(sections_offset, stream_origin::begin);

            // Fill section data into class
            for (WORD i = 0; i < n_sections; i++) {
                const auto section = pe_section(this, m_stream->read_obj<IMAGE_SECTION_HEADER>());
                m_sections.emplace(section.name(), section);
            }
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

    std::optional<pe_section> pe_image::section(const std::string_view target_name) const {
        for (auto&& section : m_sections | std::views::values) {
            if (section.name() == target_name)
                return section;
        }

        return std::nullopt;
    }

    std::size_t pe_image::size(const pe_source size_type) const {
        std::size_t max_end = 0;

        if (size_type == pe_source::header) {
            max_end = std::ranges::min(m_sections | map_section_base);
        }
        else if (size_type == pe_source::mapped) {
            for (const auto* section : m_sections | map_raw_section) {
                const auto end = section->VirtualAddress + section->Misc.VirtualSize;
                max_end = std::max<size_t>(end, max_end);
            }
        }
        else if (size_type == pe_source::file) {
            for (const auto* section : m_sections | map_raw_section) {
                const auto end = section->PointerToRawData + section->SizeOfRawData;
                max_end = std::max<size_t>(end, max_end);
            }
        }

        return max_end;
    }

    addr pe_image::resolve_rva(const DWORD rva, const pe_source source) const {
        if (source == pe_source::inherit)
            throw std::exception("no");

        if (source == pe_source::mapped || source == pe_source::header)
            return base() + rva;

        const auto secs = sections();

        if (!secs.empty() && rva < secs.front()->raw()->VirtualAddress)
            return base() + rva;

        for (const auto* sec : secs) {
            const auto va = sec->raw()->VirtualAddress;
            const auto vsize = sec->raw()->Misc.VirtualSize;

            if (rva >= va && rva < va + vsize)
                return base() + sec->raw()->PointerToRawData + (rva - va);
        }

        return nullptr;
    }

    addr pe_image::internal_get_import(const std::string_view symbol, const pe_source source, pe_location loc) const {
        const auto src = canonical_source(source, this);
        const auto& import_dir = m_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir.Size == 0 || import_dir.VirtualAddress == 0)
            return nullptr;

        const auto import_descriptor = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(resolve_rva(import_dir.VirtualAddress, src));
        if (!import_descriptor) return nullptr;

        // TODO: some day refactor this shit to use new methods introduced to pe_image. If it aint broke dont fix it

        for (auto descriptor = import_descriptor; descriptor->Name != 0; descriptor++) {
            const auto thunk = static_cast<PIMAGE_THUNK_DATA>(resolve_rva(descriptor->FirstThunk, src));
            const auto orig_thunk = static_cast<PIMAGE_THUNK_DATA>(
                descriptor->OriginalFirstThunk
                ? resolve_rva(descriptor->OriginalFirstThunk, src)
                : resolve_rva(descriptor->FirstThunk, src)
            );

            for (SIZE_T i = 0; orig_thunk[i].u1.AddressOfData != 0; i++) {
                if (IMAGE_SNAP_BY_ORDINAL(orig_thunk[i].u1.Ordinal)) continue;

                const auto import_by_name = static_cast<PIMAGE_IMPORT_BY_NAME>(resolve_rva(static_cast<DWORD>(orig_thunk[i].u1.AddressOfData), src));
                const std::string_view current_symbol = import_by_name->Name;
                if (current_symbol == symbol) {
                    const addr offset{ &thunk[i].u1.Function };
                    return loc == pe_location::absolute ? base() + offset : offset;
                }
            }
        }

        return nullptr;
    }

    addr pe_image::internal_get_export(const std::string_view symbol, const pe_source source, const pe_location loc) const {
        const auto src = canonical_source(source, this);
        const auto& export_dir_entry = m_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (export_dir_entry.Size == 0 || export_dir_entry.VirtualAddress == 0)
            return nullptr;

        const auto export_dir = static_cast<PIMAGE_EXPORT_DIRECTORY>(resolve_rva(export_dir_entry.VirtualAddress, src));
        if (!export_dir) return nullptr;

        const auto names = static_cast<PDWORD>(resolve_rva(export_dir->AddressOfNames, src));
        const auto ordinals = static_cast<PWORD>(resolve_rva(export_dir->AddressOfNameOrdinals, src));
        const auto functions = static_cast<PDWORD>(resolve_rva(export_dir->AddressOfFunctions, src));

        for (DWORD i = 0; i < export_dir->NumberOfNames; i++) {
            const std::string_view current_symbol = static_cast<char*>(resolve_rva(names[i], src));
            if (current_symbol != symbol) continue;

            // Return virtual address: base() + RVA (works for both file and mapped sources)
            const addr offset{ static_cast<std::uintptr_t>(functions[ordinals[i]]) };
            return loc == pe_location::absolute ? base() + offset : offset;
        }

        return nullptr;
    }

    remote_module::remote_module(std::shared_ptr<process> proc, const addr base, std::optional<std::string> name, std::filesystem::path path)
        : pe_image(std::make_unique<remote_stream>(proc, base), pe_source::mapped), m_proc(std::move(proc)), m_name(std::move(name)), m_path(std::move(path)) {

        if (read() != pe_status::success) // bad_weak_ptr
            throw std::runtime_error("invalid PE header");
    }

    /*bool remote_module::dump_image(const hy::buffer& buffer, dump_context* ctx) {
        const auto remote_base = buffer.base();
        const auto total_pages = (size(pe_size::mapped) + page_size - 1) / page_size;

        // Grow to full size
        m_stream->resize(size(pe_size::file));

        // Fill with 0xCC and then copy DOS header into buffer
        std::memset(remote_base, 0xCC, m_stream->size());
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
    }*/
}
