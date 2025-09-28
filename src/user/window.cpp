#pragma once

#include <hydra/user/window.hpp>

HWND hy::window::find(const WNDENUMPROC proc, const DWORD proc_id) {
    detail::find_window_context ctx { .proc_id = proc_id };
    EnumWindows(proc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd;
}

BOOL hy::window::match_owner(const HWND hwnd, const LPARAM param) {
    const auto ctx = reinterpret_cast<detail::find_window_context*>(param);

    DWORD proc_id;
    GetWindowThreadProcessId(hwnd, &proc_id);

    if (proc_id == ctx->proc_id && GetWindow(hwnd, GW_OWNER) == nullptr) {
        ctx->hwnd = hwnd;
        return FALSE;
    }

    return TRUE;
}
