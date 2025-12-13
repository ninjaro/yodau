#include "tripwire_grid_stream_index.hpp"

void yodau::backend::grid_candidate_tracker::ensure_size(const std::size_t n) {
    if (seen.size() != n) {
        seen.assign(n, 0);
        stamp = 1;
    }
}

void yodau::backend::grid_candidate_tracker::next_stamp() {
    stamp++;
    if (stamp == 0) {
        for (auto& v : seen) {
            v = 0;
        }
        stamp = 1;
    }
}

yodau::backend::grid_stream_index yodau::backend::build_grid_stream_index(
    const std::vector<line_ptr>& input_lines, const grid_dims& g
) {
    grid_stream_index out {};
    out.dims = g;

    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    out.cell_to_segment_ids.resize(static_cast<std::size_t>(g.nx * g.ny));

    for (const auto& lp : input_lines) {
        if (!lp) {
            continue;
        }

        grid_compiled_line cl {};
        cl.name = lp->name;
        cl.dir = lp->dir;
        cl.index = build_grid_line_index(*lp, g);

        out.lines.push_back(std::move(cl));
    }

    for (std::size_t li = 0; li < out.lines.size(); ++li) {
        const auto& line_idx = out.lines[li].index;

        for (std::size_t si = 0; si < line_idx.segments.size(); ++si) {
            const std::size_t id = out.segments.size();

            grid_segment_ref ref {};
            ref.id = id;
            ref.line_index = li;
            ref.seg_index = si;

            out.segments.push_back(ref);

            const auto& seg = line_idx.segments[si];
            for (const auto& c : seg.cells) {
                const int idx = grid_index(c, g);
                out.cell_to_segment_ids[static_cast<std::size_t>(idx)]
                    .push_back(id);
            }
        }
    }

    return out;
}

void yodau::backend::collect_grid_candidates(
    const grid_stream_index& idx, const std::vector<int>& active_cell_indices,
    grid_candidate_tracker& tracker, std::vector<std::size_t>& out_segment_ids
) {
    out_segment_ids.clear();

    tracker.ensure_size(idx.segments.size());
    tracker.next_stamp();

    const int cells_total = idx.dims.nx * idx.dims.ny;

    for (const int cell_idx : active_cell_indices) {
        if (cell_idx < 0 || cell_idx >= cells_total) {
            continue;
        }

        const auto& ids
            = idx.cell_to_segment_ids[static_cast<std::size_t>(cell_idx)];
        for (const std::size_t id : ids) {
            if (id >= tracker.seen.size()) {
                continue;
            }

            if (tracker.seen[id] == tracker.stamp) {
                continue;
            }

            tracker.seen[id] = tracker.stamp;
            out_segment_ids.push_back(id);
        }
    }
}
