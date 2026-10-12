#ifndef YODAU_CORE_TESTS_STREAMS_STREAM_MANAGER_FRAMES_HPP
#define YODAU_CORE_TESTS_STREAMS_STREAM_MANAGER_FRAMES_HPP

#include "analysis/processing_algorithm.hpp"
#include "streams/frame.hpp"

#include <chrono>
#include <limits>
#include <string>

namespace stream_manager_tests_support {

inline std::string diagnostic_value(
    const yodau::core::processing_result& result, const std::string& key
) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.key == key) {
            return diagnostic.value;
        }
    }

    return {};
}

inline double metric_value(
    const yodau::core::processing_result& result, const std::string& name,
    const double fallback = std::numeric_limits<double>::quiet_NaN()
) {
    for (const auto& metric : result.metrics) {
        if (metric.name == name) {
            return metric.value;
        }
    }

    return fallback;
}

inline yodau::core::frame make_gray_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::core::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width;
    frame_value.format = yodau::core::pixel_format::gray8;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height),
        static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            frame_value.data
                [static_cast<size_t>(y) * static_cast<size_t>(width)
                 + static_cast<size_t>(x)] = 255;
        }
    }

    return frame_value;
}

inline yodau::core::frame make_rgb_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::core::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width * 3;
    frame_value.format = yodau::core::pixel_format::rgb24;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(frame_value.stride) * static_cast<size_t>(height),
        static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            const auto pixel_offset = static_cast<size_t>(y)
                    * static_cast<size_t>(frame_value.stride)
                + static_cast<size_t>(x) * 3U;
            frame_value.data[pixel_offset + 0] = 255;
            frame_value.data[pixel_offset + 1] = 255;
            frame_value.data[pixel_offset + 2] = 255;
        }
    }

    return frame_value;
}

} // namespace stream_manager_tests_support

#endif
