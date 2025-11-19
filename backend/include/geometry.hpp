#ifndef YODAU_BACKEND_GEOMETRY_HPP
#define YODAU_BACKEND_GEOMETRY_HPP
#include <memory>
#include <vector>

namespace yodau::backend {
struct point {
    float x {}; // percentage [0.0; 100.0]
    float y {}; // percentage [0.0; 100.0]

    static constexpr float epsilon { 0.001f };

    float distance_to(const point& other) const;

    bool compare(const point& other) const;
};

struct line {
    std::string name;
    std::vector<point> points;
    bool closed { false };

    void dump(std::ostream& out) const;
    void normalize();
    bool operator==(const line& other) const;
};

using line_ptr = std::shared_ptr<line const>;

line_ptr
make_line(std::vector<point> points, std::string name, bool closed = false);
}

#endif // YODAU_BACKEND_GEOMETRY_HPP