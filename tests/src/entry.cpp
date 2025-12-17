//
// Created by nick on 10/27/2025.
//

#include "entry.hpp"

#include <iostream>
#include <hydra/process.hpp>

template <typename ...Args>
void print(const char* fmt, Args... args) {
    char buf[100];
    snprintf(buf, sizeof(buf), fmt, args...);
    std::cout << "[+] " << buf << '\n';
}

int fail(const int code, const std::string_view message) {
    std::cout << "[-] " << message << '\n';
    return code;
}

int main(int argc, char* argv[]) {
    const auto proc = hy::process::open("Substrant.HydraTests.exe");
    if (!proc) return fail(1, "Process not found");

    print("Process handle: 0x%x", proc->handle());

    auto main = proc->module();
    print("Main Module: %s | Base: 0x%x | Size: 0x%x", main->file_name().c_str(), main->base(), main->size(hy::pe_size::mapped));

    // List all modules in the process
    for (const auto& mod : proc->linked_modules()) {
        print("Module: %s | Base: 0x%x | Size: 0x%x", mod->file_name().c_str(), mod->base(), mod->size(hy::pe_size::mapped));
    }

    return 0;
}
