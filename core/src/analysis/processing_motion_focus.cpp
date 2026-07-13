#include "analysis/processing_motion_focus.hpp"

#ifdef YODAU_OPENCV

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace yodau::core {

namespace {

    std::string normalized_focus_id(std::string_view value) {
        std::string normalized;
        normalized.reserve(value.size());

        for (const char ch : value) {
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '-') {
                if (!normalized.empty() && normalized.back() != '_') {
                    normalized.push_back('_');
                }
                continue;
            }

            normalized.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))
            );
        }

        while (!normalized.empty() && normalized.back() == '_') {
            normalized.pop_back();
        }

        return normalized;
    }

    cv::Point point_to_pixel(const point& value, const cv::Size& frame_size) {
        const int max_x = std::max(0, frame_size.width - 1);
        const int max_y = std::max(0, frame_size.height - 1);
        return { std::clamp(
                     static_cast<int>(std::lround(
                         static_cast<double>(value.x) / 100.0 * max_x
                     )),
                     0, max_x
                 ),
                 std::clamp(
                     static_cast<int>(std::lround(
                         static_cast<double>(value.y) / 100.0 * max_y
                     )),
                     0, max_y
                 ) };
    }

    std::vector<cv::Point>
    line_points_px(const line& line_value, const cv::Size& frame_size) {
        std::vector<cv::Point> points;
        points.reserve(line_value.points.size());

        for (const point& point_value : line_value.points) {
            points.push_back(point_to_pixel(point_value, frame_size));
        }

        return points;
    }

    int corridor_thickness_px(
        const stream& stream_value, const line& line_value,
        const cv::Size& frame_size,
        const processing_motion_focus_options& options
    ) {
        float width_pct = std::max(options.corridor_width_pct, 0.1f);
        if (const auto profile
            = stream_value.find_line_profile(line_value.name)) {
            width_pct = std::max(width_pct, profile->interaction_width);
        }

        const int base
            = std::max(1, std::min(frame_size.width, frame_size.height));
        return std::max(
            3,
            static_cast<int>(
                std::lround(static_cast<double>(base) * width_pct / 100.0)
            )
        );
    }

    bool include_regions(const processing_motion_focus_mode mode) {
        // Automatic mode keeps the full frame so the downstream region filter
        // can account for and diagnose detections outside configured regions.
        // Region masking is an explicit performance/behavior choice.
        return mode == processing_motion_focus_mode::regions;
    }

    bool include_corridors(const processing_motion_focus_mode mode) {
        // Open tripwires need observations from both sides to establish motion
        // and crossing direction. Applying a narrow corridor in automatic mode
        // can discard both observations before an object reaches the line.
        // Operators can still opt into corridor-only processing explicitly.
        return mode == processing_motion_focus_mode::corridors;
    }

} // namespace

processing_motion_focus_mode motion_focus_mode_from_id(std::string_view value) {
    const std::string normalized = normalized_focus_id(value);
    if (normalized == "off" || normalized == "none"
        || normalized == "disabled") {
        return processing_motion_focus_mode::off;
    }
    if (normalized == "regions" || normalized == "roi"
        || normalized == "closed_regions") {
        return processing_motion_focus_mode::regions;
    }
    if (normalized == "corridors" || normalized == "line_corridors"
        || normalized == "tripwire_corridors") {
        return processing_motion_focus_mode::corridors;
    }
    return processing_motion_focus_mode::auto_focus;
}

std::string
processing_motion_focus_mode_id(const processing_motion_focus_mode mode) {
    if (mode == processing_motion_focus_mode::off) {
        return "off";
    }
    if (mode == processing_motion_focus_mode::regions) {
        return "regions";
    }
    if (mode == processing_motion_focus_mode::corridors) {
        return "corridors";
    }
    return "auto";
}

processing_motion_focus_result build_motion_focus_mask(
    const stream& stream_value, const cv::Size frame_size,
    const processing_motion_focus_options& options
) {
    processing_motion_focus_result result;
    if (options.mode == processing_motion_focus_mode::off
        || frame_size.width <= 0 || frame_size.height <= 0) {
        return result;
    }

    const auto lines = stream_value.lines_snapshot();
    if (lines.empty()) {
        return result;
    }

    result.mask = cv::Mat::zeros(frame_size, CV_8UC1);

    for (const auto& line_ptr_value : lines) {
        if (!line_ptr_value || line_ptr_value->points.size() < 2) {
            continue;
        }

        const auto points_px = line_points_px(*line_ptr_value, frame_size);
        if (line_ptr_value->closed && points_px.size() >= 3
            && include_regions(options.mode)) {
            std::vector<std::vector<cv::Point>> polygons { points_px };
            cv::fillPoly(result.mask, polygons, cv::Scalar(255));
            result.shape_count += 1;
            continue;
        }

        if (!line_ptr_value->closed && include_corridors(options.mode)) {
            const int thickness = corridor_thickness_px(
                stream_value, *line_ptr_value, frame_size, options
            );
            for (size_t index = 1; index < points_px.size(); ++index) {
                cv::line(
                    result.mask, points_px[index - 1], points_px[index],
                    cv::Scalar(255), thickness, cv::LINE_AA
                );
            }
            result.shape_count += 1;
        }
    }

    if (result.shape_count == 0) {
        result.mask = cv::Mat {};
    }
    return result;
}

} // namespace yodau::core

#endif
