#ifndef YODAU_CORE_GEOMETRY_HPP
#define YODAU_CORE_GEOMETRY_HPP
#include "core/namespace_alias.hpp"
#include <memory>
#include <string>
#include <vector>

namespace yodau::core {
struct point {
    float x {}; // percentage [0.0; 100.0]
    float y {}; // percentage [0.0; 100.0]

    static constexpr float epsilon { 0.001f };

    float distance_to(const point& other) const;

    bool compare(const point& other) const;
};

enum class tripwire_dir { any, neg_to_pos, pos_to_neg };

// Core line geometry stays intentionally minimal. Width, string-length,
// damping, or other richer semantics belong in app-only settings today
// and should move into a separate profile type later rather than widening this
// geometry struct implicitly.
struct line {
    std::string name;
    std::vector<point> points;
    bool closed { false };
    tripwire_dir dir { tripwire_dir::any };

    void dump(std::ostream& out) const;
    void normalize();
    bool operator==(const line& other) const;
};

// Richer line or string semantics live beside `line` instead of widening the
// core geometry struct. The current runtime does not consume this profile yet,
// but it provides a core-owned foothold for future width/response settings.
struct line_profile {
    std::string line_name;
    float visual_width { 1.0f };
    float interaction_width { 0.0f };
    float effective_length { 1.0f };
    float damping { 0.5f };

    void normalize();
    bool operator==(const line_profile& other) const;
};

using line_ptr = std::shared_ptr<line const>;

line_ptr
make_line(std::vector<point> points, std::string name, bool closed = false);
line_profile make_line_profile(
    std::string line_name, float visual_width = 1.0f,
    float interaction_width = 0.0f, float effective_length = 1.0f,
    float damping = 0.5f
);

std::vector<point> parse_points(const std::string& points_str);

std::string normalize_str(std::string_view str);
float parse_float(std::string_view num_str);

}

#endif // YODAU_CORE_GEOMETRY_HPP
