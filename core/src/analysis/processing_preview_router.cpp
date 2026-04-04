#include "analysis/processing_preview_router.hpp"

#include "streams/virtual_camera.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

#ifdef YODAU_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace yodau::core {

namespace processing_preview_router_support {

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
    const cv::Mat& bgr, const std::chrono::steady_clock::time_point timestamp
) {
    frame out;
    if (bgr.empty()) {
        return out;
    }

    out.width = bgr.cols;
    out.height = bgr.rows;
    out.stride = static_cast<int>(bgr.step);
    out.format = pixel_format::bgr24;
    out.ts = timestamp;
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
        const auto profile = stream_value.find_line_profile(line_ptr_value->name);
        const int thickness = std::clamp(
            static_cast<int>(std::lround(
                profile.has_value() ? profile->visual_width : 2.0f
            )),
            1, 12
        );
        cv::polylines(image, polyline, closed, color, thickness, cv::LINE_AA);

        const cv::Point label_anchor
            = polyline.front() + cv::Point(6, -6 - thickness);
        cv::putText(
            image, line_ptr_value->name, label_anchor, cv::FONT_HERSHEY_SIMPLEX,
            0.45, color, 1, cv::LINE_AA
        );
    }
}

cv::Scalar event_color(const event_kind kind) {
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

void draw_label(
    cv::Mat& image, const std::string& label, const cv::Point& origin,
    const cv::Scalar& color
) {
    if (label.empty()) {
        return;
    }

    cv::putText(
        image, label, origin, cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1,
        cv::LINE_AA
    );
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
            draw_label(
                image, event_value.line_name, center + cv::Point(8, -8), color
            );
        }
    }
}

void draw_processing_overlays(
    cv::Mat& image, const processing_result& result
) {
    for (const auto& overlay : result.overlays) {
        const std::string color_key = overlay.label.empty()
            ? std::string("overlay")
            : overlay.label;
        const cv::Scalar color = color_from_text(color_key);

        switch (overlay.kind) {
        case processing_overlay_kind::point:
            if (overlay.anchor_pct.has_value()) {
                const cv::Point anchor
                    = pct_to_pixel(*overlay.anchor_pct, image);
                cv::circle(image, anchor, 6, color, 2, cv::LINE_AA);
                draw_label(image, overlay.label, anchor + cv::Point(8, -8), color);
            }
            break;
        case processing_overlay_kind::polyline:
        case processing_overlay_kind::polygon:
            if (overlay.points_pct.size() >= 2) {
                std::vector<cv::Point> points;
                points.reserve(overlay.points_pct.size());
                for (const auto& point_value : overlay.points_pct) {
                    points.push_back(pct_to_pixel(point_value, image));
                }

                const bool closed
                    = overlay.kind == processing_overlay_kind::polygon
                    && points.size() >= 3;
                cv::polylines(image, points, closed, color, 2, cv::LINE_AA);

                const cv::Point label_anchor = overlay.anchor_pct.has_value()
                    ? pct_to_pixel(*overlay.anchor_pct, image)
                    : points.front();
                draw_label(
                    image, overlay.label, label_anchor + cv::Point(8, -8), color
                );
            }
            break;
        case processing_overlay_kind::label:
            if (overlay.anchor_pct.has_value()) {
                const cv::Point anchor
                    = pct_to_pixel(*overlay.anchor_pct, image);
                draw_label(image, overlay.label, anchor, color);
            }
            break;
        }
    }
}

frame render_overlay_frame(
    const stream& stream_value, const frame& source_frame,
    const std::vector<event>& events, const processing_result* latest_result
) {
    cv::Mat image = frame_to_bgr_mat(source_frame);
    if (image.empty()) {
        return source_frame;
    }

    draw_line_overlays(image, stream_value);
    if (latest_result != nullptr) {
        draw_processing_overlays(image, *latest_result);
    }
    draw_event_overlays(image, events);
    return bgr_mat_to_frame(image, source_frame.ts);
}
#endif

} // namespace processing_preview_router_support

processing_preview_router::processing_preview_router(
    const bool enable_virtual_camera
) {
    if (enable_virtual_camera) {
        preview_camera_value = std::make_unique<virtual_camera>();
    }
}

processing_preview_router::~processing_preview_router() = default;

processing_preview_router::processing_preview_router(
    processing_preview_router&&
) noexcept = default;

processing_preview_router&
processing_preview_router::operator=(processing_preview_router&&) noexcept
    = default;

bool processing_preview_router::has_virtual_camera() const {
    return preview_camera_value != nullptr;
}

virtual_camera* processing_preview_router::preview_camera() {
    return preview_camera_value.get();
}

const virtual_camera* processing_preview_router::preview_camera() const {
    return preview_camera_value.get();
}

void processing_preview_router::publish_processed_frame(
    const stream& stream_value, const frame& source_frame,
    const std::vector<event>& events, const processing_result* latest_result
) {
    if (preview_camera_value == nullptr) {
        return;
    }

    if (source_frame.data.empty() || source_frame.width <= 0
        || source_frame.height <= 0) {
        return;
    }

#ifdef YODAU_OPENCV
    const frame rendered = processing_preview_router_support::render_overlay_frame(
        stream_value, source_frame, events, latest_result
    );
    preview_camera_value->publish(stream_value.get_name(), rendered);
#else
    (void)events;
    (void)latest_result;
    preview_camera_value->publish(stream_value.get_name(), source_frame);
#endif
}

} // namespace yodau::core
