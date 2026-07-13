#ifndef YODAU_CORE_ANALYSIS_PROCESSING_CANDIDATE_SOURCE_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_CANDIDATE_SOURCE_HPP

#include "core/namespace_alias.hpp"
#include "geometry/geometry.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace yodau::core {

struct processing_candidate_box_pct {
    point top_left_pct;
    point bottom_right_pct;
};

struct processing_candidate {
    point center_pct;
    std::optional<processing_candidate_box_pct> box_pct;
    std::vector<point> mask_pct;
    double confidence { 1.0 };
    std::optional<std::string> class_id;
    double area_px2 { 0.0 };
};

struct processing_candidate_set {
    std::string source_id;
    std::vector<processing_candidate> candidates;
};

class processing_candidate_source {
public:
    virtual ~processing_candidate_source() = default;

    [[nodiscard]] virtual std::string source_id() const = 0;
    virtual processing_candidate_set
    candidates_for_frame(const stream& stream_value, const frame& frame_value)
        = 0;
};

inline processing_candidate_box_pct
candidate_center_box_pct(const point center_pct) {
    return processing_candidate_box_pct {
        .top_left_pct = center_pct,
        .bottom_right_pct = center_pct,
    };
}

inline std::optional<processing_candidate_box_pct>
candidate_box_from_mask_pct(const std::vector<point>& mask_pct) {
    if (mask_pct.empty()) {
        return std::nullopt;
    }

    point top_left = mask_pct.front();
    point bottom_right = mask_pct.front();
    for (const point& point_value : mask_pct) {
        top_left.x = std::min(top_left.x, point_value.x);
        top_left.y = std::min(top_left.y, point_value.y);
        bottom_right.x = std::max(bottom_right.x, point_value.x);
        bottom_right.y = std::max(bottom_right.y, point_value.y);
    }

    return processing_candidate_box_pct {
        .top_left_pct = top_left,
        .bottom_right_pct = bottom_right,
    };
}

inline processing_candidate make_processing_candidate(
    const point center_pct, const double area_px2,
    std::vector<point> mask_pct = {}, const double confidence = 1.0,
    std::optional<std::string> class_id = std::nullopt
) {
    processing_candidate candidate {
        .center_pct = center_pct,
        .box_pct = candidate_center_box_pct(center_pct),
        .mask_pct = std::move(mask_pct),
        .confidence = std::clamp(confidence, 0.0, 1.0),
        .class_id = std::move(class_id),
        .area_px2 = std::max(0.0, area_px2),
    };

    if (auto box = candidate_box_from_mask_pct(candidate.mask_pct)) {
        candidate.box_pct = *box;
    }

    return candidate;
}

} // namespace yodau::core

#endif // YODAU_CORE_ANALYSIS_PROCESSING_CANDIDATE_SOURCE_HPP
