#ifndef YODAU_CORE_TRIPWIRE_GRID_INDEX_HPP
#define YODAU_CORE_TRIPWIRE_GRID_INDEX_HPP

#include "analysis/tripwire_grid.hpp"
#include "core/namespace_alias.hpp"

#include <cstddef>
#include <vector>

namespace yodau::core {

struct grid_line_index {
    grid_dims dims;
    std::vector<grid_tripwire_segment> segments;
    std::vector<std::vector<size_t>> cell_to_segments;
};

grid_line_index build_grid_line_index(const line& l, const grid_dims& g);
}

#endif // YODAU_CORE_TRIPWIRE_GRID_INDEX_HPP
