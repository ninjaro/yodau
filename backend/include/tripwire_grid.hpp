#ifndef YODAU_BACKEND_TRIPWIRE_GRID_HPP
#define YODAU_BACKEND_TRIPWIRE_GRID_HPP

#include "coords.hpp"
#include "geometry.hpp"

#include <cstddef>
#include <vector>

namespace yodau::backend {

struct grid_tripwire_segment {
    std::size_t seg_index {};
    point a_pct;
    point b_pct;
    std::vector<grid_point> cells;
};

std::vector<grid_tripwire_segment>
compile_line_to_grid_segments(const line& l, const grid_dims& g);

}

#endif // YODAU_BACKEND_TRIPWIRE_GRID_HPP
