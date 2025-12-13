#include "tripwire_grid_index.hpp"

yodau::backend::grid_line_index
yodau::backend::build_grid_line_index(const line& l, const grid_dims& g) {
    grid_line_index out {};
    out.dims = g;

    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    out.segments = compile_line_to_grid_segments(l, g);
    out.cell_to_segments.resize(static_cast<std::size_t>(g.nx * g.ny));

    for (std::size_t seg_i = 0; seg_i < out.segments.size(); ++seg_i) {
        const auto& seg = out.segments[seg_i];

        for (const auto& c : seg.cells) {
            const int idx = grid_index(c, g);
            out.cell_to_segments[static_cast<std::size_t>(idx)].push_back(
                seg_i
            );
        }
    }

    return out;
}
