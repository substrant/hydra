#include "hydra/procstm.hpp"

namespace hy {
    std::unique_ptr<stm> procstm::clone_move() {
        return std::make_unique<procstm>(std::move(*this));
    }
}
