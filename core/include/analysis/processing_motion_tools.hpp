#ifndef YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP

#ifdef YODAU_OPENCV

#include "core/namespace_alias.hpp"
#include "geometry/coords.hpp"
#include "streams/event.hpp"

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::core {

int normalized_odd_kernel_size(int kernel_size);

cv::Mat blurred_absdiff(
    const cv::Mat& previous_gray, const cv::Mat& current_gray, int blur_kernel
);

cv::Mat binary_motion_mask(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    int diff_threshold, int blur_kernel, int morph_kernel
);

cv::Mat legacy_frame_delta_motion_mask(
    const cv::Mat& previous_gray, const cv::Mat& current_gray,
    int diff_threshold, int erode_iterations, int dilate_iterations
);

double motion_mask_ratio(const cv::Mat& binary_mask);

cv::Mat downsample_motion_mask_to_grid(
    const cv::Mat& binary_mask, const grid_dims& grid
);

std::vector<int>
active_motion_grid_cells(const cv::Mat& grid_mask, const grid_dims& grid);

double legacy_impact_speed(
    const point& previous_center, const point& current_center,
    double motion_ratio, double min_ratio
);

event make_motion_event(
    std::string stream_name, std::chrono::steady_clock::time_point timestamp,
    point position_pct
);

void append_motion_grid_cell_events(
    std::vector<event>& events, const std::string& stream_name,
    std::chrono::steady_clock::time_point timestamp,
    const std::vector<int>& active_cell_indices, const grid_dims& grid,
    int max_events
);

class processing_motion_event_state {
public:
    bool update_previous_gray(
        const std::string& stream_name, const cv::Mat& current_gray,
        cv::Mat& previous_gray
    );

    bool allow_motion_emit(
        const std::string& stream_name,
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds cooldown
    );

    bool update_motion_position(
        const std::string& stream_name, point current_position,
        point& previous_position
    );

    bool allow_tripwire_emit(
        const std::string& key, std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds cooldown
    );

private:
    std::mutex mtx_;
    std::unordered_map<std::string, cv::Mat> previous_gray_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        motion_emit_by_stream_;
    std::unordered_map<std::string, point> motion_position_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        tripwire_emit_by_key_;
};

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_MOTION_TOOLS_HPP
