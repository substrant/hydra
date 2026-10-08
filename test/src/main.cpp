#include <iostream>

#include <hydra/proc.hpp>
#include <hydra/blkstm.hpp>
#include <hydra/ptr.hpp>
#include <hydra/nt/err.hpp>

#include <hydra/mod.hpp>

#include <Psapi.h>

#include "hydra/procstm.hpp"
#include "hydra/disasm.hpp"

//#include "hydra/mod.hpp"

/// Stream operation for ptr. Used for easy reading.
inline std::ostream& operator<<(std::ostream& os, const hy::ptr& p) {
	return os << p.u;
}

int main() {
	hy::proc target;

	hy::pid_t pid;
	GetWindowThreadProcessId(FindWindowA(nullptr, "Untitled - Notepad"), pid);
	
	auto handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    if (hy::proc::open_hnd(handle, &target) != hy::STA_SUCCESS) {
		std::cout << "failed to open: " << std::hex << hy::nt::err << "\n";
		return 0;
    }

	std::cout << "works: " << std::hex << hy::nt::err << "\n";

	for (auto& mod : target.mod_enum()) {
		std::cout << mod.name << ": " << mod.base << " -> " << mod.size << "\n";
	}

	std::cout << "works: " << std::hex << hy::nt::err << "\n";

	return 0;

	MODULEINFO mod_info; // yes i know its in this proc but base for ntdll is static
	const auto mod_address = GetModuleHandleA("ntdll.dll");
	
    GetModuleInformation(target.hnd, mod_address, &mod_info, sizeof(mod_info));

	hy::blk mod_block{ mod_address, mod_info.SizeOfImage };
	hy::mod module("cock", hy::procstm(target, mod_block), hy::mod_state::mapped);

	std::cout << "[+] Parse status: " << module.parse() << "\n";

	for (const auto& seg : module.segments()) {
		std::cout << "[+] Segment '" << seg.name.value_or("<unknown>") << "'\n";
		std::cout << "[+] Base: 0x" << std::hex << std::uppercase << seg.base << "\n";
		std::cout << "[+] Size: 0x" << std::hex << std::uppercase << seg.size << "\n\n";
	}

	auto text = module.segment(".text").value();
	std::cout << "[+] .text segment: " << text.base << "\n";
	
	hy::disasm analysis(hy::procstm(target, hy::blk(text.base, text.size)));

	hy::err last_err;
	int tries = 0;

	hy::disasm_func fn;
	do {
		last_err = analysis.step_func(&fn);

		if (last_err == hy::STA_END_OF_STREAM)
			break;
		
		if (last_err == hy::STA_DISASM_DECODE_FAIL_INSTRUCTION)
			analysis.step_align(0x10);

		if (fn.size > 0x200)
			continue;

		std::cout << "[+] Function:\n-------------------\n" << analysis.format_func(&fn) << "-------------------\n";
		//Sleep(1000);
	} while (true);

	std::cout << "[?] Last error: " << std::dec << last_err << "\n";
}
