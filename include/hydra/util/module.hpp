#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <fstream>

#include "memory.hpp"

namespace hydra {
    // Forward-decl
    class process;
    class pe_section;

    // status > 0 = yay
    enum class pe_status : std::int8_t {
        bad_internal_error = -0x7F,
        bad_dos_signature,
        bad_nt_offset,
        bad_nt_signature,
        bad_sections,
        buffer_too_small,
        success = 0
    };

    enum class pe_size : std::uint8_t {
        header,
        mapped,
        file
    };

    class pe_image : public std::enable_shared_from_this<pe_image> {
        friend class pe_module;
        friend class process;

    protected:
        mem::buffer m_buffer{};

        IMAGE_DOS_HEADER m_dos_header{};
        IMAGE_NT_HEADERS m_nt_headers{};

        std::unordered_map<std::string, std::shared_ptr<pe_section>> m_sections_map{};
        std::vector<std::shared_ptr<pe_section>> m_sections_lst{};
        
        explicit pe_image(const mem::buffer& buffer) : m_buffer(buffer) { }

    public:
        explicit pe_image() {}

        operator PIMAGE_DOS_HEADER() { return &m_dos_header; }

        operator PIMAGE_NT_HEADERS() { return &m_nt_headers; }

        virtual ~pe_image() = default;

        static bool verify_header(const mem::buffer& buffer, pe_status* p_status = nullptr);

        static pe_image from_file(const std::filesystem::path& path);

        static pe_image from_buffer(const mem::buffer& buffer);

        static pe_image from_base(mem::addr base);

        pe_status read_header();

        bool is_partial() const { return m_buffer.size() < size(pe_size::mapped); }

        std::string file_type() const;

        std::vector<std::shared_ptr<pe_section>>& sections() { return m_sections_lst; }

        std::shared_ptr<pe_section> section(std::string_view target_name) const;

        std::size_t size(pe_size size_type = pe_size::file) const;

        mem::buffer& local_buffer() const { return const_cast<mem::buffer&>(m_buffer); }

        mem::addr import(std::string_view symbol) const;
    };

    class pe_module : public pe_image {
        std::shared_ptr<process> m_proc;
        mem::addr m_base;
        std::string m_name;
        std::filesystem::path m_path;

    public:
        explicit pe_module(const std::shared_ptr<process>& proc, mem::buffer& buffer, const std::string_view name, const mem::addr base, const std::filesystem::path& path = {})
            : pe_image(buffer), m_base(base), m_proc(proc), m_name(name), m_path(path) { }

        static std::shared_ptr<pe_module> from_header(const std::shared_ptr<process>& proc, mem::addr base, std::string_view name, const std::filesystem::path& path = {});

        static std::shared_ptr<pe_module> from_remote(const std::shared_ptr<process>& proc, mem::addr base);

        std::shared_ptr<process> proc() const { return m_proc; }

        mem::buffer remote_buffer() const { return mem::buffer::from_base(m_base, size(pe_size::mapped)); }

        bool dump_image(std::atomic<bool>& stop, double clear_ratio, int wait_ms = 10000);

        std::string_view file_name() const { return m_name; }

        std::filesystem::path file_path() const { return m_path; }
    };

    enum class map_status : int {
        success,
        failed_allocation,
        failed_write
    };

    class pe_section {
        std::shared_ptr<pe_image> m_image;
        IMAGE_SECTION_HEADER m_header;

    public:
        explicit pe_section(const std::shared_ptr<pe_image>& image, const IMAGE_SECTION_HEADER& header) : m_image(image), m_header(header) { }

        const IMAGE_SECTION_HEADER* raw() const { return &m_header; }

        std::shared_ptr<pe_image> image() const { return m_image; }

        std::shared_ptr<pe_module> module() const { return std::dynamic_pointer_cast<pe_module>(m_image); }

        mem::addr local_base() const {
            return m_image->local_buffer().data() + m_header.PointerToRawData;
        }

        mem::buffer local_buffer(const pe_size size = pe_size::file) const {
            return mem::buffer::from_base(local_base(), size == pe_size::mapped ? m_header.Misc.VirtualSize : m_header.SizeOfRawData);
        }

        mem::addr remote_base() const {
            const auto mod = module();
            return mod ? mod->remote_buffer().data() + m_header.VirtualAddress : nullptr;
        }

        mem::buffer remote_buffer(const pe_size size = pe_size::mapped) const {
            return mem::buffer::from_base(remote_base(), size == pe_size::mapped ? m_header.Misc.VirtualSize : m_header.SizeOfRawData);
        }

        std::string_view name() const {
            const auto* name_cstr = reinterpret_cast<const char*>(m_header.Name);
            return {name_cstr, strnlen(name_cstr, IMAGE_SIZEOF_SHORT_NAME)};
        }

        std::size_t size() const {
            const auto mod = module();
            return mod ? m_header.Misc.VirtualSize : m_header.SizeOfRawData;
        }
    };
}