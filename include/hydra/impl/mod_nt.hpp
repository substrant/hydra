#pragma once

#include <hydra/impl/mod.hpp>

#include <hydra/ptr.hpp>
#include <hydra/ost.hpp>
#include <hydra/err.hpp>

namespace hy {
    struct seg : impl::seg {
    protected:
        ptr image_base;

        std::size_t physical_offset;
        std::size_t virtual_offset;

        std::size_t physical_size;
        std::size_t virtual_size;

    public:
        inline std::size_t calc_base(const mod_state state, ptr base = -1) const {
            if (base == -1) base = image_base;
            return base + (state == mod_state::flat ? physical_offset : virtual_offset);
        }
        
        inline std::size_t calc_size(const mod_state state) const {
            return state == mod_state::flat ? physical_size : virtual_size;
        }

        explicit seg(const mod_state state, const ptr image_base, const IMAGE_SECTION_HEADER& header) : image_base(image_base) {
            mode = mem_mode::none;

            physical_offset = header.PointerToRawData;
            virtual_offset = header.VirtualAddress;

            physical_size = header.SizeOfRawData;
            virtual_size = header.Misc.VirtualSize;

            if (header.Characteristics & IMAGE_SCN_MEM_READ)
                mode |= mem_mode::read;

            if (header.Characteristics & IMAGE_SCN_MEM_WRITE)
                mode |= mem_mode::write;

            if (header.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                mode |= mem_mode::exec;

            this->base = calc_base(state);
            this->size = calc_size(state);

            const auto segment_name = reinterpret_cast<const char*>(header.Name);
            name = std::string(segment_name, strnlen(segment_name, sizeof(segment_name)));
        }
    };
}

namespace hy::shim {
    class mod : public impl::mod {
    protected:
        IMAGE_DOS_HEADER dos;
        IMAGE_NT_HEADERS nt;

    public:
        const std::string name{};

        explicit mod(const std::string& name, stm& stream, const mod_state state)
            : impl::mod(stream, state), name(std::move(name)) { }

        explicit mod(const std::string& name, stm&& stream, const mod_state state)
            : impl::mod(stream, state), name(std::move(name)) { }

        err parse(mod_state state = mod_state::inherit) override;

        std::size_t calc_size(mod_state state) override;

        std::generator<seg> segments(err* error) override;
    };
}
