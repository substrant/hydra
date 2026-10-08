#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/impl/mod_nt.hpp>

namespace hy::shim {
    err mod::parse(mod_state state) {
        base = m_stream->seek(stm_origin::begin);

        if (m_stream->read(&dos) != sizeof(dos))
            return STA_PARTIAL_READ;

        if (dos.e_magic != IMAGE_DOS_SIGNATURE)
            return (err)-100;

        if (!dos.e_lfanew)
            return (err)-101;

        m_stream->seek(stm_origin::begin, dos.e_lfanew);

        if (m_stream->read(&nt) != sizeof(nt))
            return STA_PARTIAL_READ;

        if (nt.Signature != IMAGE_NT_SIGNATURE)
            return (err)-200;

        size = calc_size(state);
        return STA_SUCCESS;
    }

    std::size_t mod::calc_size(mod_state state) {
        if (state == mod_state::inherit)
            state = m_state;

        err error = STA_SUCCESS; // todo

        if (state == mod_state::header) {
            const auto first_it = segments(&error).begin();
            if (error != STA_SUCCESS) return sizeof(dos) + sizeof(nt);

            return (*first_it).base - base;
        }
        else {
            std::size_t max_end = 0;

            for (const auto& segment : segments(&error)) {
                const auto end = segment.calc_base(state) + segment.calc_size(state);
                max_end = std::max<size_t>(end, max_end);
            }

            return max_end - base;
        }
    }

    std::generator<seg> mod::segments(err* error) {
        const auto count = nt.FileHeader.NumberOfSections;
        if (count == 0) {
            *error = STA_PARTIAL_READ;
            co_return;
        }

        const auto offset = FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        if (!offset) {
            *error = STA_PARTIAL_READ;
            co_return;
        }

        IMAGE_SECTION_HEADER header;
        m_stream->seek(stm_origin::begin, dos.e_lfanew + offset);

        for (WORD i = 0; i < count; i++) {
            if (!m_stream->read(&header)) {
                *error = STA_PARTIAL_READ;
                co_return;
            }

            co_yield seg(m_state, base, header);
        }
    }
}

#endif
