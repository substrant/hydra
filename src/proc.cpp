#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/nt/err.hpp>
#include <hydra/nt/str.hpp>
#include <hydra/proc.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace hy {
    std::generator<pid_t> proc::find_by_mod(const std::string name) {
        const nt::unicode_string target(name);
        if (!NT_SUCCESS(nt::err = target.status()))
            co_return;

        ULONG size = 0;
        nt::err = NtQuerySystemInformation(
            SystemProcessInformation,
            nullptr,
            0,
            &size
        );

        if (nt::err != STATUS_INFO_LENGTH_MISMATCH)
            co_return;

        std::vector<std::byte> buffer;
        for (;;) {
            buffer.resize(size);

            ULONG required_size = 0;
            nt::err = NtQuerySystemInformation(
                SystemProcessInformation,
                buffer.data(),
                size,
                &required_size
            );

            if (nt::err != STATUS_INFO_LENGTH_MISMATCH)
                break;

            size = std::max(required_size, size * 2);
        }

        if (!NT_SUCCESS(nt::err))
            co_return;

        auto* info = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(buffer.data());
        for (;;) {
            if (info->ImageName.Buffer && RtlEqualUnicodeString(&info->ImageName, target.get(), true))
                co_yield pid_t(info->UniqueProcessId);

            if (!info->NextEntryOffset)
                break;

            info = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
                reinterpret_cast<std::uint8_t*>(info) + info->NextEntryOffset
            );
        }
    }

    std::generator<pid_t> proc::find_by_window(const std::string name) {
        const nt::unicode_string target(name);
        if (!NT_SUCCESS(nt::err = target.status()))
            co_return;

        nt::err = STA_SUCCESS;

        HWND after = nullptr;
        for (;;) {
            const auto window = NtUserFindWindowEx(
                nullptr,
                after,
                nullptr,
                target.get(),
                FW_BOTH
            );

            if (!window) break;
            after = window;

            const auto pid = static_cast<DWORD>(NtUserQueryWindow(window, WindowProcess));
            if (pid) co_yield pid_t(pid);
        }
    }
}

#endif
