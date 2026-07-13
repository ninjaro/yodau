#include "analysis/processing_tripwire_tools.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace yodau::core {

namespace {

    void consider_contour_hit(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& current_center, const point& a,
        const point& b, const point& position
    ) {
        const float dx = position.x - current_center.x;
        const float dy = position.y - current_center.y;
        const float d2 = dx * dx + dy * dy;

        if (d2 < best_dist2) {
            best_dist2 = d2;
            best_a = a;
            best_b = b;
            best_pos = position;
            hit = true;
        }
    }

    void test_line_segment_against_contour(
        bool& hit, float& best_dist2, point& best_a, point& best_b,
        point& best_pos, const point& current_center,
        const std::vector<point>& contour_pct, const point& a, const point& b,
        std::vector<point>& hit_positions_pct
    ) {
        if (contour_pct.size() < 2) {
            return;
        }

        const auto test_contour_segment = [&](const point& c1,
                                              const point& c2) {
            if (!segments_intersect(a, b, c1, c2)) {
                return;
            }

            const point position
                = segment_intersection(a, b, c1, c2).value_or(current_center);
            hit_positions_pct.push_back(position);
            consider_contour_hit(
                hit, best_dist2, best_a, best_b, best_pos, current_center, a, b,
                position
            );
        };

        for (size_t index = 1; index < contour_pct.size(); ++index) {
            test_contour_segment(contour_pct[index - 1], contour_pct[index]);
        }

        test_contour_segment(contour_pct.back(), contour_pct.front());
    }

    double contour_hit_strength(const std::vector<point>& hit_positions_pct) {
        if (hit_positions_pct.empty()) {
            return 1.0;
        }

        float min_x = hit_positions_pct[0].x;
        float max_x = hit_positions_pct[0].x;
        float min_y = hit_positions_pct[0].y;
        float max_y = hit_positions_pct[0].y;

        for (size_t index = 1; index < hit_positions_pct.size(); ++index) {
            const auto& point_value = hit_positions_pct[index];
            min_x = std::min(min_x, point_value.x);
            max_x = std::max(max_x, point_value.x);
            min_y = std::min(min_y, point_value.y);
            max_y = std::max(max_y, point_value.y);
        }

        const auto dx = static_cast<double>(max_x - min_x);
        const auto dy = static_cast<double>(max_y - min_y);
        const double span = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        const double norm = std::clamp(span / 20.0, 0.0, 1.0);
        return std::clamp(0.5 + norm * 0.5, 0.5, 1.0);
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

    bool
    direction_allowed(const tripwire_dir requested, const std::string& actual) {
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
    const auto dx = static_cast<double>(current_center.x - previous_center.x);
    const auto dy = static_cast<double>(current_center.y - previous_center.y);
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
            const auto hit_dx
                = static_cast<double>(position.x - current_center.x);
            const auto hit_dy
                = static_cast<double>(position.y - current_center.y);
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
                line_ptr_value->points[index - 1], line_ptr_value->points[index]
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

        const std::string direction
            = crossing_direction(hit_a, hit_b, previous_center, current_center);
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

std::vector<processing_tripwire_crossing> tripwire_crossings_for_contour_line(
    const line& line_value, const point& previous_center,
    const point& current_center, const std::vector<point>& contour_pct,
    const grid_line_index* line_index,
    const std::vector<size_t>* candidate_segment_indices
) {
    const auto& pts = line_value.points;
    if (pts.size() < 2) {
        return {};
    }

    bool hit = false;
    point best_a {};
    point best_b {};
    point best_position = current_center;
    float best_dist2 = std::numeric_limits<float>::max();
    std::vector<point> hit_positions_pct;

    if (line_index != nullptr && candidate_segment_indices != nullptr
        && !candidate_segment_indices->empty()) {
        for (const size_t segment_index : *candidate_segment_indices) {
            if (segment_index >= line_index->segments.size()) {
                continue;
            }

            const auto& segment = line_index->segments[segment_index];
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_position, current_center,
                contour_pct, segment.a_pct, segment.b_pct, hit_positions_pct
            );
        }
    } else {
        for (size_t index = 1; index < pts.size(); ++index) {
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_position, current_center,
                contour_pct, pts[index - 1], pts[index], hit_positions_pct
            );
        }

        if (line_value.closed && pts.size() > 2) {
            test_line_segment_against_contour(
                hit, best_dist2, best_a, best_b, best_position, current_center,
                contour_pct, pts.back(), pts.front(), hit_positions_pct
            );
        }
    }

    if (!hit) {
        return {};
    }

    if (hit_positions_pct.empty()) {
        hit_positions_pct.push_back(best_position);
    }

    const std::string direction
        = crossing_direction(best_a, best_b, previous_center, current_center);
    if (!direction_allowed(line_value.dir, direction)) {
        return {};
    }

    const double strength = contour_hit_strength(hit_positions_pct);
    std::vector<processing_tripwire_crossing> crossings;
    crossings.reserve(hit_positions_pct.size());
    for (const point& position : hit_positions_pct) {
        crossings.push_back(
            processing_tripwire_crossing {
                .line_name = line_value.name,
                .position_pct = position,
                .direction = direction,
                .strength = strength,
            }
        );
    }

    return crossings;
}

std::string
tripwire_crossing_key(const processing_tripwire_crossing& crossing) {
    return crossing.line_name + "|" + crossing.direction;
}

event make_tripwire_event(
    std::string stream_name, const processing_tripwire_crossing& crossing,
    const std::chrono::steady_clock::time_point timestamp, const double speed
) {
    event event_value;
    event_value.kind = event_kind::tripwire;
    event_value.stream_name = std::move(stream_name);
    event_value.line_name = crossing.line_name;
    event_value.ts = timestamp;
    event_value.pos_pct = crossing.position_pct;
    event_value.message = crossing.direction + "|"
        + std::to_string(crossing.strength) + "|" + std::to_string(speed);
    return event_value;
}

} // namespace yodau::core
