#include "entry.hpp"

#include <csignal>

#include "console.hpp"

#include <iostream>
#include <queue>
#include <thread>
#include <fstream>

#include "dumper.hpp"

// Testing feature impl for hydra:
// - Dumping mapped PEs from remote processes
// - Watch-dumping: monitor memory regions and dump as they change

// Giant TODOs before this is fully possible:
// - hydra/module still needs a lot of work: rebuidling, overwrite support, etc.
// - hydra/detail needs more scaffolding information
// - source needs to be merged from testing into the library itself

// What you see here is purely experimental and tentative and may or may not
// reflect true features currently present in hydra.

namespace app {
    std::shared_ptr<hy::process> target;
    std::shared_ptr<dump_ctx> context;
}

void dump_section(const hy::region dest, const hy::pe_section* section) {
    const auto src = section->buffer(hy::pe_source::file);

    // Clearly mark what executable code wasn't dumped with int3 instructions
    if (section->executable()) dest.fill(0xCC);

    console::print("Dump %s %p - %p (r) -> %p - %p (l) sz=%p",
        section->name().c_str(),
        src.base(),
        src.end(),
        dest.base(),
        dest.end(),
        dest.size()
    );

    const dump_params params(app::target, src, dest);
    const auto& context = app::context = std::make_shared<dump_ctx>(params);

    context->begin_dump();

    //std::thread([&context]() -> void { Sleep(3000); context->cancel_dump(); }).detach();
    context->wait_dump();

    //const auto read = app::target->mm_read(src.base(), dest);
    //console::print("Read bytes: %zu / %zu", read, src.size());
}

void dump_module(const hy::remote_module& module) {
    const auto module_name = module.file_name();

    const auto size_disk = module.size(hy::pe_source::file);
    const auto size_memory = module.size(hy::pe_source::mapped);

    console::print("Module %s", module_name.c_str());
    console::print("|-- Base:     0x%p", module.base());
    console::print("|-- Raw Size: 0x%p bytes", size_disk);
    console::print("|-- VAS Size: 0x%p bytes", size_memory);
    console::print("|-- Sections");

    size_t collective_sections_size_file = 0;

    for (const auto& section : module.sections()) {
        const auto bounds = section->buffer();
        console::print("|   |-- Section %s", section->name().c_str());
        console::print("|   |   |-- Base:       0x%p", bounds.base());
        console::print("|   |   |-- Raw Offset: 0x%p", section->offset(hy::pe_source::file));
        console::print("|   |   |-- Raw Size:   0x%p bytes", section->size(hy::pe_source::file));
        console::print("|   |   |-- Raw End:    0x%p", section->offset(hy::pe_source::file) + section->size(hy::pe_source::file));
        console::print("|   |   |-- VAS Offset: 0x%p", section->offset(hy::pe_source::mapped));
        console::print("|   |   |-- VAS Size:   0x%p bytes", section->size(hy::pe_source::mapped));

        collective_sections_size_file += section->size(hy::pe_source::file);
    }

    console::print("|-- Required size:    0x%p bytes (from sections)", collective_sections_size_file);
    console::print("|-- Dump buffer size: 0x%p bytes", size_disk);

    const auto dump = hy::region::alloc_local(size_disk, true);
    console::print("Local allocation: %p - %p (%zu bytes)", dump.base(), dump.end(), dump.size());

    for (const auto& section : module.sections()) {
        dump_section(hy::region(
            dump.base() + section->offset(hy::pe_source::file),
            section->size(hy::pe_source::file)
        ), section);
    }

    const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(dump.base().p);
    const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(dump.base().p + dos_header->e_lfanew);
    const auto opt_header = &nt_headers->OptionalHeader;

    opt_header->FileAlignment = 0x200;

    // realign pe header


    // dump to file
    std::ofstream out("dump.bin", std::ios::binary);
    out.write(static_cast<const char*>(dump.base().u), (int)dump.size());

    console::print("Dumped to disk at dump.bin");
}

static BOOL WINAPI signal_handler(const DWORD signal) {
    if (signal == CTRL_C_EVENT && app::context) {
        app::context->cancel_dump();
        return TRUE;
    }

    return FALSE;
}

int main(int argc, char* argv[]) {
    // Set up signal handler
    SetConsoleCtrlHandler(signal_handler, TRUE);

    app::target = hy::process::open("notepad.exe");
    if (!app::target) return console::fail(1, "Process not found");

    auto& main = app::target->module();
    dump_module(main);

    return 0;





    /*auto section = main->section(".text");
    if (!section.has_value())
        return console::fail(2, "Failed to find .text section");

    auto source = section->buffer();

    

    console::print("dump: %p - %p (%zu bytes)", dump.base(), dump.end(), dump.size());

    dump_params params(proc, source, dump);
    dump_ctx ctx(params);

    ctx.build_queue();
    console::print(".text: %p-%p (%zu bytes, %d pages)", source.base(), source.end(), source.size(), ctx.page_count);

    ctx.dump();

    // time out threads after 10 seconds
    console::print("Timed out.");

    ctx.cancel_dump();

    // dump to file
    std::ofstream out("dump.bin", std::ios::binary);
    out.write(static_cast<const char*>(dump.base().u), dump.size());

    console::print("Dumped to disk at dump.bin");*/

    return 0;
}
