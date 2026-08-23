#pragma once

#if defined(_WIN32)
#   define HY_OS_NT
#elif defined(__APPLE__) && defined(__MACH__)
#   define HY_OS_DARWIN
#   define HY_OS_UNIX
#   define HY_OS_UNIX_LIKE
#elif defined(__linux__)
#   define HY_OS_LINUX
#   define HY_OS_UNIX_LIKE
#elif defined(__FreeBSD__)
#   define HY_OS_BSD
#   define HY_OS_UNIX_LIKE
#elif defined(__unix__) || defined(__unix)
#   define HY_OS_UNIX
#else
#   error Unknown platform
#endif

#ifdef HY_OS_NT
#    define NOMINMAX
#    define WIN32_LEAN_AND_MEAN
#    include <phnt_windows.h>
#    include <phnt.h>
#endif
