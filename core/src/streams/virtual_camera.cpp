#include "streams/virtual_camera.hpp"

#include <algorithm>
#include <cstdlib>
#include <ranges>

#ifdef YODAU_OPENCV
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace yodau::backend {

namespace virtual_camera_support {

std::string pixel_format_name(const pixel_format format) {
    switch (format) {
    case pixel_format::gray8:
        return "gray8";
    case pixel_format::rgb24:
        return "rgb24";
    case pixel_format::bgr24:
        return "bgr24";
    case pixel_format::rgba32:
        return "rgba32";
    case pixel_format::bgra32:
        return "bgra32";
    }

    return "unknown";
}

#ifdef YODAU_OPENCV
bool has_live_preview_display() {
    const char* display_env = std::getenv("DISPLAY");
    if (display_env != nullptr && display_env[0] != '\0') {
        return true;
    }

    const char* wayland_env = std::getenv("WAYLAND_DISPLAY");
    return wayland_env != nullptr && wayland_env[0] != '\0';
}

cv::Mat frame_to_mat(const frame& frame_value) {
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
        cv::Mat bgr;
        cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }
    case pixel_format::rgb24: {
        cv::Mat rgb(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }
    case pixel_format::bgr24: {
        cv::Mat bgr(
            frame_value.height, frame_value.width, CV_8UC3, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        return bgr.clone();
    }
    case pixel_format::rgba32: {
        cv::Mat rgba(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat bgr;
        cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
        return bgr;
    }
    case pixel_format::bgra32: {
        cv::Mat bgra(
            frame_value.height, frame_value.width, CV_8UC4, bytes,
            static_cast<size_t>(frame_value.stride)
        );
        cv::Mat bgr;
        cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    }

    return {};
}
#endif

} // namespace virtual_camera_support

virtual_camera::virtual_camera(std::string virtual_camera_name)
    : camera_name(std::move(virtual_camera_name)) {}

void virtual_camera::publish(
    const std::string& stream_name, const frame& frame_value
) {
    {
        std::scoped_lock lock(mtx);
        latest_by_stream[stream_name] = frame_value;
        update_count_by_stream[stream_name] += 1;
    }

#ifdef YODAU_OPENCV
    if (!virtual_camera_support::has_live_preview_display()) {
        return;
    }

    cv::Mat preview = virtual_camera_support::frame_to_mat(frame_value);
    if (preview.empty()) {
        return;
    }

    const std::string window_title = camera_name + ":" + stream_name;
    cv::imshow(window_title, preview);
    cv::waitKey(1);
#endif
}

std::optional<frame>
virtual_camera::latest_frame(const std::string& stream_name) const {
    std::scoped_lock lock(mtx);

    const auto frame_it = latest_by_stream.find(stream_name);
    if (frame_it == latest_by_stream.end()) {
        return {};
    }

    return frame_it->second;
}

std::vector<virtual_camera_frame_info> virtual_camera::frames() const {
    std::vector<virtual_camera_frame_info> out;

    std::scoped_lock lock(mtx);
    out.reserve(latest_by_stream.size());

    for (const auto& [stream_name, frame_value] : latest_by_stream) {
        const auto update_it = update_count_by_stream.find(stream_name);

        virtual_camera_frame_info info;
        info.stream_name = stream_name;
        info.width = frame_value.width;
        info.height = frame_value.height;
        info.format = frame_value.format;
        info.bytes = frame_value.data.size();
        if (update_it != update_count_by_stream.end()) {
            info.update_count = update_it->second;
        }

        out.push_back(std::move(info));
    }

    std::ranges::sort(
        out, std::less<>{}, &virtual_camera_frame_info::stream_name
    );
    return out;
}

void virtual_camera::dump(std::ostream& out) const {
    const auto snapshots = frames();
    out << snapshots.size() << " virtual camera streams:";

    for (const auto& info : snapshots) {
        out << "\n\tVirtualCamera(stream=" << info.stream_name
            << ", frame=" << info.width << "x" << info.height
            << ", format="
            << virtual_camera_support::pixel_format_name(info.format)
            << ", bytes=" << info.bytes
            << ", updates=" << info.update_count << ")";
    }
}

}
