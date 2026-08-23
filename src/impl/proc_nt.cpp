#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/impl/proc_nt.hpp>
#include <hydra/nt/err.hpp>

namespace hy::shim {
    err proc::claim() {
        // Missing process ID
        if (hnd && !pid.nt) {
            PROCESS_BASIC_INFORMATION pbi;
            ULONG ret_len;

            if (!NT_SUCCESS(nt::err = NtQueryInformationProcess(
                hnd,
                ProcessBasicInformation,
                &pbi,
                sizeof(pbi),
                &ret_len
            ))) return ERR_NATIVE_ERROR;

            pid.nt = pbi.UniqueProcessId;
        }

        // Missing process handle
        else if (pid.nt && !hnd) {
            CLIENT_ID cid;
            cid.UniqueProcess = pid.nt;
            cid.UniqueThread = nullptr;

            OBJECT_ATTRIBUTES attr;
            InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);

            if (!NT_SUCCESS(nt::err = NtOpenProcess(
                &hnd.value,
                PROCESS_ALL_ACCESS,
                &attr,
                &cid
            ))) return ERR_NATIVE_ERROR;
        }

        else {
            return (err)-111;
        }

        return STA_SUCCESS;
    }

    err proc::open_hnd(const HANDLE handle, hy::proc* proc) {
        const auto proc_ = reinterpret_cast<shim::proc*>(proc);
        proc_->hnd.reset(handle, true);
        return proc_->claim();
    }

    std::size_t proc::mm_read(const ptr local_dst, const ptr remote_src, const std::size_t size) {
        SIZE_T bytes_read = 0;
            
        nt::err = NtReadVirtualMemory(
            hnd,
            remote_src,
            local_dst,
            size,
            &bytes_read
        );
            
        return bytes_read;
    }

    std::size_t proc::mm_write(const ptr remote_dst, const ptr local_src, const std::size_t size) {
        SIZE_T bytes_read = 0;

        nt::err = NtWriteVirtualMemory(
            hnd,
            remote_dst,
            local_src,
            size,
            &bytes_read
        );

        return bytes_read;
    }

    bool proc::mm_protect(const ptr remote_base, const mem_mode mode, std::size_t size) {
        static constexpr DWORD protection_map[] = {
            PAGE_NOACCESS,             // ---
            PAGE_READONLY,             // r--
            PAGE_READWRITE,            // -w-
            PAGE_READWRITE,            // rw-
            PAGE_EXECUTE,              // --x
            PAGE_EXECUTE_READ,         // r-x
            PAGE_EXECUTE_READWRITE,    // -wx
            PAGE_EXECUTE_READWRITE     // rwx
        };

        const auto rwx = mode & (mem_mode::read | mem_mode::write | mem_mode::exec);
        auto protection = protection_map[static_cast<std::uint8_t>(rwx)];

        if ((mode & mem_mode::guard) != 0)
            protection |= PAGE_GUARD;

        return NT_SUCCESS(nt::err = NtProtectVirtualMemory(
            hnd,
            remote_base,
            &size,
            protection,
            &protection
        ));
    }
}

#endif
