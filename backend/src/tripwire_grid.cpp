#include "tripwire_grid.hpp"

std::vector<yodau::backend::grid_tripwire_segment>
yodau::backend::compile_line_to_grid_segments(
    const line& l, const grid_dims& g
) {
    std::vector<grid_tripwire_segment> out;

    const size_t n = l.points.size();
    if (n < 2) {
        return out;
    }

    size_t seg_index = 0;

    for (size_t i = 1; i < n; ++i) {
        grid_tripwire_segment seg {};
        seg.seg_index = seg_index;
        seg.a_pct = l.points[i - 1];
        seg.b_pct = l.points[i];
        seg.cells = trace_grid_cells_pct(seg.a_pct, seg.b_pct, g);

        out.push_back(std::move(seg));
        seg_index++;
    }

    if (l.closed && n > 2) {
        grid_tripwire_segment seg {};
        seg.seg_index = seg_index;
        seg.a_pct = l.points.back();
        seg.b_pct = l.points.front();
        seg.cells = trace_grid_cells_pct(seg.a_pct, seg.b_pct, g);

        out.push_back(std::move(seg));
        seg_index++;
    }

    return out;
}