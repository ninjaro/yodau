#ifndef YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP

#include "analysis/tripwire_grid_index.hpp"
#include "geometry/geometry.hpp"
#include "streams/event.hpp"
#include "streams/stream.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace yodau::core {

struct processing_tripwire_crossing {
    std::string line_name;
    point position_pct;
    std::string direction;
    double strength { 1.0 };
};

std::vector<processing_tripwire_crossing> tripwire_crossings_for_motion(
    const stream& stream_value, const point& previous_center,
    const point& current_center
);

std::vector<processing_tripwire_crossing> tripwire_crossings_for_contour_line(
    const line& line_value, const point& previous_center,
    const point& current_center, const std::vector<point>& contour_pct,
    const grid_line_index* line_index = nullptr,
    const std::vector<size_t>* candidate_segment_indices = nullptr
);

std::string tripwire_crossing_key(const processing_tripwire_crossing& crossing);

event make_tripwire_event(
    std::string stream_name, const processing_tripwire_crossing& crossing,
    std::chrono::steady_clock::time_point timestamp, double speed
);

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP
