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

enum class tripwire_dir { any, neg_to_pos, pos_to_neg };

struct line {
    std::string name;
    std::vector<point> points;
    bool closed { false };
    tripwire_dir dir { tripwire_dir::any };

    void dump(std::ostream& out) const;
    void normalize();
    bool operator==(const line& other) const;
};

using line_ptr = std::shared_ptr<line const>;

line_ptr
make_line(std::vector<point> points, std::string name, bool closed = false);

std::vector<point> parse_points(const std::string& points_str);

std::string normalize_str(std::string_view str);
float parse_float(std::string_view num_str);

}

#endif // YODAU_BACKEND_GEOMETRY_HPP