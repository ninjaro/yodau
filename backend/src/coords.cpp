#include "coords.hpp"

int yodau::backend::clamp_int(const int v, const int lo, const int hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

float yodau::backend::clamp_float(
    const float v, const float lo, const float hi
) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

int yodau::backend::pct_to_px(const float pct, const int size) {
    if (size <= 0) {
        return 0;
    }

    const float p = clamp_float(pct, 0.0f, 100.0f);
    int px = static_cast<int>(p * static_cast<float>(size) / 100.0f);

    if (px >= size) {
        px = size - 1;
    }
    if (px < 0) {
        px = 0;
    }

    return px;
}

float yodau::backend::px_to_pct(const int px, const int size) {
    if (size <= 0) {
        return 0.0f;
    }

    const int pxi = clamp_int(px, 0, size - 1);
    return static_cast<float>(pxi) * 100.0f / static_cast<float>(size);
}

yodau::backend::px_point yodau::backend::pct_point_to_px(
    const point& p, const int width, const int height
) {
    px_point out {};
    out.x = pct_to_px(p.x, width);
    out.y = pct_to_px(p.y, height);
    return out;
}

yodau::backend::point yodau::backend::px_point_to_pct(
    const px_point& p, const int width, const int height
) {
    point out {};
    out.x = px_to_pct(p.x, width);
    out.y = px_to_pct(p.y, height);
    return out;
}

yodau::backend::grid_point
yodau::backend::pct_point_to_grid(const point& p, const grid_dims& g) {
    grid_point out {};
    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    const float x = clamp_float(p.x, 0.0f, 100.0f);
    const float y = clamp_float(p.y, 0.0f, 100.0f);

    int cx = static_cast<int>(x * static_cast<float>(g.nx) / 100.0f);
    int cy = static_cast<int>(y * static_cast<float>(g.ny) / 100.0f);

    if (cx >= g.nx) {
        cx = g.nx - 1;
    }
    if (cy >= g.ny) {
        cy = g.ny - 1;
    }
    if (cx < 0) {
        cx = 0;
    }
    if (cy < 0) {
        cy = 0;
    }

    out.x = cx;
    out.y = cy;
    return out;
}

yodau::backend::grid_point yodau::backend::px_point_to_grid(
    const px_point& p, const int width, const int height, const grid_dims& g
) {
    const point pct = px_point_to_pct(p, width, height);
    return pct_point_to_grid(pct, g);
}

yodau::backend::cell_bounds_pct
yodau::backend::grid_cell_bounds_pct(const grid_point& c, const grid_dims& g) {
    cell_bounds_pct b {};
    if (g.nx <= 0 || g.ny <= 0) {
        return b;
    }

    const int cx = clamp_int(c.x, 0, g.nx - 1);
    const int cy = clamp_int(c.y, 0, g.ny - 1);

    b.x0 = static_cast<float>(cx) * 100.0f / static_cast<float>(g.nx);
    b.x1 = static_cast<float>(cx + 1) * 100.0f / static_cast<float>(g.nx);
    b.y0 = static_cast<float>(cy) * 100.0f / static_cast<float>(g.ny);
    b.y1 = static_cast<float>(cy + 1) * 100.0f / static_cast<float>(g.ny);

    return b;
}

yodau::backend::point
yodau::backend::grid_cell_center_pct(const grid_point& c, const grid_dims& g) {
    const auto b = grid_cell_bounds_pct(c, g);

    point out {};
    out.x = (b.x0 + b.x1) * 0.5f;
    out.y = (b.y0 + b.y1) * 0.5f;
    return out;
}

int yodau::backend::grid_index(const grid_point& c, const grid_dims& g) {
    if (g.nx <= 0 || g.ny <= 0) {
        return 0;
    }

    const int cx = clamp_int(c.x, 0, g.nx - 1);
    const int cy = clamp_int(c.y, 0, g.ny - 1);
    return cy * g.nx + cx;
}

yodau::backend::grid_point
yodau::backend::clamp_grid_point(const grid_point& c, const grid_dims& g) {
    grid_point out {};

    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    out.x = clamp_int(c.x, 0, g.nx - 1);
    out.y = clamp_int(c.y, 0, g.ny - 1);
    return out;
}

std::vector<yodau::backend::grid_point> yodau::backend::trace_grid_cells(
    const grid_point& a, const grid_point& b, const grid_dims& g
) {
    std::vector<grid_point> out;

    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    grid_point p0 = clamp_grid_point(a, g);
    grid_point p1 = clamp_grid_point(b, g);

    int x0 = p0.x;
    int y0 = p0.y;
    const int x1 = p1.x;
    const int y1 = p1.y;

    const int dx = std::abs(x1 - x0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = (y0 < y1) ? 1 : -1;

    int err = dx + dy;

    while (true) {
        out.push_back(grid_point { x0, y0 });

        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    return out;
}

std::vector<yodau::backend::grid_point> yodau::backend::trace_grid_cells_pct(
    const point& a_pct, const point& b_pct, const grid_dims& g
) {
    const grid_point a = pct_point_to_grid(a_pct, g);
    const grid_point b = pct_point_to_grid(b_pct, g);
    return trace_grid_cells(a, b, g);
}