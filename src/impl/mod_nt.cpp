#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/impl/mod_nt.hpp>

namespace hy::shim {
    err mod::parse() {
        m_stream->seek(stm_origin::begin);

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

        return STA_SUCCESS;
    }

    std::generator<seg> mod::segments(err* error) {
        const auto count = nt.FileHeader.NumberOfSections;
        const auto offset = FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;

        if (!offset) {
            *error = STA_PARTIAL_READ;
            co_return;
        }

        IMAGE_SECTION_HEADER header;
        const auto image_base = m_stream->seek(stm_origin::begin);

        m_stream->seek(stm_origin::begin, dos.e_lfanew + offset);

        for (WORD i = 0; i < count; i++) {
            if (!m_stream->read(&header)) {
                *error = STA_PARTIAL_READ;
                co_return;
            }

            co_yield seg(m_state, image_base, header);
        }
    }
}

#endif
