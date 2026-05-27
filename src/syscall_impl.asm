SECTION .text

global inline_syscall

; NTSTATUS inline_syscall(uint16_t index, ...)
; Shifts args to match Windows x64 syscall convention:
;   rcx = index -> eax (syscall number)
;   rdx -> rcx (arg1)
;   r8  -> rdx (arg2)
;   r9  -> r8  (arg3)
;   [rsp+0x28] -> r9 (arg4)
;   Stack shift handles arg5+
inline_syscall:
    mov r10, rdx
    mov eax, ecx
    mov rcx, rdx
    mov rdx, r8
    mov r8,  r9
    mov r9,  [rsp+0x28]
    add rsp, 0x8
    syscall
    sub rsp, 0x8
    ret
