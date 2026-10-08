# hydra

A C++23 Windows process manipulation library for reverse engineering, debugging, and
instrumentation. Built on NTAPI throughout, with additional abstractions for specific
Windows APIs.

Internal library actively used in [Carbon Platform](https://carbonlabs.dev/) and several
internal reverse engineering and instrumentation utilities at Substrant.

## What It Does

Hydra gives you direct, modern, and simplified control over Windows processes. It's built
for tooling that needs to sit underneath conventional abstractions: debuggers,
instrumentation frameworks, anti-anti-debug helpers, and whatever else needs raw access to
a target process.

Notice: Hydra is not developed as a stealth toolkit, but rather an external process
manipulation tool. The leveraging of advanced C++ features results in Hydra using
high-level Windows abstractions regardless of how low level it is. *This will change in
the future as Hydra matures*.

Hydra is actively used in external host processes that need to externally analyze Windows
processes as they're running. Some of which involve process dumping utilities, executable
file patchers, and external game mods (Carbon Platform).

## Quick Look

```cpp
#include <hydra/process.hpp>
#include <hydra/memory.hpp>

// Open a process to receive a "process" object
std::expected<hy::process, NTSTATUS> proc = hy::process::open("notepad.exe");
if (!proc.has_value()) return proc.error(); // Returns NTSTATUS if failed

// Allocate local virtual memory and read from the process
hy::region buf = hy::region::alloc_local(0x1000);
proc->mm_read(0x123456ABCDEF, buf, buf.size());

// Scan for a byte pattern in a buffer
for (auto found : buf.scan_aob("\x48\x8B\x05\x00\x00\x00\x00", "xxx????")) {
    // Variable "found" is a hy::addr'
}

// Find the main module and sections in a process
auto* my_module = proc->module();
auto code_section = my_module->section(".text");

// Walk the PEB to find modules
for (auto& mod : proc->linked_modules()) {
    // Variable "mod" is a remote PE object
}

// Disassemble code in remote processes
hy::code_disasm disasm{ code_section->base() };
auto found = disasm.find(
    code_section->size(),
    code_query::opcode(0xE8),
    code_query::reg(0, ZYDIS_REG_RAX))
);
```

## Native Windows Process Discovery

Windows builds expose lazy PID searches by executable-module name and exact window title:

```cpp
for (const auto pid : hy::proc::find_by_mod("notepad.exe")) {
    // Open or inspect every matching Notepad process.
}

for (const auto pid : hy::proc::find_by_window("Untitled - Notepad")) {
    // Process IDs are yielded for matching top-level windows.
}
```

Both paths stay native: process names come from `NtQuerySystemInformation`, while window-title matching and owner PID
lookup call the PHNT declarations for `NtUserFindWindowEx` and `NtUserQueryWindow` directly.

## Planned Features

Overall implementation:

- [ ] Stealthier usage mechanics
- [ ] Internal memory/instrumentation
- [ ] Syscall index probing
- [ ] Higher API granularity
- [ ] Heuristic disassembly utilities

Additional features:

- [ ] Self-sufficient runtime injection
- [ ] Manual DLL mapping support
- [ ] Ring buffer communication

## Core Types

| Type              | Header            | Purpose                                                                                 |
|-------------------|-------------------|-----------------------------------------------------------------------------------------|
| `process`         | `process.hpp`     | Open/manipulate a target process. Memory I/O, module/thread enum, suspend/resume/kill.  |
| `thread`          | `thread.hpp`      | Thread control: suspend, resume, get/set context, query entry point.                    |
| `addr`            | `memory.hpp`      | Unified pointer type. Implicitly converts to/from `void*`, `uintptr_t`, `uint8_t*`.     |
| `region`          | `memory.hpp`      | Memory buffer. Owns (RAII) or wraps external memory. AOB scanning built in.             |
| `handle<CloseFn>` | `handle.hpp`      | RAII handle. `nt_handle = handle<syscall::NtClose>`. Closes on destruction.             |
| `pe_image`        | `module.hpp`      | PE parser. Read from disk, memory, or stream. Sections, imports, exports, RVA resolve.  |
| `remote_module`   | `module.hpp`      | A `pe_image` backed by remote process memory. Walk sections, resolve symbols.           |
| `code_disasm`     | `code_disasm.hpp` | Zydis disassembler over a `region`. Step, skip, search by predicates.                   |
| `window`          | `window.hpp`      | Thin HWND wrapper. Find, show, hide.                                                    |
| `comm_peer`       | `comm.hpp`        | Shared-memory IPC ring buffer. Send regions between processes. Unfinished impl.         |

## Core Philosophy

Hydra isn't intended to replace the Windows API, but rather simplify and abstract some clunky mechanisms of the Windows
API to reduce boilerplate, deduplicate common code patterns, and improve the overall developer experience.

**Compile-time platform contracts.** `impl::ProcImpl` and `impl::ModImpl` define the operations every platform must
provide. The selected `shim` types implement those contracts, and the public `proc` and `mod` types compose the selected
implementation while owning common behavior. `static_assert` checks each selected implementation during compilation;
calls use direct dispatch without platform-interface objects or virtual calls.

**Expected, not exceptions.** `process::open` returns `std::expected<process, NTSTATUS>`. Generators silently
`co_return` on failure. No try/catch clutter.

**Generators everywhere.** Thread lists, module walks, memory region enum, AOB scan hits all return lazy
`std::generator` coroutines. Pull what you need, stop when you're done.

**`addr` everywhere else.** One pointer type that implicitly converts to everything. No casting between `void*`,
`uintptr_t`, and `uint8_t*`. Arithmetic just works, and explicit casting work is significantly reduced.

**Implicit by default.** Handle wrapping classes cast to `HANDLE` for simpler integration to the Windows API. `addr`
will attempt to implicitly cast to valid pointer/integer types when possible.

**`region` owns or borrows.** `alloc_local` gives you owned RAII memory. Wrapping constructors give you a non-owning
view. Move transfers ownership, copy gives a view. Scan, fill, rebase, resize. It's all there.

**`pe_source` disambiguates views.** A PE can be read from disk (`file`), from mapped memory (`mapped`), from just the
header (`header`), or inherited from the stream. Every section/buffer method takes a `pe_source` so you always know
what you're reading.

## Building

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Requires CMake 4.0+, MSVC 2022, and phnt/Zydis submodules (`git submodule update --init`). Outputs `libhydra.lib`, which
statically links to your project of choice.

## Status

Experimental and used as an internal library at Substrant. APIs will change. Incomplete features live in-tree with
`// TODO` markers.

## License

This project is licensed under the Apache License, Version 2.0. By using this source code or its derivative works, you
agree to the terms and conditions outlined in the [LICENSE](./LICENSE) file.
