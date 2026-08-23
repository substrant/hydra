#pragma once

#include <hydra/ptr.hpp>
#include <hydra/err.hpp>
#include <hydra/mem.hpp>

namespace hy {
    class proc;
}

namespace hy::impl {
    class proc {
    protected:
        virtual err claim() = 0;

    public:
        virtual std::size_t mm_read(ptr local_dst, ptr remote_src, std::size_t size) = 0;

        virtual std::size_t mm_write(ptr remote_dst, ptr local_src, std::size_t size) = 0;

        virtual bool mm_protect(ptr remote_base, mem_mode mode, std::size_t size = 0) = 0; // zero = mode region/page at base

        virtual ~proc() = 0;
    };
}
