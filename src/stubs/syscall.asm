SECTION .text
    global inline_syscall
    global inline_probe_syscall
    global inline_probe_ret_addr

    inline_syscall:                     ; inline_syscall(index, ...args) -> NTSTATUS
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

    inline_probe_syscall:
        mov eax, 0x31
        lea r10, [rsp-0x08]

        ; Use dummy value as first arg
        sub rsp, 8
        mov r10, rsp

        ; Fill 4KB of the stack
        sub rsp, 0x1000
        mov rcx, rsp
        mov rdx, 0x1008 / 0x08
        mov rax, 0xCCCCCCCCCCCCCCCC

    fill_loop:
        mov [rcx], rax
        add rcx, 8
        dec rdx
        jnz fill_loop

        ; Second argument is NULL
        xor rcx, rcx
        syscall

    inline_probe_ret_addr:
        nop

        ; Clean up the stack
        add rsp, 0x1008

        ; Store stack information
        mov rax, rsp
        mov [rcx+0x08], rax             ; struct stack_info
        mov rax, gs:[0x08]              ; RAX = StackBase
        mov [rcx+0x00], rax

        ret