#include "analysis/default_processing_algorithms.hpp"

#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_background_model.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"
#include "analysis/processing_contour_tools.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_focus.hpp"
#include "analysis/processing_motion_tools.hpp"
#include "analysis/processing_overlay_tools.hpp"
#include "analysis/processing_tripwire_tools.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yodau::core {

namespace {

class contour_mask_algorithm final : public processing_algorithm {
public:
    contour_mask_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return processing_algorithm_ids::contour_mask;
    }

    std::string display_name() const override {
        return processing_algorithm_display_name(algorithm_id());
    }

    processing_algorithm_configuration default_configuration() const override {
        return processing_algorithm_default_configuration(algorithm_id());
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
        const int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        const int morph_kernel
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
        const auto background_model = processing_background_model_kind_from_id(
            config_string(configuration_, "background_model", "frame_delta")
        );
        const int background_history_frames = config_int(
            configuration_, "background_history_frames", 120, 2, 10000
        );
        const int background_threshold = config_int(
            configuration_, "background_threshold", 16, 1, 1000
        );
        const int background_learning_permille = config_int(
            configuration_, "background_learning_permille", 5, 0, 1000
        );
        const bool background_detect_shadows
            = config_int(configuration_, "background_detect_shadows", 0, 0, 1)
            != 0;
        const auto motion_focus_mode = processing_motion_focus_mode_from_id(
            config_string(configuration_, "motion_focus_mode", "auto")
        );
        const int line_focus_width_pct = config_int(
            configuration_, "line_focus_width_pct", 8, 1, 100
        );

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

        cv::Mat binary_mask = background_models_.motion_mask(
            stream_value.get_name(), previous_gray, gray,
            processing_background_model_options {
                .kind = background_model,
                .diff_threshold = diff_threshold,
                .blur_kernel = blur_kernel,
                .morph_kernel = morph_kernel,
                .history_frames = background_history_frames,
                .model_threshold = static_cast<double>(background_threshold),
                .learning_rate = static_cast<double>(
                    background_learning_permille
                )
                    / 1000.0,
                .detect_shadows = background_detect_shadows,
            }
        );
        const auto focus = build_motion_focus_mask(
            stream_value, gray.size(),
            processing_motion_focus_options {
                .mode = motion_focus_mode,
                .corridor_width_pct = static_cast<float>(line_focus_width_pct),
            }
        );
        if (!focus.mask.empty()) {
            cv::bitwise_and(binary_mask, focus.mask, binary_mask);
        }
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "background_model",
                .value = processing_background_model_kind_id(background_model),
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "motion_focus_shape_count",
                .value = static_cast<double>(focus.shape_count),
                .unit = "shapes",
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "motion_focus",
                .value = processing_motion_focus_mode_id(motion_focus_mode),
            }
        );
        const auto contour_candidates = contour_candidates_by_area(
            binary_mask, gray.size(), static_cast<double>(min_contour_area)
        );
        double total_contour_area = 0.0;
        for (const auto& candidate : contour_candidates) {
            total_contour_area += candidate.area_px2;
        }

        const double frame_area = static_cast<double>(gray.cols * gray.rows);
        const double largest_contour_area = contour_candidates.empty()
            ? 0.0
            : contour_candidates.front().area_px2;

        result.metrics.push_back(
            processing_metric {
                .name = "contour_count",
                .value = static_cast<double>(contour_candidates.size()),
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

        if (contour_candidates.empty()) {
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

        const point current_center = contour_candidates.front().center_pct;

        const size_t overlay_count = std::min(
            contour_candidates.size(), static_cast<size_t>(max_overlays)
        );
        for (size_t index = 0; index < overlay_count; ++index) {
            auto points_pct = contour_to_pct(
                contour_candidates[index].contour, gray.size(),
                static_cast<size_t>(contour_points_limit)
            );
            if (points_pct.size() >= 3) {
                result.overlays.push_back(
                    make_polygon_overlay(
                        index == 0 ? "contour_mask" : "contour",
                        std::move(points_pct),
                        contour_candidates[index].center_pct
                    )
                );
            }
        }

        const double motion_strength = std::clamp(
            frame_area > 0.0 ? largest_contour_area / (frame_area * 0.2) : 0.0,
            0.0, 1.0
        );

        if (previous_center.has_value()) {
            const double speed = std::clamp(
                std::hypot(
                    static_cast<double>(current_center.x - previous_center->x),
                    static_cast<double>(current_center.y - previous_center->y)
                )
                    / 6.0,
                0.5, 4.0
            );
            emit_tripwire_events(
                result.events, stream_value, *previous_center, current_center,
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
            event_value.pos_pct = current_center;
            result.events.push_back(event_value);

            result.overlays.push_back(
                make_point_overlay("contour_center", current_center)
            );

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
            prev_center_by_stream_[stream_value.get_name()] = current_center;
        }

        return result;
    }

private:
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
        for (const auto& crossing : tripwire_crossings_for_motion(
                 stream_value, previous_center, current_center
             )) {
            if (!allow_tripwire_emit(
                    stream_value.get_name() + "|"
                        + tripwire_crossing_key(crossing),
                    timestamp
                )) {
                continue;
            }

            event event_value;
            event_value.kind = event_kind::tripwire;
            event_value.stream_name = stream_value.get_name();
            event_value.line_name = crossing.line_name;
            event_value.ts = timestamp;
            event_value.pos_pct = crossing.position_pct;
            event_value.message = crossing.direction + "|"
                + std::to_string(strength) + "|" + std::to_string(speed);
            events.push_back(std::move(event_value));
        }
    }

    opencv_client daemon_client_;
    processing_background_model_store background_models_;
    processing_algorithm_configuration configuration_;
    std::unordered_map<std::string, cv::Mat> prev_gray_by_stream_;
    std::unordered_map<std::string, point> prev_center_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_emit_by_stream_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        last_tripwire_by_key_;
    std::mutex mtx_;
};

} // namespace

std::unique_ptr<processing_algorithm> make_contour_mask_algorithm() {
    return std::make_unique<contour_mask_algorithm>();
}

} // namespace yodau::core

#endif
