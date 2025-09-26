#pragma once

#include "detail/pch.hpp"

#include "handle.hpp"
#include "detail/generator.hpp"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#ifndef MAX_MODULE_NAME32
#define MAX_MODULE_NAME32 255
#endif

// Snapshot flags
#define TH32CS_SNAPHEAPLIST 0x00000001
#define TH32CS_SNAPPROCESS  0x00000002
#define TH32CS_SNAPTHREAD   0x00000004
#define TH32CS_SNAPMODULE   0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
#define TH32CS_SNAPALL      (TH32CS_SNAPHEAPLIST | TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD | TH32CS_SNAPMODULE)
#define TH32CS_INHERIT      0x80000000

// Snapshot creation
HANDLE WINAPI
CreateToolhelp32Snapshot(
    DWORD dwFlags,
    DWORD th32ProcessID
);

// Heap walking ----------------------------------------------------------
typedef struct tagHEAPLIST32 {
    SIZE_T     dwSize;
    DWORD      th32ProcessID;  // owning process
    ULONG_PTR  th32HeapID;     // heap (in owning process's context!)
    DWORD      dwFlags;
} HEAPLIST32, *PHEAPLIST32, *LPHEAPLIST32;

#define HF32_DEFAULT 1
#define HF32_SHARED  2

BOOL WINAPI Heap32ListFirst(HANDLE hSnapshot, LPHEAPLIST32 lphl);
BOOL WINAPI Heap32ListNext(HANDLE hSnapshot, LPHEAPLIST32 lphl);

typedef struct tagHEAPENTRY32 {
    SIZE_T     dwSize;
    HANDLE     hHandle;
    ULONG_PTR  dwAddress;
    SIZE_T     dwBlockSize;
    DWORD      dwFlags;
    DWORD      dwLockCount;
    DWORD      dwResvd;
    DWORD      th32ProcessID;
    ULONG_PTR  th32HeapID;
} HEAPENTRY32, *PHEAPENTRY32, *LPHEAPENTRY32;

#define LF32_FIXED    0x00000001
#define LF32_FREE     0x00000002
#define LF32_MOVEABLE 0x00000004

BOOL WINAPI Heap32First(LPHEAPENTRY32 lphe, DWORD th32ProcessID, ULONG_PTR th32HeapID);
BOOL WINAPI Heap32Next(LPHEAPENTRY32 lphe);

BOOL WINAPI Toolhelp32ReadProcessMemory(
    DWORD    th32ProcessID,
    LPCVOID  lpBaseAddress,
    LPVOID   lpBuffer,
    SIZE_T   cbRead,
    SIZE_T*  lpNumberOfBytesRead
);

// Process walking --------------------------------------------------------
typedef struct tagPROCESSENTRY32 {
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD   th32ModuleID;
    DWORD   cntThreads;
    DWORD   th32ParentProcessID;
    LONG    pcPriClassBase;
    DWORD   dwFlags;
    CHAR    szExeFile[MAX_PATH];
} PROCESSENTRY32, *PPROCESSENTRY32, *LPPROCESSENTRY32;

BOOL WINAPI Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
BOOL WINAPI Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);

// Thread walking ---------------------------------------------------------
typedef struct tagTHREADENTRY32 {
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ThreadID;
    DWORD   th32OwnerProcessID;
    LONG    tpBasePri;
    LONG    tpDeltaPri;
    DWORD   dwFlags;
} THREADENTRY32, *PTHREADENTRY32, *LPTHREADENTRY32;

BOOL WINAPI Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte);
BOOL WINAPI Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

// Module walking ---------------------------------------------------------
typedef struct tagMODULEENTRY32 {
    DWORD    dwSize;
    DWORD    th32ModuleID;
    DWORD    th32ProcessID;
    DWORD    GlblcntUsage;
    DWORD    ProccntUsage;
    BYTE*    modBaseAddr;
    DWORD    modBaseSize;
    HMODULE  hModule;
    char     szModule[MAX_MODULE_NAME32 + 1];
    char     szExePath[MAX_PATH];
} MODULEENTRY32, *PMODULEENTRY32, *LPMODULEENTRY32;

BOOL WINAPI Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
BOOL WINAPI Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

#ifdef __cplusplus
} /* extern "C" */
#endif

namespace hy::toolhelp {
    template <class SnapClass>
    using callback = BOOL(WINAPI*)(HANDLE, SnapClass*);

    template <class SnapClass>
    using predicate = std::function<bool(SnapClass)>;

    template <DWORD SnapFlags, class SnapClass, callback<SnapClass> QueryFirst, callback<SnapClass> QueryNext>
    detail::generator<SnapClass> scan(const DWORD pid, std::optional<predicate<SnapClass>> predicate = std::nullopt) {
        const unique_handle<CloseHandle> handle{CreateToolhelp32Snapshot(SnapFlags, pid)};

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
        return scan<TH32CS_SNAPTHREAD, THREADENTRY32, Thread32First, Thread32Next>(NULL, [pid](const THREADENTRY32 te) -> bool {
            return te.th32OwnerProcessID == pid;
        });
    }
}
