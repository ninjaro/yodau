#include "geometry.hpp"

#include <algorithm>
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
