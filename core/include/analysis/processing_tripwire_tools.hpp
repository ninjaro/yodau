#ifndef YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP

#include "core/namespace_alias.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <string>
#include <vector>

namespace yodau::core {

struct processing_tripwire_crossing {
    std::string line_name;
    point position_pct;
    std::string direction;
};

std::vector<processing_tripwire_crossing> tripwire_crossings_for_motion(
    const stream& stream_value, const point& previous_center,
    const point& current_center
);

std::string tripwire_crossing_key(
    const processing_tripwire_crossing& crossing
);

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_TRIPWIRE_TOOLS_HPP
