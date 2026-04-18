#include "analysis/default_processing_algorithms.hpp"

#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_tools.hpp"
#include "analysis/processing_overlay_tools.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace yodau::core {

namespace {

class spot_grid_algorithm final : public processing_algorithm {
public:
    spot_grid_algorithm() { configuration_ = default_configuration(); }

    std::string algorithm_id() const override {
        return processing_algorithm_ids::spot_grid;
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

        const int grid_cols
            = config_int(configuration_, "grid_cols", 12, 2, 64);
        const int grid_rows
            = config_int(configuration_, "grid_rows", 12, 2, 64);
        const int diff_threshold
            = config_int(configuration_, "diff_threshold", 26, 1, 255);
        const int min_cell_energy
            = config_int(configuration_, "min_cell_energy", 30, 1, 255);
        const int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        const int emit_interval_ms
            = config_int(configuration_, "emit_interval_ms", 120, 0, 5000);

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

        const cv::Mat diff = blurred_absdiff(previous_gray, gray, blur_kernel);

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

        result.overlays.push_back(
            make_point_overlay("hot_cell", event_position)
        );

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

} // namespace

std::unique_ptr<processing_algorithm> make_spot_grid_algorithm() {
    return std::make_unique<spot_grid_algorithm>();
}

} // namespace yodau::core

#endif
