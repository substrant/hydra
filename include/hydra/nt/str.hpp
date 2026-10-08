#pragma once

#include <hydra/ost.hpp>

#include <string>
#include <string_view>

namespace hy::nt {
    class unicode_string {
        UNICODE_STRING m_value{};
        NTSTATUS m_status;

    public:
        explicit unicode_string(std::string_view src);
        unicode_string(const unicode_string&) = delete;
        unicode_string& operator=(const unicode_string&) = delete;

        [[nodiscard]] NTSTATUS status() const { return m_status; }
        [[nodiscard]] PCUNICODE_STRING get() const { return &m_value; }

        ~unicode_string();
    };

    inline std::wstring unicode_to_wstring(const UNICODE_STRING& src) {
        return std::wstring{ src.Buffer, src.Length / sizeof(wchar_t) };
    }

    std::string unicode_to_string(const UNICODE_STRING& src);

    std::string unicode_to_string(const std::wstring& src);
}