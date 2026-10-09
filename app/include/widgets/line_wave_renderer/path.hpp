#ifndef YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PATH_HPP
#define YODAU_APP_WIDGETS_LINE_WAVE_RENDERER_PATH_HPP

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace line_wave_renderer {

struct path_segment {
    int index { -1 };
    QPointF a_pct;
    QPointF b_pct;
    double start_pos { 0.0 };
    double length { 0.0 };
};

struct path_geometry {
    bool closed { false };
    double total_length { 0.0 };
    std::vector<path_segment> segments;
};

struct hit_projection {
    int segment_index { -1 };
    double path_pos { 0.0 };
};

inline double point_distance_pct(const QPointF& a_pct, const QPointF& b_pct) {
    const double dx = b_pct.x() - a_pct.x();
    const double dy = b_pct.y() - a_pct.y();
    return std::sqrt(dx * dx + dy * dy);
}

inline path_geometry
build_path_geometry(const std::vector<QPointF>& pts_pct, const bool closed) {
    path_geometry geometry;
    geometry.closed = closed;

    if (pts_pct.size() < 2) {
        return geometry;
    }

    geometry.segments.reserve(pts_pct.size() + (closed ? 1u : 0u));

    for (size_t i = 1; i < pts_pct.size(); ++i) {
        const QPointF a_pct = pts_pct[i - 1];
        const QPointF b_pct = pts_pct[i];
        const double length = point_distance_pct(a_pct, b_pct);

        if (length <= 0.0) {
            continue;
        }

        geometry.segments.push_back(
            path_segment {
                .index = static_cast<int>(geometry.segments.size()),
                .a_pct = a_pct,
                .b_pct = b_pct,
                .start_pos = geometry.total_length,
                .length = length,
            }
        );
        geometry.total_length += length;
    }

    if (closed && pts_pct.size() > 2) {
        const QPointF a_pct = pts_pct.back();
        const QPointF b_pct = pts_pct.front();
        const double length = point_distance_pct(a_pct, b_pct);

        if (length > 0.0) {
            geometry.segments.push_back(
                path_segment {
                    .index = static_cast<int>(geometry.segments.size()),
                    .a_pct = a_pct,
                    .b_pct = b_pct,
                    .start_pos = geometry.total_length,
                    .length = length,
                }
            );
            geometry.total_length += length;
        }
    }

    return geometry;
}

inline std::optional<hit_projection>
project_hit(const path_geometry& geometry, const QPointF& hit_pct) {
    if (geometry.segments.empty()) {
        return {};
    }

    hit_projection best_projection;
    double best_dist2 = std::numeric_limits<double>::max();

    for (const auto& segment : geometry.segments) {
        const double abx = segment.b_pct.x() - segment.a_pct.x();
        const double aby = segment.b_pct.y() - segment.a_pct.y();
        const double len2 = abx * abx + aby * aby;
        if (len2 <= 0.0) {
            continue;
        }

        const double apx = hit_pct.x() - segment.a_pct.x();
        const double apy = hit_pct.y() - segment.a_pct.y();
        const double t = std::clamp((apx * abx + apy * aby) / len2, 0.0, 1.0);

        const double proj_x = segment.a_pct.x() + t * abx;
        const double proj_y = segment.a_pct.y() + t * aby;
        const double dx = hit_pct.x() - proj_x;
        const double dy = hit_pct.y() - proj_y;
        const double dist2 = dx * dx + dy * dy;

        if (dist2 >= best_dist2) {
            continue;
        }

        best_dist2 = dist2;
        best_projection.segment_index = segment.index;
        best_projection.path_pos = segment.start_pos + t * segment.length;
    }

    if (best_projection.segment_index < 0) {
        return {};
    }

    return best_projection;
}

inline int segment_hops(
    const path_geometry& geometry, const int lhs_index, const int rhs_index
) {
    int hops = std::abs(lhs_index - rhs_index);
    if (!geometry.closed) {
        return hops;
    }

    const int segment_count = static_cast<int>(geometry.segments.size());
    if (segment_count <= 0) {
        return hops;
    }

    return std::min(hops, segment_count - hops);
}

inline double forward_distance(
    const path_geometry& geometry, const double origin_pos,
    const double sample_pos, const int travel_sign
) {
    if (!geometry.closed) {
        return travel_sign > 0 ? sample_pos - origin_pos
                               : origin_pos - sample_pos;
    }

    if (geometry.total_length <= 0.0) {
        return 0.0;
    }

    double delta
        = travel_sign > 0 ? sample_pos - origin_pos : origin_pos - sample_pos;

    while (delta < 0.0) {
        delta += geometry.total_length;
    }
    while (delta >= geometry.total_length) {
        delta -= geometry.total_length;
    }

    return delta;
}

} // namespace line_wave_renderer

#endif
