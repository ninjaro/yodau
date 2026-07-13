#ifndef YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP
#define YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP

#ifdef YODAU_OPENCV

#include "analysis/processing_algorithm.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"

#include <opencv2/core/mat.hpp>

#include <chrono>
#include <stop_token>
#include <string>

namespace cv {
class VideoCapture;
}

namespace yodau::core {

enum class video_capture_read_status { frame_ready, wait_timeout, finished };

int config_int(
    const processing_algorithm_configuration& configuration,
    const std::string& key, int fallback, int min_value, int max_value
);

std::string config_string(
    const processing_algorithm_configuration& configuration,
    const std::string& key, const std::string& fallback
);

cv::Mat frame_to_gray_mat(const frame& frame_value);

bool frame_to_bgr_gray_mats(
    const frame& frame_value, cv::Mat& bgr, cv::Mat& gray
);

// Accepts two-dimensional CV_8UC1 (gray), CV_8UC3 (BGR), or CV_8UC4
// (BGRA). Empty input produces an empty frame; other layouts are rejected.
frame bgr_mat_to_frame(
    const cv::Mat& image,
    std::chrono::steady_clock::time_point timestamp
    = std::chrono::steady_clock::now()
);

// Returns a BGR frame scaled down to the requested pixel budget while
// preserving aspect ratio. Frames already within the budget are copied
// unchanged; malformed frames are rejected with an empty result.
frame scaled_frame_to_max_pixels(const frame& frame_value, int max_pixels);

bool open_video_capture_for_stream(
    const stream& stream_value, cv::VideoCapture& capture
);

video_capture_read_status read_video_capture_frame(
    const stream& stream_value, cv::VideoCapture& capture, cv::Mat& image,
    const std::stop_token& stop_token = {}
);

point grid_cell_center_pct(int col, int row, int cols, int rows);

} // namespace yodau::core

#endif

#endif // YODAU_CORE_ANALYSIS_PROCESSING_FRAME_TOOLS_HPP
