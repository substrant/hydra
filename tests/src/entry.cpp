//
// Created by nick on 10/27/2025.
//

#include "entry.hpp"

#include <iostream>
#include <hydra/sys/process.hpp>

int fail(const int code, const std::string_view message) {
    std::cout << "[-] " << message << '\n';
    return code;
}

int main(int argc, char* argv[]) {
    const auto proc = hy::process::open("Notepad.exe");
    if (!proc) return fail(1, "Process not found");



    return 0;
}c