#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include <memory>

namespace hydra::_internal {
    static thread_local HWND _window_enum_result;
}

namespace hydra::window_cond {
    BOOL CALLBACK owner(HWND hwnd, LPARAM param);
}

namespace hydra {
    class window {
        HWND _handle;

        static HWND enumerate(WNDENUMPROC proc, DWORD proc_id);

    public:
        explicit window(const HWND handle) {
            _handle = handle;
        }

        void show() const {
            ShowWindow(_handle, SW_SHOW);
        }

        void hide() const {
            ShowWindow(_handle, SW_HIDE);
        }

        friend class process;
    };

    struct window_enum_ctx {
        std::shared_ptr<process> proc;
        HANDLE handle;

        explicit window_enum_ctx(const std::shared_ptr<process>& proc) {
            this->proc = proc;
            this->handle = nullptr;
        }
    };
}
