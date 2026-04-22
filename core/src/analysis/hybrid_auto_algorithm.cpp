#include "analysis/default_processing_algorithms.hpp"

#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_tools.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace yodau::core {

namespace {

using namespace processing_algorithm_ids;

class hybrid_auto_algorithm final : public processing_algorithm {
public:
    hybrid_auto_algorithm()
        : motion_baseline_(make_motion_baseline_algorithm())
        , spot_grid_(make_spot_grid_algorithm())
        , contour_mask_(make_contour_mask_algorithm())
        , centroid_track_(make_centroid_track_algorithm()) {
        configuration_ = default_configuration();
    }

    std::string algorithm_id() const override { return hybrid_auto; }

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
        configuration_ = completed_processing_configuration(
            algorithm_id(), std::move(configuration)
        );
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
        add_algorithm_diagnostics(result, *this);

        const cv::Mat gray = frame_to_gray_mat(frame_value);
        if (gray.empty()) {
            add_processing_diagnostic(result, "frame_state", "invalid");
            return result;
        }

        const int probe_grid_cols
            = config_int(configuration_, "probe_grid_cols", 16, 2, 64);
        const int probe_grid_rows
            = config_int(configuration_, "probe_grid_rows", 16, 2, 64);
        const int diff_threshold
            = config_int(configuration_, "diff_threshold", 24, 1, 255);
        const int blur_kernel
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
            const cv::Mat diff = blurred_absdiff(
                previous_gray, gray, blur_kernel
            );

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
            add_processing_diagnostic(
                result, "selection_state", "missing_delegate"
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

        add_processing_metric(
            result, "line_count", static_cast<double>(line_count), "lines"
        );
        add_processing_metric(
            result, "scene_motion_permille",
            static_cast<double>(motion_permille), "permille"
        );
        add_processing_metric(
            result, "adaptive_processing_ms", elapsed_ms, "ms"
        );
        add_processing_metric(
            result, "adaptive_average_processing_ms",
            updated_average_processing_ms, "ms"
        );

        add_prefixed_processing_metrics(
            result, std::move(delegate_result.metrics), "selected_"
        );

        add_processing_diagnostic(
            result, "selected_algorithm", selection.algorithm_id
        );
        add_processing_diagnostic(
            result, "selection_reason", selection.reason
        );
        add_processing_diagnostic(
            result, "scene_state", selection.scene_state
        );

        for (auto& diagnostic : delegate_result.diagnostics) {
            if (diagnostic.key == "algorithm") {
                add_processing_diagnostic(
                    result, "selected_algorithm_reported", diagnostic.value
                );
                continue;
            }
            if (diagnostic.key == "display_name") {
                add_processing_diagnostic(
                    result, "selected_display_name", diagnostic.value
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
                    .algorithm_id = motion_baseline,
                    .reason = "warmup_tripwire_bias",
                    .scene_state = "warmup",
                };
            }

            return selection_decision {
                .algorithm_id = spot_grid,
                .reason = "warmup_no_lines",
                .scene_state = "warmup",
            };
        }

        if (overload_avg_ms == 0
            || average_processing_ms >= static_cast<double>(overload_avg_ms)) {
            return selection_decision {
                .algorithm_id = spot_grid,
                .reason = overload_avg_ms == 0 ? "forced_load_guard"
                                               : "load_guard",
                .scene_state = "load_guard",
            };
        }

        if (last_selected_algorithm == spot_grid
            && recover_avg_ms > 0
            && average_processing_ms >= static_cast<double>(recover_avg_ms)) {
            return selection_decision {
                .algorithm_id = spot_grid,
                .reason = "load_hysteresis",
                .scene_state = "load_guard",
            };
        }

        if (motion_permille >= busy_motion_permille) {
            return selection_decision {
                .algorithm_id = contour_mask,
                .reason = "busy_scene",
                .scene_state = "busy",
            };
        }

        if (line_count > 0 && motion_permille >= calm_motion_permille) {
            return selection_decision {
                .algorithm_id = motion_baseline,
                .reason = "tripwire_bias",
                .scene_state = "tripwire",
            };
        }

        if (line_count > 0
            && last_selected_algorithm == motion_baseline
            && motion_permille > 0) {
            return selection_decision {
                .algorithm_id = motion_baseline,
                .reason = "tripwire_hysteresis",
                .scene_state = "tripwire",
            };
        }

        if (line_count == 0 && motion_permille >= calm_motion_permille) {
            return selection_decision {
                .algorithm_id = centroid_track,
                .reason = "trackable_motion",
                .scene_state = "tracking",
            };
        }

        if (line_count == 0
            && last_selected_algorithm == centroid_track
            && motion_permille > 0) {
            return selection_decision {
                .algorithm_id = centroid_track,
                .reason = "tracking_hysteresis",
                .scene_state = "tracking",
            };
        }

        return selection_decision {
            .algorithm_id = spot_grid,
            .reason = motion_permille <= calm_motion_permille ? "calm_scene"
                                                              : "low_value_motion",
            .scene_state = motion_permille <= calm_motion_permille ? "calm"
                                                                   : "sparse",
        };
    }

    processing_algorithm* delegate_for_algorithm(const std::string& algorithm_id) {
        if (algorithm_id == contour_mask) {
            return contour_mask_.get();
        }
        if (algorithm_id == centroid_track) {
            return centroid_track_.get();
        }
        if (algorithm_id == motion_baseline) {
            return motion_baseline_.get();
        }
        return spot_grid_.get();
    }

    opencv_client daemon_client_;
    std::unique_ptr<processing_algorithm> motion_baseline_;
    std::unique_ptr<processing_algorithm> spot_grid_;
    std::unique_ptr<processing_algorithm> contour_mask_;
    std::unique_ptr<processing_algorithm> centroid_track_;
    processing_algorithm_configuration configuration_;
    std::unordered_map<std::string, adaptive_state> state_by_stream_;
    std::mutex mtx_;
};

} // namespace

std::unique_ptr<processing_algorithm> make_hybrid_auto_algorithm() {
    return std::make_unique<hybrid_auto_algorithm>();
}

} // namespace yodau::core

#endif
