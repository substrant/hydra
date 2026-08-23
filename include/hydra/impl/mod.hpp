#pragma once

#include <hydra/stm.hpp>
#include <hydra/err.hpp>

#include <optional>
#include <string>
#include <generator>

namespace hy {
    enum class mod_state : std::uint8_t {
        flat,
        mapped
    };

    struct seg;
    class mod;
}

namespace hy::impl {
    struct seg : blk {
        std::optional<std::string> name;
    };

    class mod {
    protected:
        stm* m_stream;
        bool m_owner;
        mod_state m_state;

    public:
        explicit mod(stm& stream, const mod_state state) : m_stream(&stream), m_owner(false), m_state(state) { }

        template <typename T> requires std::derived_from<std::remove_cvref_t<T>, stm> && (!std::is_lvalue_reference_v<T>)
        explicit mod(T&& stream, const mod_state state) :
            m_stream(new std::remove_cvref_t<T>(std::forward<T>(stream))),
            m_owner(true),
            m_state(state) { }

        virtual err parse() = 0;

        virtual std::generator<hy::seg> segments(err* error) = 0;

        virtual ~mod() {
            if (m_owner && m_stream)
                delete m_stream;
        }
    };
}
