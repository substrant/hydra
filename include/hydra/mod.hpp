#pragma once

#include <hydra/ost.hpp>
#include <hydra/stm.hpp>

#ifdef HY_OS_NT
#   include <hydra/impl/mod_nt.hpp>
#else
#   error Unsupported platform
#endif

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace hy {
    class mod final : public blk {
        std::unique_ptr<stm> m_owned_stream;
        stm* m_stream;
        mod_state m_state;
        shim::mod m_impl;

        friend class shim::mod;

    public:
        const std::string name{};

        explicit mod(const std::string& name, stm& stream, const mod_state state) :
            m_stream(&stream),
            m_state(state),
            name(name) { }

        explicit mod(const std::string& name, stm&& stream, const mod_state state) :
            m_owned_stream(stream.clone_move()),
            m_stream(m_owned_stream.get()),
            m_state(state),
            name(name) { }

        err parse(const mod_state state = mod_state::inherit) {
            return m_impl.parse(*this, state);
        }

        std::size_t calc_size(const mod_state state) {
            return m_impl.calc_size(*this, state);
        }

        inline std::size_t calc_size() {
            return calc_size(mod_state::inherit);
        }

        std::generator<seg> segments(err* error) {
            return m_impl.segments(*this, error);
        }

        inline std::generator<seg> segments() {
            return segments(nullptr);
        }

        std::expected<seg, err> segment(std::string_view name);
    };

    static_assert(impl::ModImpl<shim::mod>);
}
