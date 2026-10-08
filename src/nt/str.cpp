#include "hydra/nt/str.hpp"

namespace hy::nt {
    std::string unicode_to_string(const UNICODE_STRING& src) {
        if (src.Buffer == nullptr || src.Length == 0)
            return "";

        const int count = static_cast<int>(src.Length) / static_cast<int>(sizeof(wchar_t));
        const auto size = WideCharToMultiByte(CP_UTF8, 0, src.Buffer, count, nullptr, 0, nullptr, nullptr);

        if (size <= 0)
            return "";

        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, src.Buffer, count, result.data(), size, nullptr, nullptr);

        return result;
    }

    std::string unicode_to_string(const std::wstring& src) {
        UNICODE_STRING nt_src;
        nt_src.Buffer = const_cast<PWCH>(src.data());
        nt_src.Length = static_cast<USHORT>(src.size() * sizeof(wchar_t));
        return unicode_to_string(nt_src);
    }
}
