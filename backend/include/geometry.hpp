#ifndef YODAU_BACKEND_GEOMETRY_HPP
#define YODAU_BACKEND_GEOMETRY_HPP

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace yodau::backend {

/**
 * @brief Point in percentage-based image coordinates.
 *
 * Coordinates are expressed in the range [0.0; 100.0], where:
 * - x = 0   is the left edge, x = 100 is the right edge
 * - y = 0   is the top edge,  y = 100 is the bottom edge
 */
struct point {
    /**
     * @brief Horizontal coordinate (percentage of width).
     */
    float x {}; // percentage [0.0; 100.0]

    /**
     * @brief Vertical coordinate (percentage of height).
     */
    float y {}; // percentage [0.0; 100.0]

    /**
     * @brief Tolerance used for fuzzy point comparisons.
     */
    static constexpr float epsilon { 0.001f };

    /**
     * @brief Compute Euclidean distance to another point.
     *
     * Distance is computed in percentage units.
     *
     * @param other Target point.
     * @return Euclidean distance between this and @p other.
     */
    float distance_to(const point& other) const;

    /**
     * @brief Compare two points with tolerance @ref epsilon.
     *
     * The comparison is component-wise:
     * @code
     * abs(x - other.x) < epsilon && abs(y - other.y) < epsilon
     * @endcode
     *
     * @param other Point to compare to.
     * @return true if points are equal within tolerance.
     */
    bool compare(const point& other) const;
};

/**
 * @brief Allowed crossing direction for a tripwire.
 */
enum class tripwire_dir {
    /** Direction is not constrained; any crossing counts. */
    any,
    /** Crossing from negative side to positive side counts. */
    neg_to_pos,
    /** Crossing from positive side to negative side counts. */
    pos_to_neg
};

/**
 * @brief Polyline / polygon described in percentage coordinates.
 *
 * A line can represent:
 * - an open polyline (when @ref closed == false),
 * - or a closed polygon-like chain (when @ref closed == true).
 *
 * The points may be reordered by @ref normalize() to provide a canonical
 * representation for equality checks and stable processing.
 */
struct line {
    /**
     * @brief Logical name of the line (e.g., "entrance_tripwire").
     */
    std::string name;

    /**
     * @brief Vertex list in percentage coordinates.
     */
    std::vector<point> points;

    /**
     * @brief Whether the chain is closed.
     *
     * If true, the first point is considered connected to the last point.
     */
    bool closed { false };

    /**
     * @brief Optional tripwire direction constraint.
     *
     * Used by line-crossing logic. Defaults to @ref tripwire_dir::any.
     */
    tripwire_dir dir { tripwire_dir::any };

    /**
     * @brief Print a human-readable representation of the line.
     *
     * @param out Output stream to write to.
     */
    void dump(std::ostream& out) const;

    /**
     * @brief Canonicalize point order.
     *
     * Behavior (as per implementation):
     * - If @ref closed is true, rotates points so the vertex closest to (0,0)
     *   becomes the first element.
     * - Then ensures a consistent direction by comparing distances of
     *   first/last vertices to a reference point and reversing if needed.
     *
     * @note This operation may reorder @ref points but does not change
     *       their values.
     */
    void normalize();

    /**
     * @brief Equality check using canonical point comparison.
     *
     * Two lines are equal if:
     * - their @ref closed flags match,
     * - they have the same number of points,
     * - and all points compare equal via @ref point::compare.
     *
     * @note The @ref name and @ref dir fields are intentionally NOT compared.
     *
     * @param other Line to compare with.
     * @return true if geometrically equal within tolerance.
     */
    bool operator==(const line& other) const;
};

/**
 * @brief Shared, immutable line pointer.
 */
using line_ptr = std::shared_ptr<line const>;

/**
 * @brief Create and normalize a line.
 *
 * Allocates a new @ref line, moves in @p points and @p name, sets @p closed,
 * and calls @ref line::normalize().
 *
 * @param points Vertex list to move into the line.
 * @param name Logical name to move into the line.
 * @param closed Whether the line should be treated as closed.
 * @return A shared pointer to an immutable line instance.
 */
line_ptr
make_line(std::vector<point> points, std::string name, bool closed = false);

/**
 * @brief Parse points from a textual representation.
 *
 * Input format:
 * - Points are separated by semicolons ';'
 * - Each point is "x,y"
 * - Whitespace and parentheses are ignored.
 *
 * Examples:
 * - "(10, 20); (30,40)"
 * - "10,20;30,40; 50, 60"
 *
 * @param points_str Input string containing points.
 * @return Parsed list of points.
 * @throws std::runtime_error if parsing fails or no valid points are found.
 */
std::vector<point> parse_points(const std::string& points_str);

/**
 * @brief Remove whitespace and parentheses from a string.
 *
 * Used internally to simplify parsing of point lists.
 *
 * @param str Input string view.
 * @return Normalized string with spaces and '(' / ')' removed.
 */
std::string normalize_str(std::string_view str);

/**
 * @brief Parse a float from a string view.
 *
 * Uses std::from_chars for locale-independent parsing.
 *
 * @param num_str Number representation.
 * @return Parsed float value.
 * @throws std::runtime_error if the input is not a valid float.
 */
float parse_float(std::string_view num_str);

} // namespace yodau::backend

#endif // YODAU_BACKEND_GEOMETRY_HPP