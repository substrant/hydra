#pragma once

#include <phnt_windows.h>
#include <filesystem>

#include <hydra/memory.hpp>

namespace hy {
    // Forward-decl
    class process;
    struct dump_context;
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
        friend class process;

    protected:
        memory_stream m_stream;

        IMAGE_DOS_HEADER m_dos_header{};
        IMAGE_NT_HEADERS m_nt_headers{};

        std::unordered_map<std::string, std::shared_ptr<pe_section>> m_sections_map{};
        std::vector<std::shared_ptr<pe_section>> m_sections_lst{};

        explicit pe_image(memory_stream stm) : m_stream(std::move(stm)) { }

    public:
        explicit operator PIMAGE_DOS_HEADER() { return &m_dos_header; }

        explicit operator PIMAGE_NT_HEADERS() { return &m_nt_headers; }

        virtual ~pe_image() = default;

        static pe_image load(const std::filesystem::path& path);

        static pe_image load(const memory_stream &stm);

        static pe_image load_base(addr base);

        pe_status read();

        std::string file_type() const;

        std::vector<std::shared_ptr<pe_section>>& sections() { return m_sections_lst; }

        std::shared_ptr<pe_section> section(std::string_view target_name) const;

        std::size_t size(pe_size size_type = pe_size::file) const;

        addr import(std::string_view symbol) const;
    };

    class remote_module : public pe_image {
        std::shared_ptr<process> m_proc;
        addr m_base;
        std::string m_name;
        std::filesystem::path m_path;

    protected:
        explicit remote_module(memory_stream& stm, std::shared_ptr<process> proc, std::string name, std::filesystem::path path = {})
            : pe_image(std::move(stm)), m_proc(std::move(proc)), m_base(m_stream.base()), m_name(std::move(name)), m_path(std::move(path)) { }

    public:
        static std::shared_ptr<remote_module> from_header(const std::shared_ptr<process> &proc, addr base, const std::string &name, const std::filesystem::path &path = {});

        static std::shared_ptr<remote_module> from_remote(const std::shared_ptr<process>& proc, addr base);

        std::shared_ptr<process> proc() const { return m_proc; }

        [[nodiscard]] region buffer() const { return { m_base, size(pe_size::mapped) }; }

        //bool dump_image(const hy::buffer& buffer, dump_context* ctx);

        std::string_view file_name() const { return m_name; }

        std::filesystem::path file_path() const { return m_path; }
    };

    class pe_section {
        friend class pe_image;

        std::shared_ptr<pe_image> m_image;
        IMAGE_SECTION_HEADER m_header;

    protected:
        explicit pe_section(const std::shared_ptr<pe_image>& image, const IMAGE_SECTION_HEADER& header) : m_image(image), m_header(header) { }

    public:
        [[nodiscard]] const IMAGE_SECTION_HEADER* raw() const { return &m_header; }

        [[nodiscard]] std::shared_ptr<pe_image> image() const { return m_image; }

        [[nodiscard]] std::shared_ptr<remote_module> module() const { return std::dynamic_pointer_cast<remote_module>(m_image); }

        [[nodiscard]] addr base() const {
            const auto mod = module();
            return mod
                ? mod->buffer().base() + m_header.VirtualAddress
                : nullptr;
        }

        [[nodiscard]] region buffer(const pe_size size = pe_size::mapped) const {
            return { base(), size == pe_size::mapped ? m_header.Misc.VirtualSize : m_header.SizeOfRawData };
        }

        [[nodiscard]] std::string name() const {
            const auto* name_cstr = reinterpret_cast<const char*>(m_header.Name);
            return { name_cstr, strnlen(name_cstr, IMAGE_SIZEOF_SHORT_NAME) };
        }

        [[nodiscard]] std::size_t size() const {
            const auto mod = module();
            return mod ? m_header.Misc.VirtualSize : m_header.SizeOfRawData;
        }
    };

    enum class map_status : int {
        success,
        failed_allocation,
        failed_write
    };
}
