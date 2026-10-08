#pragma once

#include <hydra/ost.hpp>
#include <string>

namespace hy::nt {
    inline std::wstring unicode_to_wstring(const UNICODE_STRING& src) {
        return std::wstring{ src.Buffer, src.Length / sizeof(wchar_t) };
    }

    std::string unicode_to_string(const UNICODE_STRING& src);

    std::string unicode_to_string(const std::wstring& src);
}