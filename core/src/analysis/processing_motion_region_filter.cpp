#include "analysis/processing_motion_region_filter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace yodau::core {

namespace processing_motion_region_filter_support {

    struct motion_region {
        std::string line_name;
        std::vector<point> polygon_pct;
    };

    bool polygon_contains_point(
        const std::vector<point>& polygon_pct, const point& value
    ) {
        if (polygon_pct.size() < 3) {
            return false;
        }

        bool inside = false;
        for (size_t index = 0, previous = polygon_pct.size() - 1;
             index < polygon_pct.size(); previous = index, index += 1) {
            const point& a = polygon_pct[previous];
            const point& b = polygon_pct[index];

            if (yodau::core::point_on_segment(a, b, value)) {
                return true;
            }

            const bool crosses_scanline = (a.y > value.y) != (b.y > value.y);
            if (!crosses_scanline) {
                continue;
            }

            const float denominator = b.y - a.y;
            if (std::abs(denominator) <= point::epsilon) {
                continue;
            }

            const float intersect_x
                = a.x + (b.x - a.x) * ((value.y - a.y) / denominator);
            if (intersect_x + point::epsilon >= value.x) {
                inside = !inside;
            }
        }

        return inside;
    }

    std::vector<motion_region> regions_for_stream(const stream& stream_value) {
        std::vector<motion_region> regions;

        for (const auto& line_ptr_value : stream_value.lines_snapshot()) {
            if (!line_ptr_value || !line_ptr_value->closed
                || line_ptr_value->points.size() < 3) {
                continue;
            }

            regions.push_back(
                motion_region {
                    .line_name = line_ptr_value->name,
                    .polygon_pct = line_ptr_value->points,
                }
            );
        }

        return regions;
    }

    std::vector<const motion_region*> matching_regions(
        const std::vector<motion_region>& regions, const point& value
    ) {
        std::vector<const motion_region*> matches;
        matches.reserve(regions.size());

        for (const auto& region : regions) {
            if (polygon_contains_point(region.polygon_pct, value)) {
                matches.push_back(&region);
            }
        }

        return matches;
    }

    std::optional<point> overlay_anchor_pct(const processing_overlay& overlay) {
        if (overlay.anchor_pct.has_value()) {
            return *overlay.anchor_pct;
        }

        if (overlay.points_pct.empty()) {
            return std::nullopt;
        }

        point center;
        for (const auto& point_value : overlay.points_pct) {
            center.x += point_value.x;
            center.y += point_value.y;
        }

        center.x /= static_cast<float>(overlay.points_pct.size());
        center.y /= static_cast<float>(overlay.points_pct.size());
        return center;
    }

    event roi_event_for_match(
        const event& source_event, const std::string& line_name
    ) {
        event roi_event;
        roi_event.kind = event_kind::roi;
        roi_event.stream_name = source_event.stream_name;
        roi_event.message = source_event.message.empty()
            ? std::string("motion_region_match")
            : std::string("motion_region_match|") + source_event.message;
        roi_event.ts = source_event.ts;
        roi_event.pos_pct = source_event.pos_pct;
        roi_event.line_name = line_name;
        return roi_event;
    }

    bool contains_named_region(
        const std::vector<const motion_region*>& matches,
        const std::string& line_name
    ) {
        return std::any_of(
            matches.cbegin(), matches.cend(),
            [&line_name](const motion_region* region) {
                return region != nullptr && region->line_name == line_name;
            }
        );
    }

} // namespace processing_motion_region_filter_support

processing_result processing_motion_region_filter::apply(
    const stream& stream_value, processing_result result
) {
    using namespace processing_motion_region_filter_support;

    const auto regions = regions_for_stream(stream_value);
    result.metrics.push_back(
        processing_metric {
            .name = "motion_region_count",
            .value = static_cast<double>(regions.size()),
            .unit = "regions",
        }
    );

    if (regions.empty()) {
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "motion_region_filter",
                .value = "inactive",
            }
        );
        return result;
    }

    size_t matched_event_count = 0;
    size_t dropped_event_count = 0;
    size_t emitted_roi_event_count = 0;
    std::vector<event> filtered_events;
    filtered_events.reserve(result.events.size() + regions.size());

    for (const auto& event_value : result.events) {
        if ((event_value.kind != event_kind::motion
             && event_value.kind != event_kind::roi)
            || !event_value.pos_pct.has_value()) {
            filtered_events.push_back(event_value);
            continue;
        }

        const auto matches = matching_regions(regions, *event_value.pos_pct);
        if (matches.empty()) {
            dropped_event_count += 1;
            continue;
        }

        matched_event_count += 1;

        if (event_value.kind == event_kind::motion) {
            filtered_events.push_back(event_value);
            for (const motion_region* region : matches) {
                filtered_events.push_back(
                    roi_event_for_match(event_value, region->line_name)
                );
                emitted_roi_event_count += 1;
            }
            continue;
        }

        if (!event_value.line_name.empty()) {
            if (contains_named_region(matches, event_value.line_name)) {
                filtered_events.push_back(event_value);
            } else {
                dropped_event_count += 1;
            }
            continue;
        }

        for (const motion_region* region : matches) {
            filtered_events.push_back(
                roi_event_for_match(event_value, region->line_name)
            );
            emitted_roi_event_count += 1;
        }
    }

    size_t dropped_overlay_count = 0;
    std::vector<processing_overlay> filtered_overlays;
    filtered_overlays.reserve(result.overlays.size());
    for (auto& overlay : result.overlays) {
        const auto anchor_pct = overlay_anchor_pct(overlay);
        if (!anchor_pct.has_value()) {
            filtered_overlays.push_back(std::move(overlay));
            continue;
        }

        const auto matches = matching_regions(regions, *anchor_pct);
        if (!matches.empty()) {
            filtered_overlays.push_back(std::move(overlay));
            continue;
        }

        dropped_overlay_count += 1;
    }

    result.events = std::move(filtered_events);
    result.overlays = std::move(filtered_overlays);
    result.metrics.push_back(
        processing_metric {
            .name = "motion_region_matched_event_count",
            .value = static_cast<double>(matched_event_count),
            .unit = "events",
        }
    );
    result.metrics.push_back(
        processing_metric {
            .name = "motion_region_dropped_event_count",
            .value = static_cast<double>(dropped_event_count),
            .unit = "events",
        }
    );
    result.metrics.push_back(
        processing_metric {
            .name = "motion_region_roi_event_count",
            .value = static_cast<double>(emitted_roi_event_count),
            .unit = "events",
        }
    );
    result.metrics.push_back(
        processing_metric {
            .name = "motion_region_dropped_overlay_count",
            .value = static_cast<double>(dropped_overlay_count),
            .unit = "overlays",
        }
    );
    result.diagnostics.push_back(
        processing_diagnostic {
            .key = "motion_region_filter",
            .value = dropped_event_count > 0 || emitted_roi_event_count > 0
                ? "applied"
                : "pass_through",
        }
    );

    return result;
}

} // namespace yodau::core
