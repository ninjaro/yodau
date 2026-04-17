#include "analysis/processing_frame_tools.hpp"

#ifdef YODAU_OPENCV

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>

namespace yodau::core {

int config_int(
    const processing_algorithm_configuration& configuration,
    const std::string& key, const int fallback, const int min_value,
    const int max_value
) {
    const auto it = configuration.values.find(key);
    if (it == configuration.values.end()) {
        return fallback;
    }

    int value = fallback;
    const auto* begin = it->second.data();
    const auto* end = it->second.data() + it->second.size();
    const auto [ptr, error] = std::from_chars(begin, end, value);
    if (error != std::errc() || ptr != end) {
        return fallback;
    }

    return std::clamp(value, min_value, max_value);
}

cv::Mat frame_to_gray_mat(const frame& frame_value) {
    if (frame_value.width <= 0 || frame_value.height <= 0
        || frame_value.stride <= 0 || frame_value.data.empty()) {
        return {};
    }

    auto* bytes = const_cast<std::uint8_t*>(frame_value.data.data());

    switch (frame_value.format) {
    case pixel_format::gray8: {
        cv::Mat gray(
            frame_value.height, frame_value.width, CV_8UC1, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        return gray.clone();
    }
    case pixel_format::rgb24: {
        cv::Mat rgb(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat gray;
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        return gray;
    }
    case pixel_format::bgr24: {
        cv::Mat bgr(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat gray;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        return gray;
    }
    case pixel_format::rgba32: {
        cv::Mat rgba(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat gray;
        cv::cvtColor(rgba, gray, cv::COLOR_RGBA2GRAY);
        return gray;
    }
    case pixel_format::bgra32: {
        cv::Mat bgra(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat gray;
        cv::cvtColor(bgra, gray, cv::COLOR_BGRA2GRAY);
        return gray;
    }
    }

    return {};
}

point grid_cell_center_pct(
    const int col, const int row, const int cols, const int rows
) {
    return point {
        .x = static_cast<float>((static_cast<double>(col) + 0.5) * 100.0
                                / static_cast<double>(cols)),
        .y = static_cast<float>((static_cast<double>(row) + 0.5) * 100.0
                                / static_cast<double>(rows)),
    };
}

} // namespace yodau::core

#endif
