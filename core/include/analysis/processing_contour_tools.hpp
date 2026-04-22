#ifndef YODAU_CORE_ANALYSIS_PROCESSING_CONTOUR_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_CONTOUR_TOOLS_HPP

#ifdef YODAU_OPENCV

#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_candidate_source.hpp"
#include "core/namespace_alias.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace yodau::core {

struct processing_contour_candidate {
    std::vector<cv::Point> contour;
    point center_pct;
    double area_px2 { 0.0 };
};

point pixel_to_pct(const cv::Point& pixel, const cv::Size& size);

std::optional<point> contour_center_pct(
    const std::vector<cv::Point>& contour, const cv::Size& size
);

std::vector<point> contour_to_pct(
    const std::vector<cv::Point>& contour, const cv::Size& size, size_t limit
);

double distance_pct(const point& lhs, const point& rhs);

std::optional<size_t> largest_contour_index(
    const std::vector<std::vector<cv::Point>>& contours
);

std::vector<std::vector<cv::Point>> accepted_contours_by_area(
    const cv::Mat& binary_mask, double min_area_px2
);

std::vector<processing_contour_candidate> contour_candidates_by_area(
    const cv::Mat& binary_mask, const cv::Size& frame_size,
    double min_area_px2
);

std::vector<processing_candidate> processing_candidates_from_contours(
    const std::vector<processing_contour_candidate>& contour_candidates,
    const cv::Size& frame_size, size_t mask_point_limit,
    std::optional<std::string> class_id = std::nullopt
);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_CONTOUR_TOOLS_HPP
