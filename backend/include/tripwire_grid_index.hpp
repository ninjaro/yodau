#ifndef YODAU_BACKEND_TRIPWIRE_GRID_INDEX_HPP
#define YODAU_BACKEND_TRIPWIRE_GRID_INDEX_HPP

#include "tripwire_grid.hpp"

#include <cstddef>
#include <vector>

namespace yodau::backend {

struct grid_line_index {
    grid_dims dims;
    std::vector<grid_tripwire_segment> segments;
    std::vector<std::vector<std::size_t>> cell_to_segments;
};

grid_line_index build_grid_line_index(const line& l, const grid_dims& g);
}

#endif // YODAU_BACKEND_TRIPWIRE_GRID_INDEX_HPP