#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include <functional>
#include <optional>
#include <memory>
#include <generator>

#include <hydra/handle.hpp>
#include <hydra/decl/th32_phnt.h>

namespace hy::toolhelp {
    template <class SnapClass>
    using callback = BOOL(WINAPI*)(HANDLE, SnapClass*);

    template <class SnapClass>
    using predicate = std::function<bool(SnapClass&)>;

    template <DWORD SnapFlags, class SnapClass, callback<SnapClass> QueryFirst, callback<SnapClass> QueryNext>
    std::generator<SnapClass> scan(const DWORD pid, std::optional<predicate<SnapClass>> predicate = std::nullopt) {
        const handle<CloseHandle> handle{CreateToolhelp32Snapshot(SnapFlags, pid)};

        SnapClass object;
        object.dwSize = sizeof(SnapClass);

        for (BOOL success = QueryFirst(handle, &object); success; success = QueryNext(handle, &object)) {
            if (!predicate.has_value() || (*predicate)(object))
                co_yield object;
        }
    }

    inline auto get_processes() {
        return scan<TH32CS_SNAPPROCESS, PROCESSENTRY32, Process32First, Process32Next>(NULL);
    }

    inline auto get_modules(const DWORD pid = NULL) {
        return scan<TH32CS_SNAPMODULE, MODULEENTRY32, Module32First, Module32Next>(pid);
    }

    inline auto get_threads(const DWORD pid = NULL) {
        return scan<TH32CS_SNAPTHREAD, THREADENTRY32, Thread32First, Thread32Next>(NULL, [pid](const THREADENTRY32& te) -> bool {
            return te.th32OwnerProcessID == pid;
        });
    }
}
