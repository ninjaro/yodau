#ifndef YODAU_CORE_TRIPWIRE_GRID_HPP
#define YODAU_CORE_TRIPWIRE_GRID_HPP

#include "geometry/coords.hpp"
#include "geometry/geometry.hpp"

#include <cstddef>
#include <vector>

namespace yodau::core {

struct grid_tripwire_segment {
    size_t seg_index {};
    point a_pct;
    point b_pct;
    std::vector<grid_point> cells;
};

std::vector<grid_tripwire_segment>
compile_line_to_grid_segments(const line& l, const grid_dims& g);

}

#endif // YODAU_CORE_TRIPWIRE_GRID_HPP
