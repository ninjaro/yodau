#include "analysis/processing_frame_tools.hpp"

#ifdef YODAU_OPENCV

#include "streams/linux_capture_device.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

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
    const std::string& key, const std::string& fallback
) {
    const auto it = configuration.values.find(key);
    return it == configuration.values.end() ? fallback : it->second;
}

cv::Mat frame_to_gray_mat(const frame& frame_value) {
    if (!validate_frame_layout(frame_value)) {
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

bool frame_to_bgr_gray_mats(
    const frame& frame_value, cv::Mat& bgr, cv::Mat& gray
) {
    bgr.release();
    gray.release();
    if (!validate_frame_layout(frame_value)) {
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
    if (image.empty()) {
        return {};
    }
    if (image.dims != 2) {
        throw std::invalid_argument(
            "bgr_mat_to_frame requires a two-dimensional image"
        );
    }
    if (image.depth() != CV_8U) {
        throw std::invalid_argument("bgr_mat_to_frame requires an 8-bit image");
    }

    cv::Mat bgr;
    if (image.channels() == 3) {
        bgr = image;
    } else if (image.channels() == 1) {
        cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
    } else {
        throw std::invalid_argument(
            "bgr_mat_to_frame requires a 1-, 3-, or 4-channel image"
        );
    }

    if (!bgr.isContinuous()) {
        bgr = bgr.clone();
    }
    if (bgr.step > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("BGR image stride does not fit in frame");
    }
    if (bgr.elemSize() == 0
        || bgr.total() > std::numeric_limits<size_t>::max() / bgr.elemSize()) {
        throw std::overflow_error("BGR image extent does not fit in frame");
    }
    const size_t data_size = bgr.total() * bgr.elemSize();

    frame out;
    out.width = bgr.cols;
    out.height = bgr.rows;
    out.stride = static_cast<int>(bgr.step);
    out.ts = timestamp;
    out.format = pixel_format::bgr24;
    out.data.assign(bgr.data, bgr.data + data_size);

    if (!validate_frame_layout(out)) {
        throw std::runtime_error("OpenCV produced an invalid BGR frame layout");
    }
    return out;
}

frame scaled_frame_to_max_pixels(
    const frame& frame_value, const int max_pixels
) {
    if (!validate_frame_layout(frame_value) || max_pixels <= 0) {
        return {};
    }

    const auto source_pixels = static_cast<std::int64_t>(frame_value.width)
        * static_cast<std::int64_t>(frame_value.height);
    if (source_pixels <= static_cast<std::int64_t>(max_pixels)) {
        return frame_value;
    }

    cv::Mat bgr;
    cv::Mat gray;
    if (!frame_to_bgr_gray_mats(frame_value, bgr, gray) || bgr.empty()) {
        return {};
    }

    const double scale = std::sqrt(
        static_cast<double>(max_pixels) / static_cast<double>(source_pixels)
    );
    int width = std::max(
        1,
        static_cast<int>(
            std::floor(static_cast<double>(frame_value.width) * scale)
        )
    );
    int height = std::max(
        1,
        static_cast<int>(
            std::floor(static_cast<double>(frame_value.height) * scale)
        )
    );
    while (static_cast<std::int64_t>(width) * static_cast<std::int64_t>(height)
           > static_cast<std::int64_t>(max_pixels)) {
        if (width >= height && width > 1) {
            --width;
        } else if (height > 1) {
            --height;
        } else {
            break;
        }
    }

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(width, height), 0.0, 0.0, cv::INTER_AREA);
    return bgr_mat_to_frame(resized, frame_value.ts);
}

bool open_video_capture_for_stream(
    const stream& stream_value, cv::VideoCapture& capture
) {
    const auto path = stream_value.get_path();
#ifdef __linux__
    if (stream_value.get_type() == stream_type::local) {
        if (!is_linux_capture_device(path)) {
            return false;
        }
        capture.open(path, cv::CAP_V4L2);
        return capture.isOpened();
    }
#endif

    if (stream_value.get_type() == stream_type::rtsp
        || stream_value.get_type() == stream_type::http) {
        constexpr int open_timeout_ms = 5000;
        constexpr int read_timeout_ms = 1000;
        capture.open(
            path, cv::CAP_ANY,
            {
                cv::CAP_PROP_OPEN_TIMEOUT_MSEC,
                open_timeout_ms,
                cv::CAP_PROP_READ_TIMEOUT_MSEC,
                read_timeout_ms,
            }
        );
        return capture.isOpened();
    }

    const int idx = local_capture_index_from_path(path);
    if (idx >= 0) {
        capture.open(idx);
    } else {
        capture.open(path);
    }

    return capture.isOpened();
}

video_capture_read_status read_video_capture_frame(
    const stream& stream_value, cv::VideoCapture& capture, cv::Mat& image,
    const stop_token& stop_token
) {
    if (!capture.isOpened()) {
        return video_capture_read_status::finished;
    }

#ifdef __linux__
    if (stream_value.get_type() == stream_type::local) {
        if (stop_token.stop_requested()) {
            return video_capture_read_status::finished;
        }
        std::vector<cv::VideoCapture> captures { capture };
        std::vector<int> ready;
        constexpr std::int64_t wait_timeout_ns = 100'000'000;
        if (!cv::VideoCapture::waitAny(captures, ready, wait_timeout_ns)) {
            return video_capture_read_status::wait_timeout;
        }
        if (ready.empty() || stop_token.stop_requested()
            || !captures.front().retrieve(image) || image.empty()) {
            return video_capture_read_status::finished;
        }
        return video_capture_read_status::frame_ready;
    }
#endif

    if (capture.read(image) && !image.empty()) {
        return video_capture_read_status::frame_ready;
    }

    if (stream_value.is_looping()
        && stream_value.get_type() == stream_type::file) {
        if (!capture.set(cv::CAP_PROP_POS_FRAMES, 0)) {
            return video_capture_read_status::finished;
        }

        // A looping source gets one bounded rewind attempt. Reading the first
        // frame here distinguishes a usable rewind from an empty, corrupt, or
        // unseekable source and prevents the caller from busy-spinning.
        if (capture.read(image) && !image.empty()) {
            return video_capture_read_status::frame_ready;
        }
    }

    return video_capture_read_status::finished;
}

point grid_cell_center_pct(
    const int col, const int row, const int cols, const int rows
) {
    return point {
        .x = static_cast<float>(
            (static_cast<double>(col) + 0.5) * 100.0 / static_cast<double>(cols)
        ),
        .y = static_cast<float>(
            (static_cast<double>(row) + 0.5) * 100.0 / static_cast<double>(rows)
        ),
    };
}

} // namespace yodau::core

#endif
