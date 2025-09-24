#pragma once

#include "hydra/user/window.hpp"

struct enum_context {
    DWORD proc_id = 0;
    HWND hwnd = nullptr;
};

namespace hydra::window_cond {
    BOOL CALLBACK owner(const HWND hwnd, const LPARAM param) {
        const auto ctx = reinterpret_cast<enum_context*>(param);

        DWORD proc_id;
        GetWindowThreadProcessId(hwnd, &proc_id);

        if (proc_id == ctx->proc_id && GetWindow(hwnd, GW_OWNER) == nullptr) {
            ctx->hwnd = hwnd;
            return FALSE;
        }

        return TRUE;
    }
}

HWND hydra::window::enumerate(const WNDENUMPROC proc, const DWORD proc_id) {
    enum_context ctx { .proc_id = proc_id };
    EnumWindows(proc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.hwnd;
}
