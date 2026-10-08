#include <cstdint>
#include <iostream>

#include <hydra/mod.hpp>
#include <hydra/nt/err.hpp>
#include <hydra/proc.hpp>

int main() {
    hy::proc target;
    if (hy::proc::open_hnd(GetCurrentProcess(), &target) != hy::STA_SUCCESS) {
        std::cerr << "failed to open current process: " << std::hex << hy::nt::err << '\n';
        return 1;
    }

    const std::uint64_t expected = 0x123456789abcdef0;
    std::uint64_t actual = 0;
    if (target.mm_read(&actual, hy::ptr(&expected)) != sizeof(actual) || actual != expected) {
        std::cerr << "failed to read current process memory\n";
        return 1;
    }

    std::size_t module_count = 0;
    for (const auto& module : target.mod_enum()) {
        if (!module.base || !module.size || module.name.empty()) {
            std::cerr << "failed to parse a current process module\n";
            return 1;
        }

        ++module_count;
    }

    if (!module_count) {
        std::cerr << "failed to enumerate current process modules\n";
        return 1;
    }

    std::cout << "read current process memory and parsed " << module_count << " modules\n";
    return 0;
}
