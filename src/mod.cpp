#include <hydra/mod.hpp>

namespace hy {
    std::expected<seg, err> mod::segment(std::string_view name) {
        auto error = STA_PARTIAL_READ;
        auto generator = segments(&error);

        const auto it = std::ranges::find_if(generator, [&name](const seg& maybe) {
            return name == maybe.name;
        });

        if (it != generator.end())
            return *it;

        return std::unexpected(error);
    }
}
