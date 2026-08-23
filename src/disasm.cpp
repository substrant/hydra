#include <hydra/disasm.hpp>

#include <Zydis/Utils.h>
#include <sstream>

namespace {
    bool is_padding(const hy::disasm_ins& ins) {
        return ins.mnemonic == ZYDIS_MNEMONIC_NOP || ins.mnemonic == ZYDIS_MNEMONIC_INT3;
    }

    bool is_rsp(const ZydisDecodedOperand& op) {
        return op.type == ZYDIS_OPERAND_TYPE_REGISTER && op.reg.value == ZYDIS_REGISTER_RSP;
    }

    bool is_rbp(const ZydisDecodedOperand& op) {
        return op.type == ZYDIS_OPERAND_TYPE_REGISTER && op.reg.value == ZYDIS_REGISTER_RBP;
    }

    bool is_nonvolatile(const ZydisRegister reg) {
        switch (reg) {
        case ZYDIS_REGISTER_RBX:
        case ZYDIS_REGISTER_RBP:
        case ZYDIS_REGISTER_RSI:
        case ZYDIS_REGISTER_RDI:
        case ZYDIS_REGISTER_R12:
        case ZYDIS_REGISTER_R13:
        case ZYDIS_REGISTER_R14:
        case ZYDIS_REGISTER_R15:
            return true;
        default:
            return false;
        }
    }

    bool is_push_prologue(const hy::disasm_ins& ins) {
        if (ins.mnemonic != ZYDIS_MNEMONIC_PUSH || ins.operand_count_visible < 1)
            return false;

        return ins.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER && is_nonvolatile(ins.ops[0].reg.value);
    }

    bool is_store_prologue(const hy::disasm_ins& ins) {
        if (ins.mnemonic != ZYDIS_MNEMONIC_MOV || ins.operand_count_visible < 2)
            return false;

        if (ins.ops[0].type != ZYDIS_OPERAND_TYPE_MEMORY || ins.ops[0].mem.base != ZYDIS_REGISTER_RSP)
            return false;

        return ins.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER && is_nonvolatile(ins.ops[1].reg.value);
    }

    bool is_stack_alloc(const hy::disasm_ins& ins) {
        return ins.mnemonic == ZYDIS_MNEMONIC_SUB && ins.operand_count_visible >= 2 && is_rsp(ins.ops[0]) && ins.ops[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
    }

    bool is_frame_setup(const hy::disasm_ins& ins) {
        if (ins.operand_count_visible < 2)
            return false;

        if (ins.mnemonic == ZYDIS_MNEMONIC_MOV && is_rbp(ins.ops[0]) && is_rsp(ins.ops[1]))
            return true;

        if (ins.mnemonic == ZYDIS_MNEMONIC_LEA && is_rbp(ins.ops[0]) && ins.ops[1].type == ZYDIS_OPERAND_TYPE_MEMORY && ins.ops[1].mem.base == ZYDIS_REGISTER_RSP)
            return true;

        return false;
    }

    bool is_terminal(const hy::disasm_ins& ins) {
        return ins.mnemonic == ZYDIS_MNEMONIC_RET || ins.mnemonic == ZYDIS_MNEMONIC_JMP;
    }
}

namespace hy {
    err disasm::step_ins(disasm_ins* ins, const bool operands) const {
        ZyanStatus status;
        ZydisDecoderContext context;

        constexpr auto max_ins_size = 15;
        std::int8_t ins_buffer[max_ins_size]{};

        if (!m_stream->read(ins_buffer, max_ins_size))
            return STA_END_OF_STREAM;

        if (!ZYAN_SUCCESS(status = ZydisDecoderDecodeInstruction(
            &m_decoder,
            &context,
            ins_buffer,
            max_ins_size,
            ins
        ))) return STA_DISASM_DECODE_FAIL_INSTRUCTION;
        
        if (operands && !ZYAN_SUCCESS(status = ZydisDecoderDecodeOperands(
            &m_decoder,
            &context,
            ins,
            ins->ops,
            ins->operand_count
        ))) return STA_DISASM_DECODE_FAIL_OPERANDS;

        ins->pc = m_stream->position;
        m_stream->position += ins->length;

        return STA_SUCCESS;
    }

    err disasm::step_func_prologue(disasm_func* func) const {
        err status;
        disasm_ins ins{};

        do {
            const auto save = m_stream->position;

            if ((status = step_ins(&ins)) != STA_SUCCESS)
                return status;

            if (!is_padding(ins)) {
                m_stream->position = save;
                break;
            }
        } while (true);

        func->base = m_stream->position;
        func->size = 0;

        do {
            const auto save = m_stream->position;

            if ((status = step_ins(&ins)) != STA_SUCCESS)
                return status;

            if (!is_push_prologue(ins) && !is_store_prologue(ins) && !is_stack_alloc(ins) && !is_frame_setup(ins)) {
                m_stream->position = save;
                break;
            }
        } while (true);

        return STA_SUCCESS;
    }

    err disasm::step_func_epilogue(disasm_func* func) const {
        err status;
        disasm_ins ins{};

        do {
            if ((status = step_ins(&ins)) != STA_SUCCESS)
                return status;

            if (!is_terminal(ins))
                continue;

            const auto end = m_stream->position;

            do {
                const auto save = m_stream->position;

                if ((status = step_ins(&ins)) != STA_SUCCESS) {
                    if (status == STA_END_OF_STREAM) {
                        func->size = end - func->base;
                        return STA_SUCCESS;
                    }

                    return status;
                }

                if (!is_padding(ins)) {
                    m_stream->position = save;
                    break;
                }

                if (!(m_stream->position % 16)) {
                    func->size = end - func->base;
                    return STA_SUCCESS;
                }
            } while (true);
        } while (true);
    }

    err disasm::step_func(disasm_func* func) const {
        err status;

        if ((status = step_func_prologue(func)) != STA_SUCCESS)
            return status;

        if ((status = step_func_epilogue(func)) != STA_SUCCESS)
            return status;

        return STA_SUCCESS;
    }

    std::string disasm::format_ins(const disasm_ins* ins) const {
        constexpr auto result_max_size = 256;
        std::string text(result_max_size, '\0');

        if (!ZYAN_SUCCESS(ZydisFormatterFormatInstruction(
            &m_formatter,
            ins,
            ins->ops,
            ins->operand_count_visible,
            text.data(),
            result_max_size,
            ins->pc,
            ZYAN_NULL
        ))) return "<unknown>"; // todo: returns invalid not unknown so this is not executing

        text.resize(std::strlen(text.data()));
        return text;
    }

    std::string disasm::format_func(const disasm_func* func) const {
        disasm_ins ins;
        std::stringstream ss;
        
        const auto func_end = func->base + func->size;
        const auto save = m_stream->position;

        m_stream->position = func->base;
        while (m_stream->position < func_end && step_ins(&ins) == STA_SUCCESS)
            ss << format_ins(&ins) << "\n";

        m_stream->position = save;
        return ss.str();
    }
}
