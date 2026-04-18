#include "analysis/processing_overlay_tools.hpp"

#include <utility>

namespace yodau::core {

processing_overlay make_point_overlay(std::string label, const point anchor_pct) {
    processing_overlay overlay;
    overlay.kind = processing_overlay_kind::point;
    overlay.label = std::move(label);
    overlay.anchor_pct = anchor_pct;
    return overlay;
}

processing_overlay make_polyline_overlay(
    std::string label, std::vector<point> points_pct,
    std::optional<point> anchor_pct
) {
    processing_overlay overlay;
    overlay.kind = processing_overlay_kind::polyline;
    overlay.label = std::move(label);
    overlay.points_pct = std::move(points_pct);
    overlay.anchor_pct = anchor_pct;
    return overlay;
}

processing_overlay make_polygon_overlay(
    std::string label, std::vector<point> points_pct,
    std::optional<point> anchor_pct
) {
    processing_overlay overlay;
    overlay.kind = processing_overlay_kind::polygon;
    overlay.label = std::move(label);
    overlay.points_pct = std::move(points_pct);
    overlay.anchor_pct = anchor_pct;
    return overlay;
}

} // namespace yodau::core
