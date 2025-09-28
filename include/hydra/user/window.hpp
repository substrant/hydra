#pragma once

#include <hydra/detail/pch.hpp>

namespace hy {
    namespace detail {
        struct find_window_context {
            DWORD proc_id = 0;
            HWND hwnd = nullptr;
        };
    }

    class window {
        HWND m_handle;

    public:
        static HWND find(WNDENUMPROC proc, DWORD proc_id);

        static BOOL CALLBACK match_owner(const HWND hwnd, const LPARAM param);

        explicit window(const HWND handle) {
            m_handle = handle;
        }

        void show() const {
            ShowWindow(m_handle, SW_SHOW);
        }

        void hide() const {
            ShowWindow(m_handle, SW_HIDE);
        }
    };
}
