#include "hydra/disasm.hpp"

namespace {
    thread_local hy::disasm_instr dummy_instr;
}

namespace hy {
    bool disasm::read(disasm_instr* pc, const std::size_t size) {
        if (!pc) pc = &dummy_instr;
        m_status = ZydisDecoderDecodeFull(&m_decoder, pos(), size, &pc->data, &pc->args[0]);
        return ZYAN_SUCCESS(m_status);
    }

    bool disasm::step(disasm_instr* pc, const std::size_t size) {
        const auto success = read(pc, size);
        if (success) m_off += pc->size();
        return success;
    }

    bool disasm::skip(int n) {
        for (; n > 0; n--)
            if (!step(nullptr)) return false;
        return true;
    }

    bool disasm::match(const std::uint8_t* pattern, const char* mask, const addr stop_off, const direction dir) {
        const std::intptr_t step = (dir == direction::forwards) ? 1 : -1;
        const std::size_t pattern_size = strlen(mask);

        addr current = pos();
        const addr end = current + stop_off;

        // Early exit if we're already out of bounds
        if ((dir == direction::forwards && current >= end) || (dir == direction::backwards && current <= end))
            return false;

        while ((dir == direction::forwards && current < end) || (dir == direction::backwards && current > end)) {
            if (region::match_aob(current, pattern, mask, pattern_size)) {
                m_off = current - m_buffer.base(); // update position only on match
                return true;
            }

            current += step;
        }

        return false;
    }

    std::optional<disasm> disasm::find_impl(const std::size_t limit, const code_query::predicate& master_predicate) const {
        auto preview = save();
        const auto max_pos = pos() + limit;

        disasm_instr pc;
        do {
            const auto bytes_left = max_pos - pos();
            if (!preview.step(&pc, bytes_left)) return std::nullopt;
        } while (!master_predicate(pc) || pos() >= max_pos);

        return preview;
    }

    std::string disasm::format(const disasm_instr* instr) {
        char buf[0xFF];

        m_status = ZydisFormatterFormatInstruction(&m_formatter, &instr->data, instr->args, instr->data.operand_count, buf, sizeof(buf), m_rip + instr->offset, ZYAN_NULL);
        if (!ZYAN_SUCCESS(m_status)) buf[0] = '\0';

        return { buf };
    }
}
