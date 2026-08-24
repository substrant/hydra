#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/impl/proc_nt.hpp>
#include <hydra/nt/err.hpp>

namespace {
    constexpr DWORD protection_map[] = {
        PAGE_NOACCESS,             // ---
        PAGE_READONLY,             // r--
        PAGE_READWRITE,            // -w-
        PAGE_READWRITE,            // rw-
        PAGE_EXECUTE,              // --x
        PAGE_EXECUTE_READ,         // r-x
        PAGE_EXECUTE_READWRITE,    // -wx
        PAGE_EXECUTE_READWRITE     // rwx
    };

    constexpr ULONG get_protection(const hy::mem_mode mode) {
        const auto rwx = mode & (hy::mem_mode::read | hy::mem_mode::write | hy::mem_mode::exec);
        ULONG protection = protection_map[static_cast<std::uint8_t>(rwx)];

        if ((mode & hy::mem_mode::guard) != 0)
            protection |= PAGE_GUARD;

        return protection;
    }
}

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

    bool proc::mm_protect(const ptr remote_base, std::size_t size, const mem_mode mode) {
        auto protection = get_protection(mode);
        return NT_SUCCESS(nt::err = NtProtectVirtualMemory(
            hnd,
            remote_base,
            &size,
            protection,
            &protection
        ));
    }

    blk proc::mm_alloc(const ptr remote_base, std::size_t size, const mem_mode mode) {
        const auto protection = get_protection(mode);
        
        if (!NT_SUCCESS(nt::err = NtAllocateVirtualMemory(
            hnd,
            remote_base,
            0,
            &size,
            MEM_COMMIT | MEM_RESERVE,
            protection
        ))) return blk(0);

        return remote_base;
    }

    bool proc::mm_free(const ptr remote_base) {
        std::size_t size = 0;
        return NT_SUCCESS(nt::err = NtFreeVirtualMemory(
            hnd,
            remote_base,
            &size,
            MEM_RELEASE
        ));
    }
}

#endif
