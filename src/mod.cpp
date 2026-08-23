#include <hydra/mod.hpp>

namespace hy {
    std::expected<seg, err> mod::segment(std::string_view name) {
        auto error = STA_PARTIAL_READ;
        auto generator = segments(&error);

        const auto it = std::ranges::find_if(generator, [&name](const seg& maybe) {
            const auto maybe_ = reinterpret_cast<const seg&>(maybe);
            return name == maybe_.name;
        });

        if (it != generator.end())
            return *it;

        return std::unexpected(error);
    }
}
