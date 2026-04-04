#include "analysis/default_processing_hooks.hpp"

#include "analysis/opencv_client.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

namespace yodau::core {

namespace default_processing_hooks_support {

constexpr auto motion_baseline_algorithm_id = "motion_baseline";
constexpr auto spot_grid_algorithm_id = "spot_grid";
constexpr auto contour_mask_algorithm_id = "contour_mask";
constexpr auto hybrid_auto_algorithm_id = "hybrid_auto";

std::string normalized_requested_algorithm_id(const std::string& algorithm_id) {
    const std::string normalized
        = processing_algorithm_registry::normalized_algorithm_id(algorithm_id);

    if (normalized.empty() || normalized == "default"
        || normalized == "baseline" || normalized == "motion") {
        return std::string(motion_baseline_algorithm_id);
    }
    if (normalized == "spot" || normalized == "spots") {
        return std::string(spot_grid_algorithm_id);
    }
    if (normalized == "contour" || normalized == "contours"
        || normalized == "mask") {
        return std::string(contour_mask_algorithm_id);
    }
    if (normalized == "hybrid" || normalized == "auto"
        || normalized == "adaptive") {
        return std::string(hybrid_auto_algorithm_id);
    }

    return normalized;
}

#ifdef YODAU_OPENCV
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
    const auto [ptr, error]
        = std::from_chars(begin, end, value);
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

class motion_baseline_algorithm final : public processing_algorithm {
public:
    motion_baseline_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return motion_baseline_algorithm_id;
    }

    std::string display_name() const override {
        return "motion baseline";
    }

    processing_algorithm_configuration default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace(
            "pipeline_family", "motion_tripwire_baseline"
        );
        configuration.values.emplace("overlay_contract", "event_only");
        return configuration;
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        if (configuration.values.empty()) {
            configuration_ = default_configuration();
            return;
        }

        if (!configuration.values.contains("pipeline_family")) {
            configuration.values.emplace(
                "pipeline_family", "motion_tripwire_baseline"
            );
        }
        if (!configuration.values.contains("overlay_contract")) {
            configuration.values.emplace("overlay_contract", "event_only");
        }
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        client_.daemon_start(stream_value, on_frame, stop_token);
    }

    processing_result process_frame(
        const stream& stream_value, const frame& frame_value
    ) override {
        processing_result result;
        result.events = client_.motion_processor(stream_value, frame_value);
        result.metrics.push_back(
            processing_metric {
                .name = "event_count",
                .value = static_cast<double>(result.events.size()),
                .unit = "events",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "line_count",
                .value = static_cast<double>(
                    stream_value.lines_snapshot().size()
                ),
                .unit = "lines",
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic { .key = "algorithm", .value = algorithm_id() }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "display_name",
                .value = display_name(),
            }
        );
        return result;
    }

private:
    opencv_client client_;
    processing_algorithm_configuration configuration_;
};

class spot_grid_algorithm final : public processing_algorithm {
public:
    spot_grid_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return spot_grid_algorithm_id;
    }

    std::string display_name() const override { return "spot grid"; }

    processing_algorithm_configuration default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("grid_cols", "12");
        configuration.values.emplace("grid_rows", "12");
        configuration.values.emplace("diff_threshold", "26");
        configuration.values.emplace("min_cell_energy", "30");
        configuration.values.emplace("blur_kernel", "5");
        configuration.values.emplace("emit_interval_ms", "120");
        return configuration;
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        const auto defaults = default_configuration();
        for (const auto& [key, value] : defaults.values) {
            if (!configuration.values.contains(key)) {
                configuration.values.emplace(key, value);
            }
        }
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        daemon_client_.daemon_start(stream_value, on_frame, stop_token);
    }

    processing_result process_frame(
        const stream& stream_value, const frame& frame_value
    ) override {
        processing_result result;
        result.diagnostics.push_back(
            processing_diagnostic { .key = "algorithm", .value = algorithm_id() }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "display_name",
                .value = display_name(),
            }
        );

        const cv::Mat gray = frame_to_gray_mat(frame_value);
        if (gray.empty()) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "frame_state", .value = "invalid" }
            );
            return result;
        }

        const int grid_cols
            = config_int(configuration_, "grid_cols", 12, 2, 64);
        const int grid_rows
            = config_int(configuration_, "grid_rows", 12, 2, 64);
        const int diff_threshold
            = config_int(configuration_, "diff_threshold", 26, 1, 255);
        const int min_cell_energy
            = config_int(configuration_, "min_cell_energy", 30, 1, 255);
        int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        const int emit_interval_ms
            = config_int(configuration_, "emit_interval_ms", 120, 0, 5000);

        if (blur_kernel % 2 == 0) {
            blur_kernel += 1;
        }

        cv::Mat previous_gray;
        std::chrono::steady_clock::time_point last_emit {};
        bool has_previous = false;
        {
            std::scoped_lock lock(mtx_);
            const auto previous_it = prev_gray_by_stream_.find(
                stream_value.get_name()
            );
            if (previous_it != prev_gray_by_stream_.end()
                && !previous_it->second.empty()
                && previous_it->second.size() == gray.size()) {
                previous_gray = previous_it->second.clone();
                has_previous = true;
            }

            prev_gray_by_stream_[stream_value.get_name()] = gray.clone();

            const auto emit_it = last_emit_by_stream_.find(stream_value.get_name());
            if (emit_it != last_emit_by_stream_.end()) {
                last_emit = emit_it->second;
            }
        }

        result.metrics.push_back(
            processing_metric {
                .name = "grid_cell_count",
                .value = static_cast<double>(grid_cols * grid_rows),
                .unit = "cells",
            }
        );

        if (!has_previous) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "frame_state", .value = "warmup" }
            );
            return result;
        }

        cv::Mat diff;
        cv::absdiff(previous_gray, gray, diff);
        if (blur_kernel > 1) {
            cv::GaussianBlur(
                diff, diff, cv::Size(blur_kernel, blur_kernel), 0.0
            );
        }

        cv::Mat grid_energy;
        cv::resize(
            diff, grid_energy, cv::Size(grid_cols, grid_rows), 0.0, 0.0,
            cv::INTER_AREA
        );

        int hot_cell_count = 0;
        int best_col = -1;
        int best_row = -1;
        int max_energy = 0;

        for (int row = 0; row < grid_energy.rows; ++row) {
            for (int col = 0; col < grid_energy.cols; ++col) {
                const int energy = static_cast<int>(
                    grid_energy.at<std::uint8_t>(row, col)
                );
                if (energy >= diff_threshold) {
                    hot_cell_count += 1;
                }
                if (energy > max_energy) {
                    max_energy = energy;
                    best_col = col;
                    best_row = row;
                }
            }
        }

        result.metrics.push_back(
            processing_metric {
                .name = "hot_cell_count",
                .value = static_cast<double>(hot_cell_count),
                .unit = "cells",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "max_cell_energy",
                .value = static_cast<double>(max_energy),
                .unit = "intensity",
            }
        );

        if (best_col < 0 || best_row < 0 || max_energy < min_cell_energy) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "emit_state", .value = "below_threshold" }
            );
            return result;
        }

        bool allow_emit = true;
        if (emit_interval_ms > 0
            && last_emit.time_since_epoch().count() != 0) {
            const auto elapsed
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                      frame_value.ts - last_emit
                  )
                      .count();
            allow_emit = elapsed >= emit_interval_ms;
        }

        if (!allow_emit) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "emit_state", .value = "cooldown" }
            );
            return result;
        }

        const point event_position = grid_cell_center_pct(
            best_col, best_row, grid_cols, grid_rows
        );

        event event_value;
        event_value.kind = event_kind::motion;
        event_value.stream_name = stream_value.get_name();
        event_value.message = "spot_grid_motion";
        event_value.ts = frame_value.ts;
        event_value.pos_pct = event_position;
        result.events.push_back(event_value);

        processing_overlay overlay;
        overlay.kind = processing_overlay_kind::point;
        overlay.label = "hot_cell";
        overlay.anchor_pct = event_position;
        result.overlays.push_back(std::move(overlay));

        {
            std::scoped_lock lock(mtx_);
            last_emit_by_stream_[stream_value.get_name()] = frame_value.ts;
        }

        return result;
    }

private:
    opencv_client daemon_client_;
    processing_algorithm_configuration configuration_;
    std::unordered_map<std::string, cv::Mat> prev_gray_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_emit_by_stream_;
    std::mutex mtx_;
};

class contour_mask_algorithm final : public processing_algorithm {
public:
    contour_mask_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return contour_mask_algorithm_id;
    }

    std::string display_name() const override { return "contour mask"; }

    processing_algorithm_configuration default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("diff_threshold", "28");
        configuration.values.emplace("blur_kernel", "5");
        configuration.values.emplace("morph_kernel", "5");
        configuration.values.emplace("min_contour_area", "120");
        configuration.values.emplace("emit_interval_ms", "150");
        configuration.values.emplace("max_overlays", "3");
        configuration.values.emplace("contour_points_limit", "24");
        return configuration;
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        const auto defaults = default_configuration();
        for (const auto& [key, value] : defaults.values) {
            if (!configuration.values.contains(key)) {
                configuration.values.emplace(key, value);
            }
        }
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        daemon_client_.daemon_start(stream_value, on_frame, stop_token);
    }

    processing_result process_frame(
        const stream& stream_value, const frame& frame_value
    ) override {
        processing_result result;
        result.diagnostics.push_back(
            processing_diagnostic { .key = "algorithm", .value = algorithm_id() }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "display_name",
                .value = display_name(),
            }
        );

        const cv::Mat gray = frame_to_gray_mat(frame_value);
        if (gray.empty()) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "frame_state", .value = "invalid" }
            );
            return result;
        }

        const int diff_threshold
            = config_int(configuration_, "diff_threshold", 28, 1, 255);
        int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        int morph_kernel
            = config_int(configuration_, "morph_kernel", 5, 1, 31);
        const int min_contour_area
            = config_int(configuration_, "min_contour_area", 120, 1, 1000000);
        const int emit_interval_ms
            = config_int(configuration_, "emit_interval_ms", 150, 0, 5000);
        const int max_overlays
            = config_int(configuration_, "max_overlays", 3, 1, 8);
        const int contour_points_limit = config_int(
            configuration_, "contour_points_limit", 24, 3, 128
        );

        if (blur_kernel % 2 == 0) {
            blur_kernel += 1;
        }
        if (morph_kernel % 2 == 0) {
            morph_kernel += 1;
        }

        cv::Mat previous_gray;
        std::optional<point> previous_center;
        std::chrono::steady_clock::time_point last_emit {};
        bool has_previous = false;

        {
            std::scoped_lock lock(mtx_);

            const auto previous_it = prev_gray_by_stream_.find(
                stream_value.get_name()
            );
            if (previous_it != prev_gray_by_stream_.end()
                && !previous_it->second.empty()
                && previous_it->second.size() == gray.size()) {
                previous_gray = previous_it->second.clone();
                has_previous = true;
            }

            prev_gray_by_stream_[stream_value.get_name()] = gray.clone();

            const auto center_it = prev_center_by_stream_.find(
                stream_value.get_name()
            );
            if (center_it != prev_center_by_stream_.end()) {
                previous_center = center_it->second;
            }

            const auto emit_it = last_emit_by_stream_.find(stream_value.get_name());
            if (emit_it != last_emit_by_stream_.end()) {
                last_emit = emit_it->second;
            }
        }

        result.metrics.push_back(
            processing_metric {
                .name = "line_count",
                .value = static_cast<double>(
                    stream_value.lines_snapshot().size()
                ),
                .unit = "lines",
            }
        );

        if (!has_previous) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "frame_state", .value = "warmup" }
            );
            return result;
        }

        cv::Mat diff;
        cv::absdiff(previous_gray, gray, diff);
        if (blur_kernel > 1) {
            cv::GaussianBlur(
                diff, diff, cv::Size(blur_kernel, blur_kernel), 0.0
            );
        }

        cv::Mat binary_mask;
        cv::threshold(
            diff, binary_mask, static_cast<double>(diff_threshold), 255.0,
            cv::THRESH_BINARY
        );

        if (morph_kernel > 1) {
            const cv::Mat kernel = cv::getStructuringElement(
                cv::MORPH_ELLIPSE, cv::Size(morph_kernel, morph_kernel)
            );
            cv::morphologyEx(
                binary_mask, binary_mask, cv::MORPH_CLOSE, kernel
            );
        }

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(
            binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE
        );

        std::vector<std::vector<cv::Point>> accepted_contours;
        accepted_contours.reserve(contours.size());

        double total_contour_area = 0.0;
        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
            if (area < static_cast<double>(min_contour_area)) {
                continue;
            }
            accepted_contours.push_back(contour);
            total_contour_area += area;
        }

        std::sort(
            accepted_contours.begin(), accepted_contours.end(),
            [](const auto& lhs, const auto& rhs) {
                return cv::contourArea(lhs) > cv::contourArea(rhs);
            }
        );

        const double frame_area = static_cast<double>(gray.cols * gray.rows);
        const double largest_contour_area = accepted_contours.empty()
            ? 0.0
            : cv::contourArea(accepted_contours.front());

        result.metrics.push_back(
            processing_metric {
                .name = "contour_count",
                .value = static_cast<double>(accepted_contours.size()),
                .unit = "contours",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "largest_contour_area",
                .value = largest_contour_area,
                .unit = "px2",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "mask_fill_ratio",
                .value = frame_area > 0.0 ? total_contour_area / frame_area : 0.0,
                .unit = "ratio",
            }
        );

        if (accepted_contours.empty()) {
            {
                std::scoped_lock lock(mtx_);
                prev_center_by_stream_.erase(stream_value.get_name());
            }

            result.diagnostics.push_back(
                processing_diagnostic {
                    .key = "mask_state",
                    .value = "no_contours",
                }
            );
            return result;
        }

        const auto current_center = contour_center_pct(
            accepted_contours.front(), gray.size()
        );
        if (!current_center.has_value()) {
            result.diagnostics.push_back(
                processing_diagnostic {
                    .key = "mask_state",
                    .value = "invalid_center",
                }
            );
            return result;
        }

        const size_t overlay_count = std::min(
            accepted_contours.size(), static_cast<size_t>(max_overlays)
        );
        for (size_t index = 0; index < overlay_count; ++index) {
            processing_overlay overlay;
            overlay.kind = processing_overlay_kind::polygon;
            overlay.label = index == 0 ? "contour_mask" : "contour";
            overlay.points_pct = contour_to_pct(
                accepted_contours[index], gray.size(),
                static_cast<size_t>(contour_points_limit)
            );
            overlay.anchor_pct = current_center;
            if (overlay.points_pct.size() >= 3) {
                result.overlays.push_back(std::move(overlay));
            }
        }

        const double motion_strength = std::clamp(
            frame_area > 0.0 ? largest_contour_area / (frame_area * 0.2) : 0.0,
            0.0, 1.0
        );

        if (previous_center.has_value()) {
            const double speed = std::clamp(
                std::hypot(
                    static_cast<double>(current_center->x - previous_center->x),
                    static_cast<double>(current_center->y - previous_center->y)
                )
                    / 6.0,
                0.5, 4.0
            );
            emit_tripwire_events(
                result.events, stream_value, *previous_center, *current_center,
                frame_value.ts, 0.5 + motion_strength * 0.5, speed
            );
        }

        bool allow_emit = true;
        if (emit_interval_ms > 0
            && last_emit.time_since_epoch().count() != 0) {
            const auto elapsed
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                      frame_value.ts - last_emit
                  )
                      .count();
            allow_emit = elapsed >= emit_interval_ms;
        }

        if (allow_emit) {
            event event_value;
            event_value.kind = event_kind::motion;
            event_value.stream_name = stream_value.get_name();
            event_value.message = "contour_mask_motion";
            event_value.ts = frame_value.ts;
            event_value.pos_pct = *current_center;
            result.events.push_back(event_value);

            processing_overlay overlay;
            overlay.kind = processing_overlay_kind::point;
            overlay.label = "contour_center";
            overlay.anchor_pct = *current_center;
            result.overlays.push_back(std::move(overlay));

            {
                std::scoped_lock lock(mtx_);
                last_emit_by_stream_[stream_value.get_name()] = frame_value.ts;
            }
        } else {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "emit_state", .value = "cooldown" }
            );
        }

        {
            std::scoped_lock lock(mtx_);
            prev_center_by_stream_[stream_value.get_name()] = *current_center;
        }

        return result;
    }

private:
    static point pixel_to_pct(const cv::Point& pixel, const cv::Size& size) {
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

    static std::vector<point> contour_to_pct(
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

    static std::optional<point> contour_center_pct(
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

    static float cross_z(const point& a, const point& b, const point& c) {
        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float acx = c.x - a.x;
        const float acy = c.y - a.y;
        return abx * acy - aby * acx;
    }

    static int orient(const point& a, const point& b, const point& c) {
        const float value = cross_z(a, b, c);
        if (value > point::epsilon) {
            return 1;
        }
        if (value < -point::epsilon) {
            return -1;
        }
        return 0;
    }

    static bool between(const float a, const float b, const float c) {
        const auto [lo, hi] = std::minmax(a, b);
        return lo <= c + point::epsilon && c <= hi + point::epsilon;
    }

    static bool on_segment(const point& a, const point& b, const point& c) {
        return orient(a, b, c) == 0 && between(a.x, b.x, c.x)
            && between(a.y, b.y, c.y);
    }

    static bool segments_intersect(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) {
        const int o1 = orient(p1, p2, q1);
        const int o2 = orient(p1, p2, q2);
        const int o3 = orient(q1, q2, p1);
        const int o4 = orient(q1, q2, p2);

        if (o1 != o2 && o3 != o4) {
            return true;
        }

        if (o1 == 0 && on_segment(p1, p2, q1)) {
            return true;
        }
        if (o2 == 0 && on_segment(p1, p2, q2)) {
            return true;
        }
        if (o3 == 0 && on_segment(q1, q2, p1)) {
            return true;
        }
        if (o4 == 0 && on_segment(q1, q2, p2)) {
            return true;
        }

        return false;
    }

    static std::optional<point> segment_intersection(
        const point& p1, const point& p2, const point& q1, const point& q2
    ) {
        const float rpx = p2.x - p1.x;
        const float rpy = p2.y - p1.y;
        const float spx = q2.x - q1.x;
        const float spy = q2.y - q1.y;

        const float denominator = rpx * spy - rpy * spx;
        if (std::abs(denominator) <= point::epsilon) {
            return std::nullopt;
        }

        const float qpx = q1.x - p1.x;
        const float qpy = q1.y - p1.y;
        const float t = (qpx * spy - qpy * spx) / denominator;
        const float u = (qpx * rpy - qpy * rpx) / denominator;

        if (t < -point::epsilon || t > 1.0f + point::epsilon
            || u < -point::epsilon || u > 1.0f + point::epsilon) {
            return std::nullopt;
        }

        point value;
        value.x = p1.x + t * rpx;
        value.y = p1.y + t * rpy;
        return value;
    }

    bool allow_tripwire_emit(
        const std::string& key,
        const std::chrono::steady_clock::time_point timestamp
    ) {
        constexpr auto tripwire_cooldown = std::chrono::milliseconds(1200);

        std::scoped_lock lock(mtx_);
        const auto it = last_tripwire_by_key_.find(key);
        if (it != last_tripwire_by_key_.end()
            && timestamp - it->second < tripwire_cooldown) {
            return false;
        }

        last_tripwire_by_key_[key] = timestamp;
        return true;
    }

    void emit_tripwire_events(
        std::vector<event>& events, const stream& stream_value,
        const point& previous_center, const point& current_center,
        const std::chrono::steady_clock::time_point timestamp,
        const double strength, const double speed
    ) {
        const auto lines = stream_value.lines_snapshot();

        for (const auto& line_ptr_value : lines) {
            if (!line_ptr_value || line_ptr_value->points.size() < 2) {
                continue;
            }

            bool hit = false;
            point hit_a {};
            point hit_b {};
            point hit_position = current_center;
            double best_dist2 = std::numeric_limits<double>::max();

            const auto consider_segment = [&](const point& a, const point& b) {
                if (!segments_intersect(previous_center, current_center, a, b)) {
                    return;
                }

                const point position = segment_intersection(
                                           previous_center, current_center, a, b
                                       )
                                           .value_or(current_center);
                const double dx = static_cast<double>(position.x - current_center.x);
                const double dy = static_cast<double>(position.y - current_center.y);
                const double distance2 = dx * dx + dy * dy;
                if (distance2 < best_dist2) {
                    best_dist2 = distance2;
                    hit = true;
                    hit_a = a;
                    hit_b = b;
                    hit_position = position;
                }
            };

            for (size_t index = 1; index < line_ptr_value->points.size(); ++index) {
                consider_segment(
                    line_ptr_value->points[index - 1], line_ptr_value->points[index]
                );
            }

            if (line_ptr_value->closed && line_ptr_value->points.size() > 2) {
                consider_segment(
                    line_ptr_value->points.back(), line_ptr_value->points.front()
                );
            }

            if (!hit) {
                continue;
            }

            const float previous_side = cross_z(
                hit_a, hit_b, previous_center
            );
            const float current_side = cross_z(hit_a, hit_b, current_center);

            std::string direction = "flat";
            if (previous_side <= 0.0f && current_side > 0.0f) {
                direction = "neg_to_pos";
            } else if (previous_side >= 0.0f && current_side < 0.0f) {
                direction = "pos_to_neg";
            }

            if (line_ptr_value->dir == tripwire_dir::neg_to_pos
                && direction != "neg_to_pos") {
                continue;
            }
            if (line_ptr_value->dir == tripwire_dir::pos_to_neg
                && direction != "pos_to_neg") {
                continue;
            }

            if (!allow_tripwire_emit(
                    stream_value.get_name() + "|" + line_ptr_value->name + "|"
                        + direction,
                    timestamp
                )) {
                continue;
            }

            event event_value;
            event_value.kind = event_kind::tripwire;
            event_value.stream_name = stream_value.get_name();
            event_value.line_name = line_ptr_value->name;
            event_value.ts = timestamp;
            event_value.pos_pct = hit_position;
            event_value.message = direction + "|" + std::to_string(strength)
                + "|" + std::to_string(speed);
            events.push_back(std::move(event_value));
        }
    }

    opencv_client daemon_client_;
    processing_algorithm_configuration configuration_;
    std::unordered_map<std::string, cv::Mat> prev_gray_by_stream_;
    std::unordered_map<std::string, point> prev_center_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_emit_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_tripwire_by_key_;
    std::mutex mtx_;
};

class hybrid_auto_algorithm final : public processing_algorithm {
public:
    hybrid_auto_algorithm()
        : motion_baseline_(std::make_unique<motion_baseline_algorithm>())
        , spot_grid_(std::make_unique<spot_grid_algorithm>())
        , contour_mask_(std::make_unique<contour_mask_algorithm>()) {
        configuration_ = default_configuration();
    }

    std::string algorithm_id() const override { return hybrid_auto_algorithm_id; }

    std::string display_name() const override { return "hybrid auto"; }

    processing_algorithm_configuration default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("probe_grid_cols", "16");
        configuration.values.emplace("probe_grid_rows", "16");
        configuration.values.emplace("diff_threshold", "24");
        configuration.values.emplace("blur_kernel", "5");
        configuration.values.emplace("calm_motion_permille", "24");
        configuration.values.emplace("busy_motion_permille", "110");
        configuration.values.emplace("overload_avg_ms", "10");
        configuration.values.emplace("recover_avg_ms", "6");
        return configuration;
    }

    processing_algorithm_configuration configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        const auto defaults = default_configuration();
        for (const auto& [key, value] : defaults.values) {
            if (!configuration.values.contains(key)) {
                configuration.values.emplace(key, value);
            }
        }
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value, const std::function<void(frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        daemon_client_.daemon_start(stream_value, on_frame, stop_token);
    }

    processing_result process_frame(
        const stream& stream_value, const frame& frame_value
    ) override {
        processing_result result;
        result.diagnostics.push_back(
            processing_diagnostic { .key = "algorithm", .value = algorithm_id() }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "display_name",
                .value = display_name(),
            }
        );

        const cv::Mat gray = frame_to_gray_mat(frame_value);
        if (gray.empty()) {
            result.diagnostics.push_back(
                processing_diagnostic { .key = "frame_state", .value = "invalid" }
            );
            return result;
        }

        const int probe_grid_cols
            = config_int(configuration_, "probe_grid_cols", 16, 2, 64);
        const int probe_grid_rows
            = config_int(configuration_, "probe_grid_rows", 16, 2, 64);
        const int diff_threshold
            = config_int(configuration_, "diff_threshold", 24, 1, 255);
        int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        const int calm_motion_permille = config_int(
            configuration_, "calm_motion_permille", 24, 0, 1000
        );
        const int busy_motion_permille = config_int(
            configuration_, "busy_motion_permille", 110, 0, 1000
        );
        const int overload_avg_ms
            = config_int(configuration_, "overload_avg_ms", 10, 0, 5000);
        const int recover_avg_ms
            = config_int(configuration_, "recover_avg_ms", 6, 0, 5000);

        if (blur_kernel % 2 == 0) {
            blur_kernel += 1;
        }

        const int line_count = static_cast<int>(
            stream_value.lines_snapshot().size()
        );

        cv::Mat previous_gray;
        bool has_previous = false;
        double average_processing_ms = 0.0;
        std::string last_selected_algorithm;

        {
            std::scoped_lock lock(mtx_);
            auto& state = state_by_stream_[stream_value.get_name()];
            if (!state.previous_gray.empty()
                && state.previous_gray.size() == gray.size()) {
                previous_gray = state.previous_gray.clone();
                has_previous = true;
            }

            average_processing_ms = state.average_processing_ms;
            last_selected_algorithm = state.selected_algorithm_id;
            state.previous_gray = gray.clone();
        }

        int motion_permille = 0;
        if (has_previous) {
            cv::Mat diff;
            cv::absdiff(previous_gray, gray, diff);
            if (blur_kernel > 1) {
                cv::GaussianBlur(
                    diff, diff, cv::Size(blur_kernel, blur_kernel), 0.0
                );
            }

            cv::Mat probe_energy;
            cv::resize(
                diff, probe_energy, cv::Size(probe_grid_cols, probe_grid_rows),
                0.0, 0.0, cv::INTER_AREA
            );

            int hot_cells = 0;
            for (int row = 0; row < probe_energy.rows; ++row) {
                for (int col = 0; col < probe_energy.cols; ++col) {
                    if (static_cast<int>(probe_energy.at<std::uint8_t>(row, col))
                        >= diff_threshold) {
                        hot_cells += 1;
                    }
                }
            }

            const int total_cells = std::max(1, probe_grid_cols * probe_grid_rows);
            motion_permille = static_cast<int>(
                (1000.0 * static_cast<double>(hot_cells))
                / static_cast<double>(total_cells)
            );
        }

        const auto selection = select_algorithm(
            line_count, motion_permille, average_processing_ms,
            last_selected_algorithm, has_previous, calm_motion_permille,
            busy_motion_permille, overload_avg_ms, recover_avg_ms
        );

        auto* delegate = delegate_for_algorithm(selection.algorithm_id);
        if (delegate == nullptr) {
            result.diagnostics.push_back(
                processing_diagnostic {
                    .key = "selection_state",
                    .value = "missing_delegate",
                }
            );
            return result;
        }

        const auto started = std::chrono::steady_clock::now();
        processing_result delegate_result
            = delegate->process_frame(stream_value, frame_value);
        const auto finished = std::chrono::steady_clock::now();
        const double elapsed_ms
            = static_cast<double>(
                  std::chrono::duration_cast<std::chrono::microseconds>(
                      finished - started
                  )
                      .count()
              )
            / 1000.0;
        const double updated_average_processing_ms = average_processing_ms > 0.0
            ? (average_processing_ms * 0.7 + elapsed_ms * 0.3)
            : elapsed_ms;

        {
            std::scoped_lock lock(mtx_);
            auto& state = state_by_stream_[stream_value.get_name()];
            state.average_processing_ms = updated_average_processing_ms;
            state.selected_algorithm_id = selection.algorithm_id;
        }

        result.events = std::move(delegate_result.events);
        result.overlays = std::move(delegate_result.overlays);

        result.metrics.push_back(
            processing_metric {
                .name = "line_count",
                .value = static_cast<double>(line_count),
                .unit = "lines",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "scene_motion_permille",
                .value = static_cast<double>(motion_permille),
                .unit = "permille",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "adaptive_processing_ms",
                .value = elapsed_ms,
                .unit = "ms",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "adaptive_average_processing_ms",
                .value = updated_average_processing_ms,
                .unit = "ms",
            }
        );

        for (auto& metric : delegate_result.metrics) {
            metric.name = "selected_" + metric.name;
            result.metrics.push_back(std::move(metric));
        }

        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "selected_algorithm",
                .value = selection.algorithm_id,
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "selection_reason",
                .value = selection.reason,
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "scene_state",
                .value = selection.scene_state,
            }
        );

        for (auto& diagnostic : delegate_result.diagnostics) {
            if (diagnostic.key == "algorithm") {
                result.diagnostics.push_back(
                    processing_diagnostic {
                        .key = "selected_algorithm_reported",
                        .value = diagnostic.value,
                    }
                );
                continue;
            }
            if (diagnostic.key == "display_name") {
                result.diagnostics.push_back(
                    processing_diagnostic {
                        .key = "selected_display_name",
                        .value = diagnostic.value,
                    }
                );
                continue;
            }

            diagnostic.key = "selected_" + diagnostic.key;
            result.diagnostics.push_back(std::move(diagnostic));
        }

        return result;
    }

private:
    struct adaptive_state {
        cv::Mat previous_gray;
        double average_processing_ms { 0.0 };
        std::string selected_algorithm_id;
    };

    struct selection_decision {
        std::string algorithm_id;
        std::string reason;
        std::string scene_state;
    };

    static selection_decision select_algorithm(
        const int line_count, const int motion_permille,
        const double average_processing_ms,
        const std::string& last_selected_algorithm, const bool has_previous,
        const int calm_motion_permille, const int busy_motion_permille,
        const int overload_avg_ms, const int recover_avg_ms
    ) {
        if (!has_previous) {
            if (line_count > 0) {
                return selection_decision {
                    .algorithm_id = motion_baseline_algorithm_id,
                    .reason = "warmup_tripwire_bias",
                    .scene_state = "warmup",
                };
            }

            return selection_decision {
                .algorithm_id = spot_grid_algorithm_id,
                .reason = "warmup_no_lines",
                .scene_state = "warmup",
            };
        }

        if (overload_avg_ms == 0
            || average_processing_ms >= static_cast<double>(overload_avg_ms)) {
            return selection_decision {
                .algorithm_id = spot_grid_algorithm_id,
                .reason = overload_avg_ms == 0 ? "forced_load_guard"
                                               : "load_guard",
                .scene_state = "load_guard",
            };
        }

        if (last_selected_algorithm == spot_grid_algorithm_id
            && recover_avg_ms > 0
            && average_processing_ms >= static_cast<double>(recover_avg_ms)) {
            return selection_decision {
                .algorithm_id = spot_grid_algorithm_id,
                .reason = "load_hysteresis",
                .scene_state = "load_guard",
            };
        }

        if (motion_permille >= busy_motion_permille) {
            return selection_decision {
                .algorithm_id = contour_mask_algorithm_id,
                .reason = "busy_scene",
                .scene_state = "busy",
            };
        }

        if (line_count > 0 && motion_permille >= calm_motion_permille) {
            return selection_decision {
                .algorithm_id = motion_baseline_algorithm_id,
                .reason = "tripwire_bias",
                .scene_state = "tripwire",
            };
        }

        if (line_count > 0
            && last_selected_algorithm == motion_baseline_algorithm_id
            && motion_permille > 0) {
            return selection_decision {
                .algorithm_id = motion_baseline_algorithm_id,
                .reason = "tripwire_hysteresis",
                .scene_state = "tripwire",
            };
        }

        return selection_decision {
            .algorithm_id = spot_grid_algorithm_id,
            .reason = motion_permille <= calm_motion_permille ? "calm_scene"
                                                              : "low_value_motion",
            .scene_state = motion_permille <= calm_motion_permille ? "calm"
                                                                   : "sparse",
        };
    }

    processing_algorithm* delegate_for_algorithm(const std::string& algorithm_id) {
        if (algorithm_id == contour_mask_algorithm_id) {
            return contour_mask_.get();
        }
        if (algorithm_id == motion_baseline_algorithm_id) {
            return motion_baseline_.get();
        }
        return spot_grid_.get();
    }

    opencv_client daemon_client_;
    std::unique_ptr<motion_baseline_algorithm> motion_baseline_;
    std::unique_ptr<spot_grid_algorithm> spot_grid_;
    std::unique_ptr<contour_mask_algorithm> contour_mask_;
    processing_algorithm_configuration configuration_;
    std::unordered_map<std::string, adaptive_state> state_by_stream_;
    std::mutex mtx_;
};
#endif

const processing_algorithm_registry& registry_instance() {
    static const processing_algorithm_registry registry = [] {
        processing_algorithm_registry value;
#ifdef YODAU_OPENCV
        value.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = motion_baseline_algorithm_id,
                .display_name = "motion baseline",
                .create = [] {
                    return std::make_unique<motion_baseline_algorithm>();
                },
            }
        );
        value.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = spot_grid_algorithm_id,
                .display_name = "spot grid",
                .create = [] {
                    return std::make_unique<spot_grid_algorithm>();
                },
            }
        );
        value.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = contour_mask_algorithm_id,
                .display_name = "contour mask",
                .create = [] {
                    return std::make_unique<contour_mask_algorithm>();
                },
            }
        );
        value.register_algorithm(
            processing_algorithm_registry::entry {
                .algorithm_id = hybrid_auto_algorithm_id,
                .display_name = "hybrid auto",
                .create = [] {
                    return std::make_unique<hybrid_auto_algorithm>();
                },
            }
        );
#endif
        return value;
    }();

    return registry;
}

processing_algorithm* shared_default_algorithm_instance() {
    static std::unique_ptr<processing_algorithm> algorithm
        = make_processing_algorithm();
    return algorithm.get();
}

} // namespace default_processing_hooks_support

stream_manager::daemon_start_fn default_daemon_start_hook() {
    if (default_processing_hooks_support::shared_default_algorithm_instance()
        == nullptr) {
        return {};
    }

    return [](
               const stream& stream_value,
               const std::function<void(frame&&)>& on_frame,
               const std::stop_token& stop_token
           ) {
        if (auto* algorithm
            = default_processing_hooks_support::shared_default_algorithm_instance()) {
            algorithm->daemon_start(stream_value, on_frame, stop_token);
        }
    };
}

stream_manager::frame_processor_fn default_frame_processor() {
    if (default_processing_hooks_support::shared_default_algorithm_instance()
        == nullptr) {
        return {};
    }

    return [](const stream& stream_value, const frame& frame_value) {
        if (auto* algorithm
            = default_processing_hooks_support::shared_default_algorithm_instance()) {
            return algorithm->process_frame(stream_value, frame_value).events;
        }

        return std::vector<event> {};
    };
}

std::string default_processing_algorithm_id() {
    const std::string candidate(
        default_processing_hooks_support::motion_baseline_algorithm_id
    );
    return default_processing_algorithm_registry().contains(candidate) ? candidate
                                                                       : std::string {};
}

const processing_algorithm_registry& default_processing_algorithm_registry() {
    return default_processing_hooks_support::registry_instance();
}

std::unique_ptr<processing_algorithm>
make_processing_algorithm(const std::string& algorithm_id) {
    const auto& registry = default_processing_algorithm_registry();
    const std::string requested_algorithm_id
        = default_processing_hooks_support::normalized_requested_algorithm_id(
            algorithm_id
        );

    if (!requested_algorithm_id.empty()) {
        if (auto algorithm = registry.create(requested_algorithm_id)) {
            return algorithm;
        }
    }

    const std::string fallback_algorithm_id = default_processing_algorithm_id();
    if (fallback_algorithm_id.empty()) {
        return {};
    }

    return registry.create(fallback_algorithm_id);
}

} // namespace yodau::core
