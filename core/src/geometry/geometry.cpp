#include "geometry/geometry.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

int orient(
    const yodau::core::point& a, const yodau::core::point& b,
    const yodau::core::point& c
) {
    const float value = yodau::core::cross_z(a, b, c);
    if (value > yodau::core::point::epsilon) {
        return 1;
    }
    if (value < -yodau::core::point::epsilon) {
        return -1;
    }
    return 0;
}

bool between(const float a, const float b, const float c) {
    const auto [lo, hi] = std::minmax(a, b);
    return lo <= c + yodau::core::point::epsilon
        && c <= hi + yodau::core::point::epsilon;
}

void validate_point_coordinate(
    const float value, const size_t point_index, const char axis
) {
    const std::string field
        = "line point " + std::to_string(point_index + 1U) + " " + axis;
    if (!std::isfinite(value)) {
        throw std::invalid_argument(field + " coordinate must be finite");
    }
    if (value < 0.0f || value > 100.0f) {
        throw std::invalid_argument(
            field + " coordinate " + std::to_string(value)
            + " is outside the inclusive range [0, 100]"
        );
    }
}

bool has_usable_name(const std::string_view name) {
    bool has_visible_character = false;
    for (const char value : name) {
        const auto ch = static_cast<unsigned char>(value);
        if (std::iscntrl(ch) != 0) {
            return false;
        }
        if (std::isspace(ch) == 0) {
            has_visible_character = true;
        }
    }
    return has_visible_character;
}

bool has_distinct_point_count(
    const std::span<const yodau::core::point> points,
    const size_t required_count
) {
    std::vector<yodau::core::point> distinct;
    distinct.reserve(required_count);
    for (const auto& value : points) {
        const bool already_seen = std::ranges::any_of(
            distinct, [&value](const yodau::core::point& other) {
                return value.compare(other);
            }
        );
        if (!already_seen) {
            distinct.push_back(value);
            if (distinct.size() >= required_count) {
                return true;
            }
        }
    }
    return false;
}

double doubled_polygon_area(const std::span<const yodau::core::point> points) {
    double area = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& current = points[i];
        const auto& next = points[(i + 1U) % points.size()];
        area += static_cast<double>(current.x) * static_cast<double>(next.y)
            - static_cast<double>(next.x) * static_cast<double>(current.y);
    }
    return std::abs(area);
}

} // namespace

float yodau::core::point::distance_to(const point& other) const {
    const float dx = x - other.x;
    const float dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool yodau::core::point::compare(const point& other) const {
    return std::fabs(x - other.x) < epsilon && std::fabs(y - other.y) < epsilon;
}

void yodau::core::line::dump(std::ostream& out) const {
    out << "Line(name=" << name << ", closed=" << (closed ? "true" : "false")
        << ", points=[";
    for (size_t i = 0; i < points.size(); i++) {
        out << "(" << points[i].x << ", " << points[i].y << ")";
        if (i < points.size() - 1) {
            out << "; ";
        }
    }
    out << "])";
}

void yodau::core::line::normalize() {
    const size_t n = points.size();
    if (n < 2) {
        return;
    }
    constexpr point origin { 0.0f, 0.0f };
    constexpr point east { 100.0f, 0.0f };

    if (closed) {
        size_t best_idx = 0;
        float best_distance = points[0].distance_to(origin);

        for (size_t i = 1; i < n; i++) {
            float distance = points[i].distance_to(origin);
            if (distance < best_distance) {
                best_distance = distance;
                best_idx = i;
            }
        }

        if (best_idx != 0) {
            const auto it
                = points.begin() + static_cast<std::ptrdiff_t>(best_idx);
            std::ranges::rotate(points, it);
        }
    }

    const size_t front = closed ? 1 : 0;

    if (n >= 2 + front) {
        const float first = points[front].distance_to(closed ? east : origin);
        const float last = points.back().distance_to(closed ? east : origin);
        if (last < first) {
            std::reverse(
                points.begin() + static_cast<std::ptrdiff_t>(front),
                points.end()
            );
        }
    }
}

bool yodau::core::line::operator==(const line& other) const {
    if (closed != other.closed || points.size() != other.points.size()) {
        return false;
    }
    for (size_t i = 0; i < points.size(); i++) {
        if (!points[i].compare(other.points[i])) {
            return false;
        }
    }
    return true;
}

void yodau::core::line_profile::normalize() {
    constexpr float maximum_profile_extent = 100.0f;
    if (!std::isfinite(visual_width) || visual_width <= point::epsilon) {
        visual_width = 1.0f;
    } else {
        visual_width = std::min(visual_width, maximum_profile_extent);
    }
    if (!std::isfinite(interaction_width)
        || interaction_width <= point::epsilon) {
        interaction_width = visual_width;
    } else {
        interaction_width = std::min(interaction_width, maximum_profile_extent);
    }
    if (!std::isfinite(effective_length)
        || effective_length <= point::epsilon) {
        effective_length = 1.0f;
    } else {
        effective_length = std::min(effective_length, maximum_profile_extent);
    }
    damping = std::isfinite(damping) ? std::clamp(damping, 0.0f, 1.0f) : 0.5f;
}

bool yodau::core::line_profile::operator==(const line_profile& other) const {
    return line_name == other.line_name
        && std::fabs(visual_width - other.visual_width) < point::epsilon
        && std::fabs(interaction_width - other.interaction_width)
        < point::epsilon
        && std::fabs(effective_length - other.effective_length) < point::epsilon
        && std::fabs(damping - other.damping) < point::epsilon;
}

yodau::core::line_ptr yodau::core::make_line(
    std::vector<point> points, std::string name, bool closed
) {
    auto line_ptr = std::make_shared<line>();
    line_ptr->points = std::move(points);
    line_ptr->name = std::move(name);
    line_ptr->closed = closed;
    line_ptr->normalize();
    return line_ptr;
}

yodau::core::line_profile yodau::core::make_line_profile(
    std::string line_name, const float visual_width,
    const float interaction_width, const float effective_length,
    const float damping
) {
    line_profile profile;
    profile.line_name = std::move(line_name);
    profile.visual_width = visual_width;
    profile.interaction_width = interaction_width;
    profile.effective_length = effective_length;
    profile.damping = damping;
    profile.normalize();
    return profile;
}

void yodau::core::validate_line_geometry(
    const std::span<const point> points, const std::string_view name,
    const bool closed
) {
    if (!has_usable_name(name)) {
        throw std::invalid_argument(
            "line name must contain a visible character and no control "
            "characters"
        );
    }

    const size_t minimum_points = closed ? 3U : 2U;
    if (points.size() < minimum_points) {
        throw std::invalid_argument(
            std::string(closed ? "closed line" : "open line")
            + " requires at least " + std::to_string(minimum_points) + " points"
        );
    }

    for (size_t i = 0; i < points.size(); ++i) {
        validate_point_coordinate(points[i].x, i, 'x');
        validate_point_coordinate(points[i].y, i, 'y');
        if (i > 0U && points[i].compare(points[i - 1U])) {
            throw std::invalid_argument(
                "line points " + std::to_string(i) + " and "
                + std::to_string(i + 1U) + " must be distinct"
            );
        }
    }

    const size_t minimum_distinct = closed ? 3U : 2U;
    if (!has_distinct_point_count(points, minimum_distinct)) {
        throw std::invalid_argument(
            std::string(closed ? "closed line" : "open line")
            + " requires at least " + std::to_string(minimum_distinct)
            + " distinct points"
        );
    }

    if (!closed) {
        return;
    }
    if (points.front().compare(points.back())) {
        throw std::invalid_argument(
            "closed line must not repeat its first point at the end"
        );
    }
    if (doubled_polygon_area(points) <= point::epsilon) {
        throw std::invalid_argument(
            "closed line points must form a non-degenerate polygon"
        );
    }
}

float yodau::core::cross_z(const point& a, const point& b, const point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool yodau::core::point_on_segment(
    const point& a, const point& b, const point& value
) {
    return orient(a, b, value) == 0 && between(a.x, b.x, value.x)
        && between(a.y, b.y, value.y);
}

bool yodau::core::segments_intersect(
    const point& p1, const point& p2, const point& q1, const point& q2
) {
    const int o1 = orient(p1, p2, q1);
    const int o2 = orient(p1, p2, q2);
    const int o3 = orient(q1, q2, p1);
    const int o4 = orient(q1, q2, p2);

    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && point_on_segment(p1, p2, q1)) {
        return true;
    }
    if (o2 == 0 && point_on_segment(p1, p2, q2)) {
        return true;
    }
    if (o3 == 0 && point_on_segment(q1, q2, p1)) {
        return true;
    }
    if (o4 == 0 && point_on_segment(q1, q2, p2)) {
        return true;
    }

    return false;
}

std::optional<yodau::core::point> yodau::core::segment_intersection(
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

    if (t < -point::epsilon || t > 1.0f + point::epsilon || u < -point::epsilon
        || u > 1.0f + point::epsilon) {
        return std::nullopt;
    }

    point value;
    value.x = p1.x + t * rpx;
    value.y = p1.y + t * rpy;
    return value;
}

std::vector<yodau::core::point>
yodau::core::parse_points(const std::string& points_str) {
    std::string normalized = normalize_str(points_str);
    std::string_view input { normalized };
    std::vector<point> points;
    size_t start = 0;
    while (start < input.size()) {
        size_t end = input.find(';', start);
        if (end == std::string_view::npos) {
            end = input.size();
        }
        std::string_view segment { input.substr(start, end - start) };
        if (!segment.empty()) {
            const size_t comma_pos = segment.find(',');
            if (comma_pos == std::string_view::npos) {
                throw std::runtime_error(
                    "Missing comma separator: " + std::string(segment)
                );
            }
            std::string_view x_str = segment.substr(0, comma_pos);
            std::string_view y_str = segment.substr(comma_pos + 1);
            if (x_str.empty() || y_str.empty()) {
                throw std::runtime_error(
                    "Empty coordinate in point: " + std::string(segment)
                );
            }
            float x = parse_float(x_str);
            float y = parse_float(y_str);
            validate_point_coordinate(x, points.size(), 'x');
            validate_point_coordinate(y, points.size(), 'y');
            points.emplace_back(x, y);
        }
        start = end + 1;
    }
    if (points.empty()) {
        throw std::runtime_error(
            "No valid points found in input: " + points_str
        );
    }
    return points;
}

std::string yodau::core::normalize_str(const std::string_view str) {
    std::string normalized;
    normalized.reserve(str.size());
    for (const char ch : str) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '('
            || ch == ')') {
            continue;
        }
        normalized.push_back(ch);
    }
    return normalized;
}

float yodau::core::parse_float(const std::string_view num_str) {
    float value {};
    const char* first = num_str.data();
    const char* last = num_str.data() + num_str.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc() || ptr != last) {
        throw std::runtime_error(
            "Invalid float value: " + std::string(num_str)
        );
    }
    return value;
}
