#pragma once

#include <cstdint>

namespace hy {
    enum err : std::int32_t {
        ERR_NATIVE_ERROR = -INT32_MAX,
        STA_PARTIAL_READ,
        STA_END_OF_STREAM,
        STA_DISASM_DECODE_FAIL_INSTRUCTION,
        STA_DISASM_DECODE_FAIL_OPERANDS,
        STA_SUCCESS = 1,
    };
}