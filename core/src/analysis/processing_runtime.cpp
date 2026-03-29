#include "analysis/processing_runtime.hpp"

#include "analysis/opencv_client.hpp"
#include "streams/virtual_camera.hpp"

#include <cmath>
#include <functional>

#ifdef YODAU_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace yodau::backend {

namespace processing_runtime_support {

#ifdef YODAU_OPENCV
    cv::Mat frame_to_bgr_mat(const frame& frame_value) {
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

    frame bgr_mat_to_frame(
        const cv::Mat& bgr, const std::chrono::steady_clock::time_point ts
    ) {
        frame out;
        if (bgr.empty()) {
            return out;
        }

        out.width = bgr.cols;
        out.height = bgr.rows;
        out.stride = static_cast<int>(bgr.step);
        out.format = pixel_format::bgr24;
        out.ts = ts;
        out.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
        return out;
    }

    cv::Point pct_to_pixel(const point& pos_pct, const cv::Mat& image) {
        const double max_x = std::max(0, image.cols - 1);
        const double max_y = std::max(0, image.rows - 1);
        const int x = static_cast<int>(std::lround(max_x * pos_pct.x / 100.0f));
        const int y = static_cast<int>(std::lround(max_y * pos_pct.y / 100.0f));
        return cv::Point(x, y);
    }

    cv::Scalar color_from_text(const std::string& text) {
        const size_t hash_value = std::hash<std::string> {}(text);
        const int blue = 64 + static_cast<int>(hash_value & 0x7f);
        const int green = 64 + static_cast<int>((hash_value >> 7) & 0x7f);
        const int red = 64 + static_cast<int>((hash_value >> 14) & 0x7f);
        return cv::Scalar(blue, green, red);
    }

    void draw_line_overlays(cv::Mat& image, const stream& stream_value) {
        const auto lines = stream_value.lines_snapshot();

        for (const auto& line_ptr_value : lines) {
            if (!line_ptr_value || line_ptr_value->points.size() < 2) {
                continue;
            }

            std::vector<cv::Point> polyline;
            polyline.reserve(line_ptr_value->points.size());
            for (const auto& point_value : line_ptr_value->points) {
                polyline.push_back(pct_to_pixel(point_value, image));
            }

            const cv::Scalar color = color_from_text(line_ptr_value->name);
            const bool closed = line_ptr_value->closed && polyline.size() > 2;
            cv::polylines(image, polyline, closed, color, 2, cv::LINE_AA);

            const cv::Point label_anchor = polyline.front() + cv::Point(6, -6);
            cv::putText(
                image, line_ptr_value->name, label_anchor,
                cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv::LINE_AA
            );
        }
    }

    cv::Scalar event_color(event_kind kind) {
        switch (kind) {
        case event_kind::motion:
            return cv::Scalar(48, 168, 255);
        case event_kind::tripwire:
            return cv::Scalar(48, 48, 255);
        case event_kind::roi:
            return cv::Scalar(64, 220, 64);
        case event_kind::info:
        default:
            return cv::Scalar(220, 220, 220);
        }
    }

    void draw_event_overlays(cv::Mat& image, const std::vector<event>& events) {
        for (const auto& event_value : events) {
            if (!event_value.pos_pct.has_value()) {
                continue;
            }

            const cv::Point center = pct_to_pixel(*event_value.pos_pct, image);
            const cv::Scalar color = event_color(event_value.kind);
            const int radius = event_value.kind == event_kind::tripwire ? 7 : 5;

            cv::circle(image, center, radius, color, 2, cv::LINE_AA);

            if (event_value.kind == event_kind::tripwire
                && !event_value.line_name.empty()) {
                const cv::Point text_origin = center + cv::Point(8, -8);
                cv::putText(
                    image, event_value.line_name, text_origin,
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv::LINE_AA
                );
            }
        }
    }

    frame render_overlay_frame(
        const stream& stream_value, const frame& source_frame,
        const std::vector<event>& events
    ) {
        cv::Mat image = frame_to_bgr_mat(source_frame);
        if (image.empty()) {
            return source_frame;
        }

        draw_line_overlays(image, stream_value);
        draw_event_overlays(image, events);
        return bgr_mat_to_frame(image, source_frame.ts);
    }
#endif

} // namespace processing_runtime_support

std::string render_mode_name(const render_mode mode) {
    switch (mode) {
    case render_mode::backend_only:
        return "backend_only";
    case render_mode::frontend_only:
        return "frontend_only";
    }

    return "frontend_only";
}

processing_runtime::processing_runtime(
    processing_runtime_options runtime_options_value
)
    : runtime_options(runtime_options_value) {
    if (runtime_options.mode == render_mode::backend_only
        && runtime_options.enable_virtual_camera) {
        preview_camera_value = std::make_unique<virtual_camera>();
    }
}

processing_runtime::~processing_runtime() = default;

processing_runtime::processing_runtime(processing_runtime&& other) noexcept
    : runtime_options(other.runtime_options)
    , preview_camera_value(std::move(other.preview_camera_value)) { }

processing_runtime&
processing_runtime::operator=(processing_runtime&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    runtime_options = other.runtime_options;
    preview_camera_value = std::move(other.preview_camera_value);
    return *this;
}

void processing_runtime::attach(stream_manager& mgr) {
    mgr.set_frame_processor(frame_processor_hook());

    if (runtime_options.mode == render_mode::backend_only) {
        mgr.set_daemon_start_hook(daemon_start_hook());
        mgr.set_processed_frame_sink(processed_frame_sink());
        return;
    }

    mgr.set_daemon_start_hook({});
    mgr.set_processed_frame_sink({});
}

stream_manager::daemon_start_fn processing_runtime::daemon_start_hook() {
    if (runtime_options.mode != render_mode::backend_only) {
        return {};
    }

#ifdef YODAU_OPENCV
    return opencv_client::shared_instance().daemon_start_fn();
#else
    return {};
#endif
}

stream_manager::frame_processor_fn processing_runtime::frame_processor_hook() {
#ifdef YODAU_OPENCV
    return std::bind_front(&processing_runtime::process_frame, this);
#else
    return {};
#endif
}

stream_manager::processed_frame_sink_fn
processing_runtime::processed_frame_sink() {
    if (runtime_options.mode != render_mode::backend_only) {
        return {};
    }

    return std::bind_front(&processing_runtime::handle_processed_frame, this);
}

render_mode processing_runtime::mode() const { return runtime_options.mode; }

bool processing_runtime::processing_enabled() const {
#ifdef YODAU_OPENCV
    return true;
#else
    return false;
#endif
}

bool processing_runtime::has_virtual_camera() const {
    return preview_camera_value != nullptr;
}

virtual_camera* processing_runtime::preview_camera() {
    return preview_camera_value.get();
}

const virtual_camera* processing_runtime::preview_camera() const {
    return preview_camera_value.get();
}

std::vector<event>
processing_runtime::process_frame(const stream& s, const frame& f) {
#ifdef YODAU_OPENCV
    return opencv_client::shared_instance().motion_processor(s, f);
#else
    (void)s;
    (void)f;
    return {};
#endif
}

void processing_runtime::handle_processed_frame(
    const stream& s, const frame& frame_value, const std::vector<event>& events
) {
    if (runtime_options.mode != render_mode::backend_only
        || preview_camera_value == nullptr) {
        return;
    }

    if (frame_value.data.empty() || frame_value.width <= 0
        || frame_value.height <= 0) {
        return;
    }

#ifdef YODAU_OPENCV
    const frame rendered = processing_runtime_support::render_overlay_frame(
        s, frame_value, events
    );
    preview_camera_value->publish(s.get_name(), rendered);
#else
    (void)events;
    preview_camera_value->publish(s.get_name(), frame_value);
#endif
}

}
