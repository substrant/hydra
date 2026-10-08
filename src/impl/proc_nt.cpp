#include <hydra/ost.hpp>

#ifdef HY_OS_NT

#include <hydra/impl/proc_nt.hpp>
#include <hydra/mod.hpp>
#include <hydra/nt/err.hpp>

#include "hydra/procstm.hpp"
#include "hydra/nt/str.hpp"

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
        if (m_hnd && !m_pid.nt) {
            PROCESS_BASIC_INFORMATION pbi;
            ULONG ret_len;

            if (!NT_SUCCESS(nt::err = NtQueryInformationProcess(
                m_hnd,
                ProcessBasicInformation,
                &pbi,
                sizeof(pbi),
                &ret_len
            ))) return ERR_NATIVE_ERROR;

            m_pid.nt = pbi.UniqueProcessId;
        }

        // Missing process handle
        else if (m_pid.nt && !m_hnd) {
            CLIENT_ID cid;
            cid.UniqueProcess = m_pid.nt;
            cid.UniqueThread = nullptr;

            OBJECT_ATTRIBUTES attr;
            InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);

            if (!NT_SUCCESS(nt::err = NtOpenProcess(
                &m_hnd.value,
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

    err proc::open_pid(const pid_t pid) {
        m_pid = pid;
        return claim();
    }

    err proc::open_hnd(const HANDLE handle) {
        m_hnd.reset(handle, true);
        return claim();
    }

    std::generator<hy::mod&> proc::mod_enum(hy::proc& owner) {
        PROCESS_BASIC_INFORMATION info;
        ptr ldr_addr;

        union {
            LIST_ENTRY l;
            LDR_DATA_TABLE_ENTRY t;
        } ldr_entry;
        
        if (!NT_SUCCESS(nt::err = NtQueryInformationProcess(
            m_hnd,
            ProcessBasicInformation,
            &info,
            sizeof(info),
            nullptr
        ))) co_return;

        if (mm_read(
            ptr(&ldr_addr),
            static_cast<ptr>(info.PebBaseAddress) + offsetof(PEB, Ldr),
            sizeof(ldr_addr)
        ) != sizeof(ldr_addr)) {
            nt::err = STA_PARTIAL_READ;
            co_return;
        }

        if (mm_read(
            ptr(&ldr_addr),
            ldr_addr + offsetof(PEB_LDR_DATA, InLoadOrderModuleList),
            sizeof(ldr_addr)
        ) != sizeof(ldr_addr)) {
            nt::err = STA_PARTIAL_READ;
            co_return;
        }

        ptr head = ldr_addr;
        for (;;) {
            if (mm_read(ptr(&ldr_entry), ldr_addr, sizeof(ldr_entry)) != sizeof(ldr_entry)) {
                nt::err = STA_PARTIAL_READ;
                co_return;
            }

            if ((ldr_addr = ldr_entry.l.Flink) == head)
                break;

            std::wstring name_ws(ldr_entry.t.BaseDllName.Length / sizeof(wchar_t), L'0');
            mm_read(name_ws.data(), ldr_entry.t.BaseDllName.Buffer, ldr_entry.t.BaseDllName.Length);

            procstm stm(owner, blk{ ldr_entry.t.DllBase, ldr_entry.t.SizeOfImage });
            hy::mod mod(nt::unicode_to_string(name_ws), std::move(stm), mod_state::mapped);

            mod.parse();
            co_yield mod;
        }

        co_return;
    }

    std::size_t proc::mm_read(const ptr local_dst, const ptr remote_src, const std::size_t size) {
        SIZE_T bytes_read = 0;
            
        nt::err = NtReadVirtualMemory(
            m_hnd,
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
            m_hnd,
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
            m_hnd,
            remote_base,
            &size,
            protection,
            &protection
        ));
    }

    blk proc::mm_alloc(const ptr remote_base, std::size_t size, const mem_mode mode) {
        const auto protection = get_protection(mode);
        
        if (!NT_SUCCESS(nt::err = NtAllocateVirtualMemory(
            m_hnd,
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
            m_hnd,
            remote_base,
            &size,
            MEM_RELEASE
        ));
    }
}

#endif
