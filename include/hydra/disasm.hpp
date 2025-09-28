#pragma once

#include <hydra/detail/pch.hpp>
#include <hydra/memory.hpp>

#pragma comment(lib, "Zydis.lib")
#pragma comment(lib, "Zycore.lib")

extern "C" {
#   include "Zydis/Zydis.h"
}

namespace hy {
    constexpr std::size_t max_instr_len = 0x0F;

    template <class T>
    concept ImmRegister = std::same_as<T, ZyanI64> || std::same_as<T, ZyanU64>;

    enum class direction : std::uint8_t {
        forwards,
        backwards
    };

    struct disasm_instr {
        mem::addr               offset{};
        ZydisDecodedInstruction data{};
        ZydisDecodedOperand     args[ZYDIS_MAX_OPERAND_COUNT]{};

        std::uint8_t opcode() const {
            return data.opcode;
        }

        int operand_count() const {
            return data.operand_count;
        }

        std::size_t size() const {
            return data.length;
        }

        const ZydisDecodedOperand& operator[](const std::size_t index) const {
            return args[index];
        }
    };

    struct code_query {
        using predicate = std::function<bool(const disasm_instr&)>;

        static predicate opcode(const std::uint8_t opcode) {
            return [=](const disasm_instr& instr) -> bool {
                return instr.opcode() == opcode;
            };
        }

        static predicate disp(const int index, const mem::addr value) {
            return [=](const disasm_instr& instr) -> bool {
                if (index > std::max(instr.operand_count() - 1, 0))
                    return false;

                const auto operand = instr[index];
                if (!operand.mem.disp.has_displacement)
                    return false;

                return operand.mem.disp.value == value;
            };
        }

        static predicate reg(const int index, const ZydisRegister reg) {
            return [=](const disasm_instr& instr) -> bool {
                if (index > std::max(instr.operand_count() - 1, 0))
                    return false;

                const auto operand = instr[index];
                return operand.reg.value == reg;
            };
        }

        template <class ...T>
        static predicate any(const T ...predicates) {
            return [=](const disasm_instr& instr) -> bool {
                return (predicates(instr) || ...);
            };
        }

        template <class... T>
        static predicate all(T... predicates) {
            return [=](const disasm_instr& instr) -> bool {
                return (predicates(instr) && ...);
            };
        }
    };

    class disasm {
    public:
        mem::buffer m_buffer;
        mem::addr m_rip = nullptr; // Remote base address of buffer
        mem::addr m_off = 0ull;

        ZyanStatus     m_status = 0;
        ZydisDecoder   m_decoder;
        ZydisFormatter m_formatter;

        explicit disasm(mem::buffer buffer) : m_buffer(std::move(buffer)) {
            ZydisDecoderInit(&m_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            ZydisFormatterInit(&m_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        }

        disasm save() const {
            return { *this };
        }

        mem::addr pos() const {
            return m_buffer.base() + m_off;
        }

        std::string format(const disasm_instr* instr);

        bool read(disasm_instr* pc = nullptr, std::size_t size = 0);

        bool step(disasm_instr* pc = nullptr, std::size_t size = 0);

        bool skip(int n = 1);

        bool match(const std::uint8_t* pattern, const char* mask, mem::addr stop_off, direction dir = direction::forwards);

        std::optional<disasm> find_impl(std::size_t limit, const code_query::predicate& master_predicate) const;

        template <class... T>
        std::optional<disasm> find(const std::size_t limit, T ...predicates) {
            const auto master_predicate = code_query::all(predicates...);
            return find_impl(limit, master_predicate);
        }
    };
}
