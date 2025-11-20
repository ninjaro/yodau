#include "geometry.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

float yodau::backend::point::distance_to(const point& other) const {
    const float dx = x - other.x;
    const float dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool yodau::backend::point::compare(const point& other) const {
    return std::fabs(x - other.x) < epsilon && std::fabs(y - other.y) < epsilon;
}

void yodau::backend::line::dump(std::ostream& out) const {
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

void yodau::backend::line::normalize() {
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

bool yodau::backend::line::operator==(const line& other) const {
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

yodau::backend::line_ptr yodau::backend::make_line(
    std::vector<point> points, std::string name, bool closed
) {
    auto line_ptr = std::make_shared<line>();
    line_ptr->points = std::move(points);
    line_ptr->name = std::move(name);
    line_ptr->closed = closed;
    line_ptr->normalize();
    return line_ptr;
}

std::vector<yodau::backend::point>
yodau::backend::parse_points(const std::string& points_str) {
    std::string_view input { normalize_str(points_str) };
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

std::string yodau::backend::normalize_str(const std::string_view str) {
    std::string normalized;
    normalized.reserve(str.size());
    for (const char ch : str) {
        if (std::isspace(ch) || ch == '(' || ch == ')') {
            continue;
        }
        normalized.push_back(ch);
    }
    return normalized;
}

float yodau::backend::parse_float(const std::string_view num_str) {
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
