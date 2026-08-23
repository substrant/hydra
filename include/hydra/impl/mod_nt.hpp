#pragma once

#include <hydra/impl/mod.hpp>

#include <hydra/ptr.hpp>
#include <hydra/ost.hpp>
#include <hydra/err.hpp>

namespace hy {
    struct seg : impl::seg {
        explicit seg(const mod_state state, const ptr image_base, const IMAGE_SECTION_HEADER& header) {
            this->base = image_base + (state == mod_state::flat ? header.PointerToRawData : header.VirtualAddress);
            this->size = state == mod_state::flat ? header.SizeOfRawData : header.Misc.VirtualSize;

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
        using impl::mod::mod;

        err parse() override;

        std::generator<seg> segments(err* error) override;
    };
}
