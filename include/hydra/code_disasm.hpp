#pragma once

#include <functional>

#include "detail.hpp"
#include "memory.hpp"

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

    struct code_ins {
        addr                    offset{};
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
        using predicate = std::function<bool(const code_ins&)>;

        static predicate opcode(const std::uint8_t opcode) {
            return [=](const code_ins& instr) -> bool {
                return instr.opcode() == opcode;
            };
        }

        static predicate disp(const int index, const addr value) {
            return [=](const code_ins& instr) -> bool {
                if (index > std::max(instr.operand_count() - 1, 0))
                    return false;

                const auto operand = instr[index];
                if (!operand.mem.disp.has_displacement)
                    return false;

                return operand.mem.disp.value == value;
            };
        }

        static predicate reg(const int index, const ZydisRegister reg) {
            return [=](const code_ins& instr) -> bool {
                if (index > std::max(instr.operand_count() - 1, 0))
                    return false;

                const auto operand = instr[index];
                return operand.reg.value == reg;
            };
        }

        template <class ...T>
        static predicate any(const T ...predicates) {
            return [=](const code_ins& instr) -> bool {
                return (predicates(instr) || ...);
            };
        }

        template <class... T>
        static predicate all(T... predicates) {
            return [=](const code_ins& instr) -> bool {
                return (predicates(instr) && ...);
            };
        }
    };

    class code_disasm {
        region m_buffer;
        addr m_rip = nullptr; // Remote base address of buffer
        addr m_off = 0ull;

        ZyanStatus     m_status = 0;
        ZydisDecoder   m_decoder;
        ZydisFormatter m_fmt;

    public:
        explicit code_disasm(
            region buffer,
            ZydisMachineMode mode = ZYDIS_MACHINE_MODE_LONG_64,
            ZydisStackWidth width = ZYDIS_STACK_WIDTH_64
        ) : m_buffer(std::move(buffer)) {

            ZydisDecoderInit(&m_decoder, mode, width);
            ZydisFormatterInit(&m_fmt, ZYDIS_FORMATTER_STYLE_INTEL);
        }

        code_disasm save() const {
            return { *this };
        }

        addr pos() const {
            return m_buffer.base() + m_off;
        }

        std::string format(const code_ins* instr);

        bool read(code_ins* pc = nullptr, std::size_t size = 0);

        bool step(code_ins* pc = nullptr, std::size_t size = 0);

        bool skip(int n = 1);

        bool match(const std::uint8_t* pattern, const char* mask, addr stop_off, direction dir = direction::forwards);

        std::optional<code_disasm> find_impl(std::size_t limit, const code_query::predicate& master_predicate) const;

        template <class... T>
        std::optional<code_disasm> find(const std::size_t limit, T ...predicates) {
            const auto master_predicate = code_query::all(predicates...);
            return find_impl(limit, master_predicate);
        }
    };
}
