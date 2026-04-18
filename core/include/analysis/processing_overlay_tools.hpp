#ifndef YODAU_CORE_ANALYSIS_PROCESSING_OVERLAY_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_OVERLAY_TOOLS_HPP

#include "analysis/processing_algorithm.hpp"
#include "core/namespace_alias.hpp"

#include <optional>
#include <string>
#include <vector>

namespace yodau::core {

processing_overlay make_point_overlay(std::string label, point anchor_pct);

processing_overlay make_polyline_overlay(
    std::string label, std::vector<point> points_pct,
    std::optional<point> anchor_pct = std::nullopt
);

processing_overlay make_polygon_overlay(
    std::string label, std::vector<point> points_pct,
    std::optional<point> anchor_pct = std::nullopt
);

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_OVERLAY_TOOLS_HPP
