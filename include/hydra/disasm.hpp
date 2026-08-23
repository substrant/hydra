#pragma once

#include <hydra/stm.hpp>
#include <hydra/err.hpp>

#include <Zydis/Decoder.h>
#include <Zydis/Formatter.h>

#include <string>

namespace hy {
    struct disasm_ins : ZydisDecodedInstruction {
        ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];
        ptr pc;
    };

    struct disasm_func {
        ptr base;
        std::size_t size;
    };

    class disasm {
        stm* m_stream;
        ZydisDecoder m_decoder;
        ZydisFormatter m_formatter;
        bool m_owner;

        void init() {
            // todo: multi-arch rn x86-64 only - will have to use capstone or llvm tools for this though
            ZydisDecoderInit(&m_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            ZydisFormatterInit(&m_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        }

    public:
        explicit disasm(stm& stream) : m_stream(&stream), m_owner(false) { init(); }

        template <typename T> requires std::derived_from<std::remove_cvref_t<T>, stm> && (!std::is_lvalue_reference_v<T>)
        explicit disasm(T&& stream) : m_stream(new std::remove_cvref_t<T>(std::forward<T>(stream))), m_owner(true) { init(); }

        std::size_t position() const { return m_stream->position; }

        void step_bytes(const std::make_signed_t<std::size_t> count) const {
            m_stream->advance(count);
        }

        void step_align(const std::size_t factor) const {
            m_stream->advance(factor + (m_stream->position % factor));
        }

        bool is_aligned(const std::size_t factor) const {
            return (m_stream->position % factor) == 0;
        }

        err step_ins(disasm_ins* ins, bool operands = true) const;

        err step_func_prologue(disasm_func* func) const;

        err step_func_epilogue(disasm_func* func) const;

        err step_func(disasm_func* func) const;

        std::string format_ins(const disasm_ins* ins) const;

        std::string format_func(const disasm_func* func) const;

        ~disasm() {
            if (m_owner && m_stream)
                delete m_stream;
        }
    };
}