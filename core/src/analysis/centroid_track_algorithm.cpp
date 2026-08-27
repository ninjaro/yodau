#include "analysis/default_processing_algorithms.hpp"

#ifdef YODAU_OPENCV

#include "analysis/opencv_client.hpp"
#include "analysis/processing_algorithm_catalog.hpp"
#include "analysis/processing_algorithm_ids.hpp"
#include "analysis/processing_background_model.hpp"
#include "analysis/processing_contour_tools.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_focus.hpp"
#include "analysis/processing_motion_tools.hpp"
#include "analysis/processing_overlay_tools.hpp"
#include "analysis/processing_sparse_flow.hpp"
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

    class centroid_track_algorithm final : public processing_algorithm {
    public:
        centroid_track_algorithm() { configuration_ = default_configuration(); }

        std::string algorithm_id() const override {
            return processing_algorithm_ids::centroid_track;
        }

        std::string display_name() const override {
            return processing_algorithm_display_name(algorithm_id());
        }

        processing_algorithm_configuration
        default_configuration() const override {
            return processing_algorithm_default_configuration(algorithm_id());
        }

        processing_algorithm_configuration configuration() const override {
            return configuration_;
        }

        void
        configure(processing_algorithm_configuration configuration) override {
            configuration_ = completed_processing_configuration(
                algorithm_id(), std::move(configuration)
            );
        }

        void daemon_start(
            const stream& stream_value,
            const std::function<void(frame&&)>& on_frame,
            const stop_token& stop_token
        ) override {
            opencv_client::daemon_start(stream_value, on_frame, stop_token);
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

            const int diff_threshold
                = config_int(configuration_, "diff_threshold", 24, 1, 255);
            const int blur_kernel
                = config_int(configuration_, "blur_kernel", 5, 1, 31);
            const int morph_kernel
                = config_int(configuration_, "morph_kernel", 5, 1, 31);
            const int min_contour_area = config_int(
                configuration_, "min_contour_area", 96, 1, 1000000
            );
            const int match_radius_pct
                = config_int(configuration_, "match_radius_pct", 12, 1, 50);
            const int min_track_age_frames
                = config_int(configuration_, "min_track_age_frames", 2, 1, 120);
            const int max_track_gap_frames
                = config_int(configuration_, "max_track_gap_frames", 3, 0, 120);
            const int track_history_limit
                = config_int(configuration_, "track_history_limit", 10, 2, 64);
            const int max_overlays
                = config_int(configuration_, "max_overlays", 4, 1, 8);
            const int emit_interval_ms
                = config_int(configuration_, "emit_interval_ms", 120, 0, 5000);
            const auto background_model = background_model_kind_from_id(
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
                = config_int(
                      configuration_, "background_detect_shadows", 0, 0, 1
                  )
                != 0;
            const auto motion_focus_mode = motion_focus_mode_from_id(
                config_string(configuration_, "motion_focus_mode", "auto")
            );
            const int line_focus_width_pct
                = config_int(configuration_, "line_focus_width_pct", 8, 1, 100);
            const bool sparse_flow_enabled
                = config_int(configuration_, "sparse_flow_enabled", 1, 0, 1)
                != 0;
            const int sparse_flow_max_features = config_int(
                configuration_, "sparse_flow_max_features", 80, 1, 1000
            );
            const int sparse_flow_quality_permille = config_int(
                configuration_, "sparse_flow_quality_permille", 12, 1, 1000
            );
            const int sparse_flow_min_distance_px = config_int(
                configuration_, "sparse_flow_min_distance_px", 7, 1, 512
            );
            const int sparse_flow_window_px = config_int(
                configuration_, "sparse_flow_window_px", 15, 3, 101
            );
            const int sparse_flow_pyramid_levels = config_int(
                configuration_, "sparse_flow_pyramid_levels", 3, 0, 8
            );
            const int sparse_flow_max_error = config_int(
                configuration_, "sparse_flow_max_error", 24, 0, 1000
            );
            const int sparse_flow_min_vector_px = config_int(
                configuration_, "sparse_flow_min_vector_px", 2, 0, 512
            );
            const int sparse_flow_prediction_radius_pct = config_int(
                configuration_, "sparse_flow_prediction_radius_pct",
                match_radius_pct, 1, 100
            );
            const int track_velocity_prediction_pct = config_int(
                configuration_, "track_velocity_prediction_pct", 55, 0, 100
            );
            const int track_gap_radius_growth_pct = config_int(
                configuration_, "track_gap_radius_growth_pct", 40, 0, 200
            );
            const int area_match_weight_pct = config_int(
                configuration_, "area_match_weight_pct", 18, 0, 100
            );
            const int max_sparse_flow_overlays = config_int(
                configuration_, "max_sparse_flow_overlays", 8, 0, 64
            );

            tracking_state state;
            {
                std::scoped_lock lock(mtx_);
                if (const auto it
                    = state_by_stream_.find(stream_value.get_name());
                    it != state_by_stream_.end()) {
                    state = it->second;
                }
            }

            const bool compatible_previous = !state.previous_gray.empty()
                && state.previous_gray.size() == gray.size();
            if (!compatible_previous) {
                state.previous_gray = gray.clone();
                state.tracks.clear();
                state.last_emit = {};
                {
                    std::scoped_lock lock(mtx_);
                    state_by_stream_[stream_value.get_name()] = state;
                }

                add_processing_metric(result, "track_count", 0.0, "tracks");
                add_processing_metric(
                    result, "stable_track_count", 0.0, "tracks"
                );
                add_processing_diagnostic(result, "frame_state", "warmup");
                return result;
            }

            cv::Mat binary_mask = background_models_.motion_mask(
                stream_value.get_name(), state.previous_gray, gray,
                processing_background_model_options {
                    .kind = background_model,
                    .diff_threshold = diff_threshold,
                    .blur_kernel = blur_kernel,
                    .morph_kernel = morph_kernel,
                    .history_frames = background_history_frames,
                    .model_threshold
                    = static_cast<double>(background_threshold),
                    .learning_rate
                    = static_cast<double>(background_learning_permille)
                        / 1000.0,
                    .detect_shadows = background_detect_shadows,
                }
            );
            const auto focus = build_motion_focus_mask(
                stream_value, gray.size(),
                processing_motion_focus_options {
                    .mode = motion_focus_mode,
                    .corridor_width_pct
                    = static_cast<float>(line_focus_width_pct),
                }
            );
            if (!focus.mask.empty()) {
                cv::bitwise_and(binary_mask, focus.mask, binary_mask);
            }

            processing_sparse_flow_result sparse_flow;
            if (sparse_flow_enabled) {
                sparse_flow = sparse_optical_flow(
                    state.previous_gray, gray, binary_mask,
                    processing_sparse_flow_options {
                        .max_features = sparse_flow_max_features,
                        .quality_permille = sparse_flow_quality_permille,
                        .min_feature_distance_px = sparse_flow_min_distance_px,
                        .window_size_px = sparse_flow_window_px,
                        .pyramid_levels = sparse_flow_pyramid_levels,
                        .max_error = sparse_flow_max_error,
                        .min_vector_length_px = sparse_flow_min_vector_px,
                    }
                );
            }
            const auto track_predictions = predict_tracks(
                state.tracks, sparse_flow,
                static_cast<double>(sparse_flow_prediction_radius_pct),
                static_cast<double>(track_velocity_prediction_pct),
                static_cast<double>(match_radius_pct),
                static_cast<double>(track_gap_radius_growth_pct)
            );

            const auto contour_candidates = contour_candidates_by_area(
                binary_mask, gray.size(), static_cast<double>(min_contour_area)
            );
            const auto motion_candidates = processing_candidates_from_contours(
                contour_candidates, gray.size(), 0, "motion"
            );
            std::vector<detection> detections;
            detections.reserve(motion_candidates.size());
            for (const auto& candidate : motion_candidates) {
                detections.push_back(
                    detection {
                        .center_pct = candidate.center_pct,
                        .area_px2 = candidate.area_px2,
                        .confidence = candidate.confidence,
                        .class_id = candidate.class_id,
                    }
                );
            }

            std::sort(
                detections.begin(), detections.end(),
                [](const detection& lhs, const detection& rhs) {
                    return lhs.area_px2 > rhs.area_px2;
                }
            );

            std::vector<bool> matched_track(state.tracks.size(), false);
            std::vector<bool> matched_detection(detections.size(), false);
            std::vector<matched_track_update> matched_updates;
            matched_updates.reserve(detections.size());
            size_t new_track_count = 0;

            std::vector<track_match_candidate> match_candidates;
            match_candidates.reserve(state.tracks.size() * detections.size());
            for (size_t track_index = 0; track_index < state.tracks.size();
                 ++track_index) {
                for (size_t detection_index = 0;
                     detection_index < detections.size(); ++detection_index) {
                    const auto& detected = detections[detection_index];
                    const auto& prediction = track_predictions[track_index];
                    const double distance = distance_pct(
                        prediction.center_pct, detected.center_pct
                    );
                    if (distance > prediction.gate_radius_pct) {
                        continue;
                    }

                    const auto& track = state.tracks[track_index];
                    const double normalized_distance
                        = prediction.gate_radius_pct > 0.0
                        ? distance / prediction.gate_radius_pct
                        : distance;
                    const double area_cost
                        = normalized_area_delta(
                              track.area_px2, detected.area_px2
                          )
                        * static_cast<double>(area_match_weight_pct) / 100.0;
                    const double stale_cost
                        = static_cast<double>(track.missed_frames) * 0.05;
                    const double age_bonus
                        = std::min(static_cast<double>(track.age_frames), 10.0)
                        * 0.003;
                    const double confidence_bonus
                        = std::clamp(detected.confidence, 0.0, 1.0) * 0.02;
                    match_candidates.push_back(
                        track_match_candidate {
                            .track_index = track_index,
                            .detection_index = detection_index,
                            .distance_pct = distance,
                            .gate_radius_pct = prediction.gate_radius_pct,
                            .assignment_cost = normalized_distance + area_cost
                                + stale_cost - age_bonus - confidence_bonus,
                        }
                    );
                }
            }

            std::sort(
                match_candidates.begin(), match_candidates.end(),
                [&state, &detections](
                    const track_match_candidate& lhs,
                    const track_match_candidate& rhs
                ) {
                    if (lhs.assignment_cost != rhs.assignment_cost) {
                        return lhs.assignment_cost < rhs.assignment_cost;
                    }
                    if (lhs.distance_pct != rhs.distance_pct) {
                        return lhs.distance_pct < rhs.distance_pct;
                    }

                    const auto& lhs_track = state.tracks[lhs.track_index];
                    const auto& rhs_track = state.tracks[rhs.track_index];
                    if (lhs_track.missed_frames != rhs_track.missed_frames) {
                        return lhs_track.missed_frames
                            < rhs_track.missed_frames;
                    }
                    if (lhs_track.age_frames != rhs_track.age_frames) {
                        return lhs_track.age_frames > rhs_track.age_frames;
                    }

                    return detections[lhs.detection_index].area_px2
                        > detections[rhs.detection_index].area_px2;
                }
            );

            for (const auto& candidate : match_candidates) {
                if (matched_track[candidate.track_index]
                    || matched_detection[candidate.detection_index]) {
                    continue;
                }

                const auto& detected = detections[candidate.detection_index];
                auto& track = state.tracks[candidate.track_index];
                const point previous_center = track.center_pct;
                matched_updates.push_back(
                    matched_track_update {
                        .track_index = candidate.track_index,
                        .previous_center = previous_center,
                        .current_center = detected.center_pct,
                        .age_frames = track.age_frames + 1,
                        .area_px2 = detected.area_px2,
                    }
                );

                track.center_pct = detected.center_pct;
                track.velocity_pct = smoothed_velocity(
                    track.velocity_pct,
                    point {
                        .x = detected.center_pct.x - previous_center.x,
                        .y = detected.center_pct.y - previous_center.y,
                    }
                );
                track.area_px2 = detected.area_px2;
                track.age_frames += 1;
                track.missed_frames = 0;
                track.history_pct.push_back(detected.center_pct);
                while (track.history_pct.size()
                       > static_cast<size_t>(track_history_limit)) {
                    track.history_pct.erase(track.history_pct.begin());
                }
                matched_track[candidate.track_index] = true;
                matched_detection[candidate.detection_index] = true;
            }

            for (size_t detection_index = 0;
                 detection_index < detections.size(); ++detection_index) {
                if (matched_detection[detection_index]) {
                    continue;
                }

                const auto& detected = detections[detection_index];
                tracked_object track;
                track.track_id = state.next_track_id++;
                track.center_pct = detected.center_pct;
                track.history_pct.push_back(detected.center_pct);
                track.area_px2 = detected.area_px2;
                state.tracks.push_back(std::move(track));
                matched_track.push_back(true);
                new_track_count += 1;
            }

            for (size_t index = 0; index < state.tracks.size(); ++index) {
                if (matched_track[index]) {
                    continue;
                }
                state.tracks[index].missed_frames += 1;
                state.tracks[index].velocity_pct
                    = scaled_velocity(state.tracks[index].velocity_pct, 0.85);
            }

            for (const auto& update : matched_updates) {
                if (update.track_index >= state.tracks.size()
                    || update.age_frames < min_track_age_frames) {
                    continue;
                }

                emit_tripwire_events(
                    result.events, stream_value,
                    state.tracks[update.track_index], update.previous_center,
                    update.current_center, frame_value.ts, update.age_frames,
                    update.area_px2
                );
            }

            bool allow_emit = true;
            if (emit_interval_ms > 0
                && state.last_emit.time_since_epoch().count() != 0) {
                const auto elapsed
                    = std::chrono::duration_cast<std::chrono::milliseconds>(
                          frame_value.ts - state.last_emit
                    )
                          .count();
                allow_emit = elapsed >= emit_interval_ms;
            }

            if (allow_emit) {
                auto best_it = std::max_element(
                    state.tracks.begin(), state.tracks.end(),
                    [min_track_age_frames](
                        const tracked_object& lhs, const tracked_object& rhs
                    ) {
                        const bool lhs_stable = lhs.missed_frames == 0
                            && lhs.age_frames >= min_track_age_frames;
                        const bool rhs_stable = rhs.missed_frames == 0
                            && rhs.age_frames >= min_track_age_frames;
                        if (lhs_stable != rhs_stable) {
                            return !lhs_stable;
                        }
                        if (lhs.age_frames != rhs.age_frames) {
                            return lhs.age_frames < rhs.age_frames;
                        }
                        return lhs.area_px2 < rhs.area_px2;
                    }
                );
                if (best_it != state.tracks.end() && best_it->missed_frames == 0
                    && best_it->age_frames >= min_track_age_frames) {
                    event event_value;
                    event_value.kind = event_kind::motion;
                    event_value.stream_name = stream_value.get_name();
                    event_value.message = "centroid_track_motion";
                    event_value.ts = frame_value.ts;
                    event_value.pos_pct = best_it->center_pct;
                    result.events.push_back(event_value);
                    state.last_emit = frame_value.ts;
                }
            }

            state.tracks.erase(
                std::remove_if(
                    state.tracks.begin(), state.tracks.end(),
                    [max_track_gap_frames](const tracked_object& track) {
                        return track.missed_frames > max_track_gap_frames;
                    }
                ),
                state.tracks.end()
            );

            std::sort(
                state.tracks.begin(), state.tracks.end(),
                [](const tracked_object& lhs, const tracked_object& rhs) {
                    if (lhs.missed_frames != rhs.missed_frames) {
                        return lhs.missed_frames < rhs.missed_frames;
                    }
                    if (lhs.age_frames != rhs.age_frames) {
                        return lhs.age_frames > rhs.age_frames;
                    }
                    return lhs.area_px2 > rhs.area_px2;
                }
            );

            const auto stable_track_count = static_cast<size_t>(std::count_if(
                state.tracks.begin(), state.tracks.end(),
                [min_track_age_frames](const tracked_object& track) {
                    return track.missed_frames == 0
                        && track.age_frames >= min_track_age_frames;
                }
            ));
            const int largest_track_age_frames = state.tracks.empty()
                ? 0
                : std::max_element(
                      state.tracks.begin(), state.tracks.end(),
                      [](const tracked_object& lhs, const tracked_object& rhs) {
                          return lhs.age_frames < rhs.age_frames;
                      }
                  )->age_frames;

            add_processing_metric(
                result, "track_count", static_cast<double>(state.tracks.size()),
                "tracks"
            );
            add_processing_metric(
                result, "stable_track_count",
                static_cast<double>(stable_track_count), "tracks"
            );
            add_processing_metric(
                result, "matched_track_count",
                static_cast<double>(matched_updates.size()), "tracks"
            );
            add_processing_metric(
                result, "new_track_count", static_cast<double>(new_track_count),
                "tracks"
            );
            add_processing_metric(
                result, "track_association_candidate_count",
                static_cast<double>(match_candidates.size()), "matches"
            );
            add_processing_metric(
                result, "track_average_match_gate_pct",
                average_gate_radius(track_predictions), "pct"
            );
            add_processing_metric(
                result, "largest_track_age_frames",
                static_cast<double>(largest_track_age_frames), "frames"
            );
            add_processing_metric(
                result, "motion_focus_shape_count",
                static_cast<double>(focus.shape_count), "shapes"
            );
            add_processing_metric(
                result, "sparse_flow_vector_count",
                static_cast<double>(sparse_flow.vectors.size()), "vectors"
            );
            add_processing_metric(
                result, "sparse_flow_average_distance_pct",
                sparse_flow.average_distance_pct, "pct"
            );
            add_processing_metric(
                result, "sparse_flow_predicted_track_count",
                static_cast<double>(predicted_track_count(track_predictions)),
                "tracks"
            );
            add_processing_metric(
                result, "velocity_predicted_track_count",
                static_cast<double>(
                    velocity_predicted_track_count(track_predictions)
                ),
                "tracks"
            );

            const size_t flow_overlay_count = std::min(
                sparse_flow.vectors.size(),
                static_cast<size_t>(max_sparse_flow_overlays)
            );
            for (size_t index = 0; index < flow_overlay_count; ++index) {
                const auto& vector = sparse_flow.vectors[index];
                result.overlays.push_back(make_polyline_overlay(
                    index == 0 ? "flow_vector_primary" : "flow_vector",
                    std::vector<point> { vector.from_pct, vector.to_pct },
                    vector.to_pct
                ));
            }
            if (sparse_flow.average_from_pct.has_value()
                && sparse_flow.average_to_pct.has_value()) {
                result.overlays.push_back(make_polyline_overlay(
                    "flow_average",
                    std::vector<point> {
                        *sparse_flow.average_from_pct,
                        *sparse_flow.average_to_pct,
                    },
                    *sparse_flow.average_to_pct
                ));
            }

            const size_t overlay_count = std::min(
                state.tracks.size(), static_cast<size_t>(max_overlays)
            );
            for (size_t index = 0; index < overlay_count; ++index) {
                const auto& track = state.tracks[index];
                if (track.missed_frames > 0) {
                    continue;
                }

                if (track.history_pct.size() >= 2) {
                    result.overlays.push_back(make_polyline_overlay(
                        index == 0 ? "track_path" : "track", track.history_pct,
                        track.center_pct
                    ));
                }

                result.overlays.push_back(make_point_overlay(
                    index == 0 ? "track_head" : "track_center", track.center_pct
                ));
            }

            add_processing_diagnostic(
                result, "tracking_state",
                detections.empty()
                    ? "no_detections"
                    : (matched_updates.empty() ? "new_tracks_only" : "tracking")
            );
            add_processing_diagnostic(result, "frame_state", "tracked");
            add_processing_diagnostic(
                result, "background_model",
                processing_background_model_kind_id(background_model)
            );
            add_processing_diagnostic(
                result, "motion_focus",
                processing_motion_focus_mode_id(motion_focus_mode)
            );
            add_processing_diagnostic(
                result, "sparse_flow",
                sparse_flow_enabled ? "enabled" : "disabled"
            );

            state.previous_gray = gray.clone();
            {
                std::scoped_lock lock(mtx_);
                state_by_stream_[stream_value.get_name()] = std::move(state);
            }

            return result;
        }

    private:
        struct tracked_object {
            int track_id { 0 };
            point center_pct;
            std::vector<point> history_pct;
            int age_frames { 1 };
            int missed_frames { 0 };
            double area_px2 { 0.0 };
            point velocity_pct;
            std::vector<std::string> crossed_tripwire_keys;
        };

        struct tracking_state {
            cv::Mat previous_gray;
            std::vector<tracked_object> tracks;
            int next_track_id { 1 };
            std::chrono::steady_clock::time_point last_emit {};
        };

        struct detection {
            point center_pct;
            double area_px2 { 0.0 };
            double confidence { 1.0 };
            std::optional<std::string> class_id;
        };

        struct matched_track_update {
            size_t track_index { 0 };
            point previous_center;
            point current_center;
            int age_frames { 0 };
            double area_px2 { 0.0 };
        };

        struct track_match_candidate {
            size_t track_index { 0 };
            size_t detection_index { 0 };
            double distance_pct { 0.0 };
            double gate_radius_pct { 0.0 };
            double assignment_cost { 0.0 };
        };

        struct track_prediction {
            point center_pct;
            size_t flow_count { 0 };
            bool velocity_projected { false };
            double gate_radius_pct { 0.0 };
        };

        static double normalized_area_delta(
            const double lhs_area_px2, const double rhs_area_px2
        ) {
            const double scale = std::max({ lhs_area_px2, rhs_area_px2, 1.0 });
            return std::abs(lhs_area_px2 - rhs_area_px2) / scale;
        }

        static bool meaningful_velocity(const point& velocity_pct) {
            return std::abs(velocity_pct.x) > point::epsilon
                || std::abs(velocity_pct.y) > point::epsilon;
        }

        static point
        scaled_velocity(const point& velocity_pct, const double scale) {
            return point {
                .x = static_cast<float>(
                    static_cast<double>(velocity_pct.x) * scale
                ),
                .y = static_cast<float>(
                    static_cast<double>(velocity_pct.y) * scale
                ),
            };
        }

        static point clamped_prediction(
            const point& center_pct, const point& velocity_pct,
            const double horizon
        ) {
            return point {
                .x = static_cast<float>(std::clamp(
                    static_cast<double>(center_pct.x)
                        + static_cast<double>(velocity_pct.x) * horizon,
                    0.0, 100.0
                )),
                .y = static_cast<float>(std::clamp(
                    static_cast<double>(center_pct.y)
                        + static_cast<double>(velocity_pct.y) * horizon,
                    0.0, 100.0
                )),
            };
        }

        static point inferred_track_velocity(const tracked_object& track) {
            if (meaningful_velocity(track.velocity_pct)) {
                return track.velocity_pct;
            }
            if (track.history_pct.size() < 2) {
                return {};
            }

            const point& previous
                = track.history_pct[track.history_pct.size() - 2];
            const point& current = track.history_pct.back();
            return point {
                .x = current.x - previous.x,
                .y = current.y - previous.y,
            };
        }

        static point smoothed_velocity(
            const point& previous_velocity_pct,
            const point& observed_velocity_pct
        ) {
            if (!meaningful_velocity(previous_velocity_pct)) {
                return observed_velocity_pct;
            }

            constexpr double previous_weight = 0.55;
            constexpr double observed_weight = 1.0 - previous_weight;
            return point {
                .x = static_cast<float>(
                    static_cast<double>(previous_velocity_pct.x)
                        * previous_weight
                    + static_cast<double>(observed_velocity_pct.x)
                        * observed_weight
                ),
                .y = static_cast<float>(
                    static_cast<double>(previous_velocity_pct.y)
                        * previous_weight
                    + static_cast<double>(observed_velocity_pct.y)
                        * observed_weight
                ),
            };
        }

        static std::vector<track_prediction> predict_tracks(
            const std::vector<tracked_object>& tracks,
            const processing_sparse_flow_result& sparse_flow,
            const double sparse_flow_radius_pct,
            const double velocity_weight_pct,
            const double base_match_radius_pct,
            const double gap_radius_growth_pct
        ) {
            std::vector<track_prediction> predictions;
            predictions.reserve(tracks.size());

            for (const auto& track : tracks) {
                double dx = 0.0;
                double dy = 0.0;
                size_t flow_count = 0;

                for (const auto& vector : sparse_flow.vectors) {
                    if (distance_pct(track.center_pct, vector.from_pct)
                        > sparse_flow_radius_pct) {
                        continue;
                    }

                    dx += static_cast<double>(
                        vector.to_pct.x - vector.from_pct.x
                    );
                    dy += static_cast<double>(
                        vector.to_pct.y - vector.from_pct.y
                    );
                    flow_count += 1;
                }

                const point velocity_pct = inferred_track_velocity(track);
                const bool velocity_projected
                    = meaningful_velocity(velocity_pct);
                const double horizon
                    = static_cast<double>(std::min(track.missed_frames + 1, 4));
                point predicted = velocity_projected
                    ? clamped_prediction(
                          track.center_pct, velocity_pct, horizon
                      )
                    : track.center_pct;

                if (flow_count > 0) {
                    const point flow_predicted = clamped_prediction(
                        track.center_pct,
                        point {
                            .x = static_cast<float>(
                                dx / static_cast<double>(flow_count)
                            ),
                            .y = static_cast<float>(
                                dy / static_cast<double>(flow_count)
                            ),
                        },
                        1.0
                    );
                    if (velocity_projected) {
                        const double velocity_weight
                            = std::clamp(velocity_weight_pct, 0.0, 100.0)
                            / 100.0;
                        const double flow_weight = 1.0 - velocity_weight;
                        predicted = point {
                            .x = static_cast<float>(
                                static_cast<double>(predicted.x)
                                    * velocity_weight
                                + static_cast<double>(flow_predicted.x)
                                    * flow_weight
                            ),
                            .y = static_cast<float>(
                                static_cast<double>(predicted.y)
                                    * velocity_weight
                                + static_cast<double>(flow_predicted.y)
                                    * flow_weight
                            ),
                        };
                    } else {
                        predicted = flow_predicted;
                    }
                }

                const double gap_multiplier = 1.0
                    + static_cast<double>(std::max(track.missed_frames, 0))
                        * std::clamp(gap_radius_growth_pct, 0.0, 200.0) / 100.0;
                const double gate_radius_pct = std::clamp(
                    base_match_radius_pct * gap_multiplier, 1.0, 100.0
                );
                predictions.push_back(
                    track_prediction {
                        .center_pct = predicted,
                        .flow_count = flow_count,
                        .velocity_projected = velocity_projected,
                        .gate_radius_pct = gate_radius_pct,
                    }
                );
            }

            return predictions;
        }

        static size_t predicted_track_count(
            const std::vector<track_prediction>& predictions
        ) {
            return static_cast<size_t>(std::count_if(
                predictions.begin(), predictions.end(),
                [](const track_prediction& prediction) {
                    return prediction.flow_count > 0;
                }
            ));
        }

        static size_t velocity_predicted_track_count(
            const std::vector<track_prediction>& predictions
        ) {
            return static_cast<size_t>(std::count_if(
                predictions.begin(), predictions.end(),
                [](const track_prediction& prediction) {
                    return prediction.velocity_projected;
                }
            ));
        }

        static double
        average_gate_radius(const std::vector<track_prediction>& predictions) {
            if (predictions.empty()) {
                return 0.0;
            }

            double total = 0.0;
            for (const auto& prediction : predictions) {
                total += prediction.gate_radius_pct;
            }
            return total / static_cast<double>(predictions.size());
        }

        static bool
        has_tripwire_key(const tracked_object& track, const std::string& key) {
            return std::find(
                       track.crossed_tripwire_keys.begin(),
                       track.crossed_tripwire_keys.end(), key
                   )
                != track.crossed_tripwire_keys.end();
        }

        static void
        record_tripwire_key(tracked_object& track, std::string key) {
            if (!has_tripwire_key(track, key)) {
                track.crossed_tripwire_keys.push_back(std::move(key));
            }
        }

        static void emit_tripwire_events(
            std::vector<event>& events, const stream& stream_value,
            tracked_object& track, const point& previous_center,
            const point& current_center,
            const std::chrono::steady_clock::time_point timestamp,
            const int age_frames, const double area_px2
        ) {
            if (distance_pct(previous_center, current_center)
                <= point::epsilon) {
                return;
            }

            for (const auto& crossing : tripwire_crossings_for_motion(
                     stream_value, previous_center, current_center
                 )) {
                const std::string key = tripwire_crossing_key(crossing);
                if (has_tripwire_key(track, key)) {
                    continue;
                }
                record_tripwire_key(track, key);

                const double strength = std::clamp(
                    static_cast<double>(age_frames) / 4.0 + area_px2 / 8000.0,
                    0.45, 1.0
                );
                const double speed = std::clamp(
                    distance_pct(previous_center, current_center) / 6.0, 0.5,
                    4.0
                );

                event event_value;
                event_value.kind = event_kind::tripwire;
                event_value.stream_name = stream_value.get_name();
                event_value.line_name = crossing.line_name;
                event_value.ts = timestamp;
                event_value.pos_pct = crossing.position_pct;
                event_value.message = crossing.direction + "|"
                    + std::to_string(strength) + "|" + std::to_string(speed)
                    + "|track=" + std::to_string(track.track_id);
                events.push_back(std::move(event_value));
            }
        }

        processing_background_model_store background_models_;
        processing_algorithm_configuration configuration_;
        std::unordered_map<std::string, tracking_state> state_by_stream_;
        std::mutex mtx_;
    };

} // namespace

std::unique_ptr<processing_algorithm> make_centroid_track_algorithm() {
    return std::make_unique<centroid_track_algorithm>();
}

} // namespace yodau::core

#endif
