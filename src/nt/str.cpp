#include "hydra/nt/str.hpp"

#include <limits>

namespace hy::nt {
    unicode_string::unicode_string(const std::string_view src) {
        if (src.size() > std::numeric_limits<USHORT>::max()) {
            m_status = STATUS_NAME_TOO_LONG;
            return;
        }

        UTF8_STRING source;
        source.Length = static_cast<USHORT>(src.size());
        source.MaximumLength = source.Length;
        source.Buffer = const_cast<char*>(src.data());

        m_status = RtlUTF8StringToUnicodeString(&m_value, &source, true);
    }

    unicode_string::~unicode_string() {
        if (m_value.Buffer)
            RtlFreeUnicodeString(&m_value);
    }

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
