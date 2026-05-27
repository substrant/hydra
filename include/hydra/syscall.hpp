#pragma once

#include <phnt_windows.h>
#include <phnt.h>

#include <cstdint>

namespace hy::detail {
    extern "C" NTSTATUS inline_syscall(std::uint16_t index, ...);
}

namespace hy::syscall {
    namespace idx {
        constexpr std::uint16_t NtClose = 0x0F;
        constexpr std::uint16_t NtQueryObject = 0x10;
        constexpr std::uint16_t NtAllocateVirtualMemory = 0x18;
        constexpr std::uint16_t NtQueryInformationProcess = 0x19;
        constexpr std::uint16_t NtQueryInformationThread = 0x25;
        constexpr std::uint16_t NtOpenProcess = 0x26;
        constexpr std::uint16_t NtTerminateProcess = 0x2C;
        constexpr std::uint16_t NtQueryVirtualMemory = 0x23;
        constexpr std::uint16_t NtReadVirtualMemory = 0x3F;
        constexpr std::uint16_t NtWriteVirtualMemory = 0x3A;
        constexpr std::uint16_t NtProtectVirtualMemory = 0x50;
        constexpr std::uint16_t NtResumeThread = 0x52;
        constexpr std::uint16_t NtTerminateThread = 0x53;
        constexpr std::uint16_t NtFreeVirtualMemory = 0x1E;
    }

    NTSTATUS NtClose(HANDLE Handle);

    __forceinline NTSTATUS NtQueryObject(HANDLE Handle, OBJECT_INFORMATION_CLASS ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength) {
        return detail::inline_syscall(idx::NtQueryObject, Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength);
    }

    __forceinline NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect) {
        return detail::inline_syscall(idx::NtAllocateVirtualMemory, ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
    }

    __forceinline NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) {
        return detail::inline_syscall(idx::NtQueryInformationProcess, ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
    }

    __forceinline NTSTATUS NtQueryInformationThread(HANDLE ThreadHandle, THREADINFOCLASS ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength) {
        return detail::inline_syscall(idx::NtQueryInformationThread, ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
    }

    __forceinline NTSTATUS NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId) {
        return detail::inline_syscall(idx::NtOpenProcess, ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
    }

    __forceinline NTSTATUS NtTerminateProcess(HANDLE ProcessHandle, LONG ExitStatus) {
        return detail::inline_syscall(idx::NtTerminateProcess, ProcessHandle, ExitStatus);
    }

    __forceinline NTSTATUS NtQueryVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength) {
        return detail::inline_syscall(idx::NtQueryVirtualMemory, ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, ReturnLength);
    }

    __forceinline NTSTATUS NtReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead) {
        return detail::inline_syscall(idx::NtReadVirtualMemory, ProcessHandle, BaseAddress, Buffer, NumberOfBytesToRead, NumberOfBytesRead);
    }

    __forceinline NTSTATUS NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten) {
        return detail::inline_syscall(idx::NtWriteVirtualMemory, ProcessHandle, BaseAddress, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten);
    }

    __forceinline NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect) {
        return detail::inline_syscall(idx::NtProtectVirtualMemory, ProcessHandle, BaseAddress, RegionSize, NewProtect, OldProtect);
    }

    __forceinline NTSTATUS NtResumeThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount) {
        return detail::inline_syscall(idx::NtResumeThread, ThreadHandle, PreviousSuspendCount);
    }

    __forceinline NTSTATUS NtTerminateThread(HANDLE ThreadHandle, LONG ExitStatus) {
        return detail::inline_syscall(idx::NtTerminateThread, ThreadHandle, ExitStatus);
    }

    __forceinline NTSTATUS NtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType) {
        return detail::inline_syscall(idx::NtFreeVirtualMemory, ProcessHandle, BaseAddress, RegionSize, FreeType);
    }
}
