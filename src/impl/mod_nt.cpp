#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/mod.hpp>

namespace hy::shim {
    err mod::parse(hy::mod& owner, mod_state state) {
        owner.base = owner.m_stream->seek(stm_origin::begin);

        if (owner.m_stream->read(&m_dos) != sizeof(m_dos))
            return STA_PARTIAL_READ;

        if (m_dos.e_magic != IMAGE_DOS_SIGNATURE)
            return (err)-100;

        if (!m_dos.e_lfanew)
            return (err)-101;

        owner.m_stream->seek(stm_origin::begin, m_dos.e_lfanew);

        if (owner.m_stream->read(&m_nt) != sizeof(m_nt))
            return STA_PARTIAL_READ;

        if (m_nt.Signature != IMAGE_NT_SIGNATURE)
            return (err)-200;

        owner.size = calc_size(owner, state);
        return STA_SUCCESS;
    }

    std::size_t mod::calc_size(hy::mod& owner, mod_state state) {
        if (state == mod_state::inherit)
            state = owner.m_state;

        err error = STA_SUCCESS; // todo

        if (state == mod_state::header) {
            auto generator = segments(owner, &error);
            const auto first_it = generator.begin();
            if (error != STA_SUCCESS) return sizeof(m_dos) + sizeof(m_nt);

            return (*first_it).base - owner.base;
        }
        else {
            std::size_t max_end = 0;

            for (const auto& segment : segments(owner, &error)) {
                const auto end = segment.calc_base(state) + segment.calc_size(state);
                max_end = std::max<size_t>(end, max_end);
            }

            return max_end - owner.base;
        }
    }

    std::generator<seg> mod::segments(hy::mod& owner, err* error) {
        const auto count = m_nt.FileHeader.NumberOfSections;
        if (count == 0) {
            if (error) *error = STA_PARTIAL_READ;
            co_return;
        }

        const auto offset = FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + m_nt.FileHeader.SizeOfOptionalHeader;
        if (!offset) {
            if (error) *error = STA_PARTIAL_READ;
            co_return;
        }

        IMAGE_SECTION_HEADER header;
        owner.m_stream->seek(stm_origin::begin, m_dos.e_lfanew + offset);

        for (WORD i = 0; i < count; i++) {
            if (!owner.m_stream->read(&header)) {
                if (error) *error = STA_PARTIAL_READ;
                co_return;
            }

            co_yield seg(owner.m_state, owner.base, header);
        }
    }
}

#endif
