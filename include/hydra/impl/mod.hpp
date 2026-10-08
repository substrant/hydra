#pragma once

#include <hydra/stm.hpp>
#include <hydra/err.hpp>
#include <hydra/mem.hpp>

#include <optional>
#include <string>
#include <generator>

namespace hy {
    enum class mod_state : std::uint8_t {
        header = 0,
        flat,
        mapped,
        inherit = -1
    };

    struct seg;
    class mod;
}

namespace hy::impl {
    struct seg : blk {
        std::optional<std::string> name;
        mem_mode mode;
    };

    class mod : public blk {
    protected:
        stm* m_stream;
        bool m_owner;
        mod_state m_state;

    public:
        explicit mod(stm& stream, const mod_state state) : m_stream(&stream), m_owner(false), m_state(state) { }

        explicit mod(stm&& stream, const mod_state state) :
            m_stream(stream.clone_move().release()),
            m_owner(true),
            m_state(state) { }

        virtual err parse(mod_state state) = 0;

        virtual std::generator<hy::seg> segments(err* error) = 0;

        virtual std::size_t calc_size(mod_state state) = 0;

        ~mod() {
            if (m_owner && m_stream)
                delete m_stream;
        }
    };
}
