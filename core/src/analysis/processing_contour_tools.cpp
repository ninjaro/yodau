#include "analysis/processing_contour_tools.hpp"

#ifdef YODAU_OPENCV

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace yodau::core {

point pixel_to_pct(const cv::Point& pixel, const cv::Size& size) {
    const double max_x = std::max(1, size.width - 1);
    const double max_y = std::max(1, size.height - 1);
    return point {
        .x = static_cast<float>(
            static_cast<double>(pixel.x) * 100.0 / static_cast<double>(max_x)
        ),
        .y = static_cast<float>(
            static_cast<double>(pixel.y) * 100.0 / static_cast<double>(max_y)
        ),
    };
}

std::optional<point> contour_center_pct(
    const std::vector<cv::Point>& contour, const cv::Size& size
) {
    if (contour.empty()) {
        return std::nullopt;
    }

    const cv::Moments moments = cv::moments(contour);
    if (std::abs(moments.m00) > 0.001) {
        return pixel_to_pct(
            cv::Point(
                static_cast<int>(moments.m10 / moments.m00),
                static_cast<int>(moments.m01 / moments.m00)
            ),
            size
        );
    }

    const cv::Rect bounds = cv::boundingRect(contour);
    return pixel_to_pct(
        cv::Point(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2),
        size
    );
}

std::vector<point> contour_to_pct(
    const std::vector<cv::Point>& contour, const cv::Size& size,
    const size_t limit
) {
    std::vector<point> points_pct;
    if (contour.empty()) {
        return points_pct;
    }

    const size_t step = std::max<size_t>(
        1, contour.size() / std::max<size_t>(1, limit)
    );
    points_pct.reserve(std::min(contour.size(), limit + 1));
    for (size_t index = 0; index < contour.size(); index += step) {
        points_pct.push_back(pixel_to_pct(contour[index], size));
        if (points_pct.size() >= limit) {
            break;
        }
    }

    if (points_pct.size() < 3 && contour.size() >= 3) {
        points_pct.clear();
        for (size_t index = 0; index < 3; ++index) {
            points_pct.push_back(pixel_to_pct(contour[index], size));
        }
    }

    return points_pct;
}

double distance_pct(const point& lhs, const point& rhs) {
    return std::hypot(
        static_cast<double>(rhs.x - lhs.x),
        static_cast<double>(rhs.y - lhs.y)
    );
}

std::vector<std::vector<cv::Point>> accepted_contours_by_area(
    const cv::Mat& binary_mask, const double min_area_px2
) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE
    );

    std::vector<std::vector<cv::Point>> accepted_contours;
    accepted_contours.reserve(contours.size());

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) >= min_area_px2) {
            accepted_contours.push_back(contour);
        }
    }

    std::sort(
        accepted_contours.begin(), accepted_contours.end(),
        [](const auto& lhs, const auto& rhs) {
            return cv::contourArea(lhs) > cv::contourArea(rhs);
        }
    );

    return accepted_contours;
}

std::vector<processing_contour_candidate> contour_candidates_by_area(
    const cv::Mat& binary_mask, const cv::Size& frame_size,
    const double min_area_px2
) {
    const auto accepted_contours = accepted_contours_by_area(
        binary_mask, min_area_px2
    );

    std::vector<processing_contour_candidate> candidates;
    candidates.reserve(accepted_contours.size());

    for (const auto& contour : accepted_contours) {
        const auto center_pct = contour_center_pct(contour, frame_size);
        if (!center_pct.has_value()) {
            continue;
        }

        candidates.push_back(
            processing_contour_candidate {
                .contour = contour,
                .center_pct = *center_pct,
                .area_px2 = cv::contourArea(contour),
            }
        );
    }

    return candidates;
}

} // namespace yodau::core

#endif
