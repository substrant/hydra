#include "hydra/blkstm.hpp"

namespace hy {
    std::unique_ptr<stm> blkstm::clone_move() {
        return std::make_unique<blkstm>(std::move(*this));
    }
}
