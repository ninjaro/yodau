#include "analysis/tripwire_grid_stream_index.hpp"

#include <algorithm>
#include <cmath>

namespace yodau::core::tripwire_grid_stream_index_support {

constexpr int default_grid_side = 8;
constexpr int min_grid_side = 6;
constexpr int max_grid_side = 32;

size_t segment_count_for_line(const line_ptr& line_ptr_value) {
    if (!line_ptr_value) {
        return 0;
    }

    const size_t point_count = line_ptr_value->points.size();
    if (point_count < 2) {
        return 0;
    }

    size_t segment_count = point_count - 1;
    if (line_ptr_value->closed && point_count > 2) {
        ++segment_count;
    }
    return segment_count;
}

float bbox_span_pct(const pct_bbox& bbox) {
    const float width = bbox.max_x - bbox.min_x;
    const float height = bbox.max_y - bbox.min_y;
    return std::max(width, height);
}

int clamp_grid_side(const int side) {
    return std::clamp(side, min_grid_side, max_grid_side);
}

} // namespace yodau::core::tripwire_grid_stream_index_support

yodau::core::pct_bbox
yodau::core::compute_pct_bbox(const std::vector<point>& pts, bool& ok) {
    pct_bbox b {};
    ok = false;

    if (pts.empty()) {
        return b;
    }

    b.min_x = pts[0].x;
    b.max_x = pts[0].x;
    b.min_y = pts[0].y;
    b.max_y = pts[0].y;

    for (size_t i = 1; i < pts.size(); ++i) {
        const auto& p = pts[i];

        if (p.x < b.min_x) {
            b.min_x = p.x;
        }
        if (p.x > b.max_x) {
            b.max_x = p.x;
        }
        if (p.y < b.min_y) {
            b.min_y = p.y;
        }
        if (p.y > b.max_y) {
            b.max_y = p.y;
        }
    }

    ok = true;
    return b;
}

void yodau::core::grid_candidate_tracker::ensure_size(const size_t n) {
    if (seen.size() != n) {
        seen.assign(n, 0);
        stamp = 1;
    }
}

void yodau::core::grid_candidate_tracker::next_stamp() {
    stamp++;
    if (stamp == 0) {
        for (auto& v : seen) {
            v = 0;
        }
        stamp = 1;
    }
}

yodau::core::grid_stream_index yodau::core::build_grid_stream_index(
    const std::vector<line_ptr>& input_lines, const grid_dims& g
) {
    grid_stream_index out {};
    out.dims = g;

    if (g.nx <= 0 || g.ny <= 0) {
        return out;
    }

    out.cell_to_segment_ids.resize(static_cast<size_t>(g.nx * g.ny));

    for (const auto& lp : input_lines) {
        if (!lp) {
            continue;
        }

        grid_compiled_line cl {};
        cl.name = lp->name;
        cl.dir = lp->dir;
        cl.index = build_grid_line_index(*lp, g);
        cl.bbox = compute_pct_bbox(lp->points, cl.bbox_ok);

        out.lines.push_back(std::move(cl));
    }

    for (size_t li = 0; li < out.lines.size(); ++li) {
        const auto& line_idx = out.lines[li].index;

        for (size_t si = 0; si < line_idx.segments.size(); ++si) {
            const size_t id = out.segments.size();

            grid_segment_ref ref {};
            ref.id = id;
            ref.line_index = li;
            ref.seg_index = si;

            out.segments.push_back(ref);

            const auto& seg = line_idx.segments[si];
            for (const auto& c : seg.cells) {
                const int idx = grid_index(c, g);
                out.cell_to_segment_ids[static_cast<size_t>(idx)].push_back(id);
            }
        }
    }

    return out;
}

yodau::core::grid_dims yodau::core::recommend_grid_dims(
    const std::vector<line_ptr>& input_lines, const line* focus_line
) {
    using namespace tripwire_grid_stream_index_support;

    size_t line_count = 0;
    size_t segment_count = 0;
    float span_sum_pct = 0.0f;
    float area_sum_pct = 0.0f;
    int compact_line_count = 0;
    bool focus_bbox_ok = false;
    pct_bbox focus_bbox {};

    for (const auto& line_ptr_value : input_lines) {
        if (!line_ptr_value) {
            continue;
        }

        const size_t line_segments = segment_count_for_line(line_ptr_value);
        if (line_segments == 0) {
            continue;
        }

        ++line_count;
        segment_count += line_segments;

        bool bbox_ok = false;
        const pct_bbox bbox
            = compute_pct_bbox(line_ptr_value->points, bbox_ok);
        if (!bbox_ok) {
            continue;
        }

        const float width = std::max(0.0f, bbox.max_x - bbox.min_x);
        const float height = std::max(0.0f, bbox.max_y - bbox.min_y);
        const float span = bbox_span_pct(bbox);

        span_sum_pct += span;
        area_sum_pct += width * height;
        if (span <= 14.0f) {
            ++compact_line_count;
        }

        if (focus_line != nullptr && line_ptr_value.get() == focus_line) {
            focus_bbox = bbox;
            focus_bbox_ok = true;
        }
    }

    if (segment_count == 0) {
        return grid_dims { default_grid_side, default_grid_side };
    }

    const double base_score = std::sqrt(
        static_cast<double>(segment_count) * 2.5
        + static_cast<double>(line_count) * 1.5
    );

    int side = static_cast<int>(std::lround(base_score));
    if (line_count >= 8) {
        side += 2;
    }
    if (compact_line_count * 2 >= static_cast<int>(line_count)) {
        side += 2;
    }

    const float avg_span_pct
        = line_count > 0
        ? span_sum_pct / static_cast<float>(line_count)
        : 100.0f;
    const float avg_area_pct
        = line_count > 0
        ? area_sum_pct / static_cast<float>(line_count)
        : 10000.0f;

    if (avg_span_pct > 40.0f) {
        side -= 2;
    }
    if (avg_span_pct > 65.0f) {
        side -= 2;
    }
    if (avg_area_pct < 180.0f) {
        side += 2;
    }

    if (focus_bbox_ok) {
        const float focus_span_pct = bbox_span_pct(focus_bbox);
        if (focus_span_pct <= 8.0f) {
            side += 6;
        } else if (focus_span_pct <= 14.0f) {
            side += 4;
        } else if (focus_span_pct <= 22.0f) {
            side += 2;
        }
    }

    side = clamp_grid_side(side);
    return grid_dims { side, side };
}

void yodau::core::collect_grid_candidates(
    const grid_stream_index& idx, const std::vector<int>& active_cell_indices,
    grid_candidate_tracker& tracker, std::vector<size_t>& out_segment_ids
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
            = idx.cell_to_segment_ids[static_cast<size_t>(cell_idx)];
        for (const size_t id : ids) {
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
