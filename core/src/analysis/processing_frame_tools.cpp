#include "analysis/processing_frame_tools.hpp"

#ifdef YODAU_OPENCV

#include "streams/linux_capture_device.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace yodau::core {

namespace {

int local_capture_index_from_path(const std::string& path) {
    const std::string pref = "/dev/video";
    if (path.rfind(pref, 0) != 0) {
        return -1;
    }

    const auto tail = path.substr(pref.size());
    int idx = -1;

    const auto res
        = std::from_chars(tail.data(), tail.data() + tail.size(), idx);
    if (res.ec != std::errc() || res.ptr != tail.data() + tail.size()) {
        return -1;
    }

    return idx;
}

} // namespace

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

std::string config_string(
    const processing_algorithm_configuration& configuration,
    const std::string& key, std::string fallback
) {
    const auto it = configuration.values.find(key);
    return it == configuration.values.end() ? std::move(fallback) : it->second;
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

bool frame_to_bgr_and_gray_mat(
    const frame& frame_value, cv::Mat& bgr, cv::Mat& gray
) {
    if (frame_value.width <= 0 || frame_value.height <= 0
        || frame_value.stride <= 0 || frame_value.data.empty()) {
        return false;
    }

    auto* bytes = const_cast<std::uint8_t*>(frame_value.data.data());

    switch (frame_value.format) {
    case pixel_format::gray8: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC1, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        gray = input.clone();
        cv::cvtColor(input, bgr, cv::COLOR_GRAY2BGR);
        return true;
    }
    case pixel_format::rgb24: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_RGB2BGR);
        cv::cvtColor(input, gray, cv::COLOR_RGB2GRAY);
        return true;
    }
    case pixel_format::bgr24: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        bgr = input;
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        return true;
    }
    case pixel_format::rgba32: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_RGBA2BGR);
        cv::cvtColor(input, gray, cv::COLOR_RGBA2GRAY);
        return true;
    }
    case pixel_format::bgra32: {
        cv::Mat input(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::cvtColor(input, bgr, cv::COLOR_BGRA2BGR);
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
        return true;
    }
    }

    return false;
}

frame bgr_mat_to_frame(
    const cv::Mat& image, const std::chrono::steady_clock::time_point timestamp
) {
    cv::Mat bgr;
    if (image.channels() == 3 && image.type() == CV_8UC3) {
        bgr = image;
    } else if (image.channels() == 1) {
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
    } else {
        image.convertTo(bgr, CV_8UC3);
    }

    if (!bgr.isContinuous()) {
        bgr = bgr.clone();
    }

    frame out;
    out.width = bgr.cols;
    out.height = bgr.rows;
    out.stride = static_cast<int>(bgr.step);
    out.ts = timestamp;
    out.format = pixel_format::bgr24;
    out.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
    return out;
}

bool open_video_capture_for_stream(
    const stream& stream_value, cv::VideoCapture& capture
) {
    const auto path = stream_value.get_path();
    const int idx = local_capture_index_from_path(path);
    if (idx >= 0) {
#ifdef __linux__
        if (!is_linux_capture_device(path)) {
            return false;
        }
#endif
        capture.open(idx);
    } else {
        capture.open(path);
    }

    return capture.isOpened();
}

video_capture_read_status read_video_capture_frame(
    const stream& stream_value, cv::VideoCapture& capture, cv::Mat& image
) {
    if (capture.read(image) && !image.empty()) {
        return video_capture_read_status::frame_ready;
    }

    if (stream_value.is_looping()
        && stream_value.get_type() == stream_type::file) {
        capture.set(cv::CAP_PROP_POS_FRAMES, 0);
        return video_capture_read_status::rewound;
    }

    return video_capture_read_status::finished;
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
