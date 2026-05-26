#pragma once

#include <phnt_windows.h>
#include <filesystem>
#include <ranges>
#include <unordered_set>

#include <hydra/memory.hpp>
#include <hydra/stream.hpp>
#include <hydra/remote_stream.hpp>

namespace hy {
    // Forward-decl
    class process;
    class pe_image;

    // status > 0 = yay
    enum class pe_status : std::int8_t {
        bad_internal_error = INT8_MIN,
        bad_dos_signature,
        bad_nt_offset,
        bad_nt_signature,
        bad_sections,
        buffer_too_small,
        success = 0
    };

    // determines the source of the PE data
    // inherit = use parent image's source
    enum class pe_source : std::uint8_t {
        inherit,
        header,
        mapped,
        file
    };

    enum class pe_location : std::uint8_t {
        relative,
        absolute
    };

    // what to read/write from the stream
    enum class pe_scope : std::uint8_t {
        dos_header = 1,
        nt_headers = 2,
        sections   = 4,
        headers    = dos_header | nt_headers,
        all        = headers | sections
    };

    // todo: helper macro
    constexpr pe_scope operator|(pe_scope a, pe_scope b) {
        return static_cast<pe_scope>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
    }

    constexpr pe_scope operator&(pe_scope a, pe_scope b) {
        return static_cast<pe_scope>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
    }

    template <typename T, typename U>
    requires std::is_same_v<T, pe_scope> || std::is_integral_v<T> &&
             std::is_same_v<U, pe_scope> || std::is_integral_v<U>
    constexpr bool operator!=(T a, U b) {
        return static_cast<std::uint8_t>(a) != static_cast<std::uint8_t>(b);
    } // yeah add this to detail this is going to get bad

    class pe_section { // todo: we need a rebuild fn just refer to what scylla does
        const pe_image* const m_image; // raw ptr lifetime of pe_image
        IMAGE_SECTION_HEADER m_header;

    public:
        explicit pe_section(const pe_image* image, const IMAGE_SECTION_HEADER&& header) : m_image(image), m_header(header) {}

        [[nodiscard]] const IMAGE_SECTION_HEADER* raw() const { return &m_header; }

        [[nodiscard]] addr offset(pe_source source = pe_source::inherit) const;

        [[nodiscard]] addr base() const;

        [[nodiscard]] region buffer(pe_source source = pe_source::inherit) const;

        [[nodiscard]] std::string name() const;

        [[nodiscard]] std::size_t size(pe_source source = pe_source::inherit) const;

        [[nodiscard]] bool executable() const {
            return (m_header.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        }
    };

    class pe_image {
        friend class pe_section;

    protected:
        std::unique_ptr<memory_stream> m_stream;
        pe_source m_source;

        IMAGE_DOS_HEADER m_dos_header;
        IMAGE_NT_HEADERS m_nt_headers;

        std::unordered_map<std::string, pe_section> m_sections{};

        HYDRA_INTERNAL("Use 'pe_image::load' to load PE images.")
        explicit pe_image(std::unique_ptr<memory_stream>&& stm, pe_source source)
            : m_stream(std::move(stm)), m_source(source) { }

        static pe_source canonical_source(pe_source source, const pe_image* image) {
            if (source == pe_source::inherit) {
                if (!image) throw std::runtime_error("cannot inherit source from null image");
                source = image->m_source;
            }
            return source;
        }

    public:
        auto dos_header() { return &m_dos_header; }

        auto nt_headers() { return &m_nt_headers; }

        explicit operator PIMAGE_NT_HEADERS() { return &m_nt_headers; }

        static pe_image load(const std::filesystem::path& path);

        static pe_image load(std::unique_ptr<memory_stream>&& stm, const pe_source source) {
            return pe_image(std::move(stm), source);
        }

        static pe_image load_base(addr base);

        pe_status read(pe_scope scope = pe_scope::all);

        std::string file_type() const; // make rebase fn

        auto sections() const {
            std::vector<const pe_section*> sorted_sections;
            sorted_sections.reserve(m_sections.size());

            for (auto& section : m_sections | std::views::values)
                sorted_sections.push_back(&section);

            // in loaded memory order
            std::ranges::sort(sorted_sections, [](const pe_section* a, const pe_section* b) -> bool {
                return a->offset(pe_source::mapped).i < b->offset(pe_source::mapped).i;
            });

            return sorted_sections;
        }

        std::optional<pe_section> section(std::string_view target_name) const;

        std::size_t size(pe_source size_type = pe_source::file) const;

        addr resolve_rva(DWORD rva, pe_source source) const;

    protected:
        addr internal_get_import(std::string_view symbol, pe_source source = pe_source::inherit, pe_location loc = pe_location::absolute) const;
        addr internal_get_export(std::string_view symbol, pe_source source = pe_source::inherit, pe_location loc = pe_location::absolute) const;

    public:
        template <pe_location Location>
        auto get_import(const std::string_view symbol, const pe_source source = pe_source::inherit) const {
            auto result = internal_get_import(symbol, source, Location);

            if constexpr (Location == pe_location::absolute)
                return result.i;
            else
                return static_cast<std::uint32_t>(result.i);
        }

        template <pe_location Location>
        auto get_export(const std::string_view symbol, const pe_source source = pe_source::inherit) const {
            auto result = internal_get_export(symbol, source, Location);

            if constexpr (Location == pe_location::absolute)
                return result.i;
            else
                return static_cast<std::uint32_t>(result.i);
        }

        region region() const {
            return { m_stream->base(), size() };
        }

        [[nodiscard]] addr base() const { return m_stream->base(); }

        [[nodiscard]] hy::region buffer() const { return { m_stream->base(), size(pe_source::mapped) }; }
    };

    class remote_module : public pe_image, public detail::noncopyable {
        std::shared_ptr<process> m_proc;
        std::optional<std::string> m_name; // just assume if name is nullopt then path is unknown
        std::filesystem::path m_path;

    public:
        explicit remote_module(std::shared_ptr<process> proc, addr base, std::optional<std::string> name = std::nullopt, std::filesystem::path path = {});

        std::shared_ptr<process> proc() const { return m_proc; }

        //bool dump_image(const hy::buffer& buffer, dump_context* ctx);

        std::string file_name() const { return m_name.value_or("<unknown>"); }

        std::filesystem::path file_path() const { return m_path; }
    };
}
