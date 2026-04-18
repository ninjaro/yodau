#include "analysis/processing_tripwire_tools.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace yodau::core {

namespace {

float cross_z(const point& a, const point& b, const point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

int orient(const point& a, const point& b, const point& c) {
    const float value = cross_z(a, b, c);
    if (value > point::epsilon) {
        return 1;
    }
    if (value < -point::epsilon) {
        return -1;
    }
    return 0;
}

bool between(const float a, const float b, const float c) {
    const auto [lo, hi] = std::minmax(a, b);
    return lo <= c + point::epsilon && c <= hi + point::epsilon;
}

bool on_segment(const point& a, const point& b, const point& c) {
    return orient(a, b, c) == 0 && between(a.x, b.x, c.x)
        && between(a.y, b.y, c.y);
}

bool segments_intersect(
    const point& p1, const point& p2, const point& q1, const point& q2
) {
    const int o1 = orient(p1, p2, q1);
    const int o2 = orient(p1, p2, q2);
    const int o3 = orient(q1, q2, p1);
    const int o4 = orient(q1, q2, p2);

    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && on_segment(p1, p2, q1)) {
        return true;
    }
    if (o2 == 0 && on_segment(p1, p2, q2)) {
        return true;
    }
    if (o3 == 0 && on_segment(q1, q2, p1)) {
        return true;
    }
    if (o4 == 0 && on_segment(q1, q2, p2)) {
        return true;
    }

    return false;
}

std::optional<point> segment_intersection(
    const point& p1, const point& p2, const point& q1, const point& q2
) {
    const float rpx = p2.x - p1.x;
    const float rpy = p2.y - p1.y;
    const float spx = q2.x - q1.x;
    const float spy = q2.y - q1.y;

    const float denominator = rpx * spy - rpy * spx;
    if (std::abs(denominator) <= point::epsilon) {
        return std::nullopt;
    }

    const float qpx = q1.x - p1.x;
    const float qpy = q1.y - p1.y;
    const float t = (qpx * spy - qpy * spx) / denominator;
    const float u = (qpx * rpy - qpy * rpx) / denominator;

    if (t < -point::epsilon || t > 1.0f + point::epsilon
        || u < -point::epsilon || u > 1.0f + point::epsilon) {
        return std::nullopt;
    }

    point value;
    value.x = p1.x + t * rpx;
    value.y = p1.y + t * rpy;
    return value;
}

std::string crossing_direction(
    const point& line_a, const point& line_b, const point& previous_center,
    const point& current_center
) {
    const float previous_side = cross_z(line_a, line_b, previous_center);
    const float current_side = cross_z(line_a, line_b, current_center);

    if (previous_side <= 0.0f && current_side > 0.0f) {
        return "neg_to_pos";
    }
    if (previous_side >= 0.0f && current_side < 0.0f) {
        return "pos_to_neg";
    }
    return "flat";
}

bool direction_allowed(const tripwire_dir requested, const std::string& actual) {
    if (requested == tripwire_dir::neg_to_pos) {
        return actual == "neg_to_pos";
    }
    if (requested == tripwire_dir::pos_to_neg) {
        return actual == "pos_to_neg";
    }
    return true;
}

} // namespace

std::vector<processing_tripwire_crossing> tripwire_crossings_for_motion(
    const stream& stream_value, const point& previous_center,
    const point& current_center
) {
    const double dx = static_cast<double>(current_center.x - previous_center.x);
    const double dy = static_cast<double>(current_center.y - previous_center.y);
    if (std::hypot(dx, dy) <= static_cast<double>(point::epsilon)) {
        return {};
    }

    std::vector<processing_tripwire_crossing> crossings;

    const auto lines = stream_value.lines_snapshot();
    for (const auto& line_ptr_value : lines) {
        if (!line_ptr_value || line_ptr_value->points.size() < 2) {
            continue;
        }

        bool hit = false;
        point hit_a {};
        point hit_b {};
        point hit_position = current_center;
        double best_dist2 = std::numeric_limits<double>::max();

        const auto consider_segment = [&](const point& a, const point& b) {
            if (!segments_intersect(previous_center, current_center, a, b)) {
                return;
            }

            const point position
                = segment_intersection(previous_center, current_center, a, b)
                      .value_or(current_center);
            const double hit_dx = static_cast<double>(
                position.x - current_center.x
            );
            const double hit_dy = static_cast<double>(
                position.y - current_center.y
            );
            const double distance2 = hit_dx * hit_dx + hit_dy * hit_dy;
            if (distance2 < best_dist2) {
                best_dist2 = distance2;
                hit = true;
                hit_a = a;
                hit_b = b;
                hit_position = position;
            }
        };

        for (size_t index = 1; index < line_ptr_value->points.size(); ++index) {
            consider_segment(
                line_ptr_value->points[index - 1],
                line_ptr_value->points[index]
            );
        }

        if (line_ptr_value->closed && line_ptr_value->points.size() > 2) {
            consider_segment(
                line_ptr_value->points.back(), line_ptr_value->points.front()
            );
        }

        if (!hit) {
            continue;
        }

        const std::string direction = crossing_direction(
            hit_a, hit_b, previous_center, current_center
        );
        if (!direction_allowed(line_ptr_value->dir, direction)) {
            continue;
        }

        crossings.push_back(
            processing_tripwire_crossing {
                .line_name = line_ptr_value->name,
                .position_pct = hit_position,
                .direction = direction,
            }
        );
    }

    return crossings;
}

std::string tripwire_crossing_key(
    const processing_tripwire_crossing& crossing
) {
    return crossing.line_name + "|" + crossing.direction;
}

} // namespace yodau::core
