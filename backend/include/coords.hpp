#ifndef YODAU_BACKEND_COORDS_HPP
#define YODAU_BACKEND_COORDS_HPP

#include "geometry.hpp"

namespace yodau::backend {

struct px_point {
    int x {};
    int y {};
};

struct grid_point {
    int x {};
    int y {};
};

struct grid_dims {
    int nx {};
    int ny {};
};

struct cell_bounds_pct {
    float x0 {};
    float y0 {};
    float x1 {};
    float y1 {};
};

int clamp_int(const int v, const int lo, const int hi);

float clamp_float(const float v, const float lo, const float hi);

int pct_to_px(const float pct, const int size);

float px_to_pct(const int px, const int size);

px_point pct_point_to_px(const point& p, const int width, const int height);

point px_point_to_pct(const px_point& p, const int width, const int height);

grid_point pct_point_to_grid(const point& p, const grid_dims& g);

grid_point px_point_to_grid(
    const px_point& p, const int width, const int height, const grid_dims& g
);

cell_bounds_pct grid_cell_bounds_pct(const grid_point& c, const grid_dims& g);

point grid_cell_center_pct(const grid_point& c, const grid_dims& g);

int grid_index(const grid_point& c, const grid_dims& g);

grid_point clamp_grid_point(const grid_point& c, const grid_dims& g);

std::vector<grid_point>
trace_grid_cells(const grid_point& a, const grid_point& b, const grid_dims& g);

std::vector<grid_point> trace_grid_cells_pct(
    const point& a_pct, const point& b_pct, const grid_dims& g
);

}

#endif // YODAU_BACKEND_COORDS_HPP
