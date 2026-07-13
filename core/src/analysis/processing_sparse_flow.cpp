#include "analysis/processing_sparse_flow.hpp"

#ifdef YODAU_OPENCV

#include "analysis/processing_contour_tools.hpp"
#include "analysis/processing_motion_tools.hpp"

#include <opencv2/core/version.hpp>
#if CV_VERSION_MAJOR >= 5
#include <opencv2/features.hpp>
#endif
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace yodau::core {

namespace {

    cv::Size tracking_window_size(const int window_size_px) {
        const int size = normalized_odd_kernel_size(window_size_px);
        return { size, size };
    }

} // namespace

processing_sparse_flow_result sparse_optical_flow(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    const cv::Mat& feature_mask, const processing_sparse_flow_options& options
) {
    processing_sparse_flow_result result;
    if (previous_gray.empty() || current_gray.empty()
        || previous_gray.size() != current_gray.size()) {
        return result;
    }

    std::vector<cv::Point2f> previous_points;
    cv::goodFeaturesToTrack(
        previous_gray, previous_points, std::max(1, options.max_features),
        std::clamp(
            static_cast<double>(options.quality_permille) / 1000.0, 0.001, 1.0
        ),
        static_cast<double>(std::max(1, options.min_feature_distance_px)),
        feature_mask.empty() ? cv::Mat {} : feature_mask
    );
    if (previous_points.empty()) {
        return result;
    }

    std::vector<cv::Point2f> current_points;
    std::vector<std::uint8_t> status;
    std::vector<float> errors;
    cv::calcOpticalFlowPyrLK(
        previous_gray, current_gray, previous_points, current_points, status,
        errors, tracking_window_size(options.window_size_px),
        std::max(0, options.pyramid_levels)
    );

    const double min_distance2
        = static_cast<double>(std::max(0, options.min_vector_length_px))
        * static_cast<double>(std::max(0, options.min_vector_length_px));
    const float max_error = static_cast<float>(std::max(0, options.max_error));

    result.vectors.reserve(current_points.size());
    for (size_t index = 0; index < current_points.size(); ++index) {
        if (index >= status.size() || status[index] == 0) {
            continue;
        }
        if (index < errors.size() && max_error > 0.0f
            && errors[index] > max_error) {
            continue;
        }

        const cv::Point2f delta
            = current_points[index] - previous_points[index];
        const double distance2 = static_cast<double>(delta.x) * delta.x
            + static_cast<double>(delta.y) * delta.y;
        if (distance2 < min_distance2) {
            continue;
        }

        const point from_pct = pixel_to_pct(
            cv::Point(
                static_cast<int>(std::lround(previous_points[index].x)),
                static_cast<int>(std::lround(previous_points[index].y))
            ),
            previous_gray.size()
        );
        const point to_pct = pixel_to_pct(
            cv::Point(
                static_cast<int>(std::lround(current_points[index].x)),
                static_cast<int>(std::lround(current_points[index].y))
            ),
            current_gray.size()
        );
        result.vectors.push_back(
            processing_sparse_flow_vector {
                .from_pct = from_pct,
                .to_pct = to_pct,
                .distance_pct = distance_pct(from_pct, to_pct),
                .error = index < errors.size() ? errors[index] : 0.0f,
            }
        );
    }

    if (result.vectors.empty()) {
        return result;
    }

    std::sort(
        result.vectors.begin(), result.vectors.end(),
        [](const processing_sparse_flow_vector& lhs,
           const processing_sparse_flow_vector& rhs) {
            return lhs.distance_pct > rhs.distance_pct;
        }
    );

    point average_from;
    point average_to;
    double total_distance = 0.0;
    for (const auto& vector : result.vectors) {
        average_from.x += vector.from_pct.x;
        average_from.y += vector.from_pct.y;
        average_to.x += vector.to_pct.x;
        average_to.y += vector.to_pct.y;
        total_distance += vector.distance_pct;
    }

    const auto count = static_cast<float>(result.vectors.size());
    average_from.x /= count;
    average_from.y /= count;
    average_to.x /= count;
    average_to.y /= count;
    result.average_from_pct = average_from;
    result.average_to_pct = average_to;
    result.average_distance_pct
        = total_distance / static_cast<double>(result.vectors.size());
    return result;
}

} // namespace yodau::core

#endif
