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
#include "analysis/processing_sparse_flow.hpp"
#include "analysis/processing_tripwire_tools.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
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
            = config_int(configuration_, "diff_threshold", 24, 1, 255);
        const int blur_kernel
            = config_int(configuration_, "blur_kernel", 5, 1, 31);
        const int morph_kernel
            = config_int(configuration_, "morph_kernel", 5, 1, 31);
        const int min_contour_area
            = config_int(configuration_, "min_contour_area", 96, 1, 1000000);
        const int match_radius_pct
            = config_int(configuration_, "match_radius_pct", 12, 1, 50);
        const int min_track_age_frames = config_int(
            configuration_, "min_track_age_frames", 2, 1, 120
        );
        const int max_track_gap_frames = config_int(
            configuration_, "max_track_gap_frames", 3, 0, 120
        );
        const int track_history_limit = config_int(
            configuration_, "track_history_limit", 10, 2, 64
        );
        const int max_overlays
            = config_int(configuration_, "max_overlays", 4, 1, 8);
        const int emit_interval_ms
            = config_int(configuration_, "emit_interval_ms", 120, 0, 5000);
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
        const bool sparse_flow_enabled
            = config_int(configuration_, "sparse_flow_enabled", 1, 0, 1) != 0;
        const int sparse_flow_max_features = config_int(
            configuration_, "sparse_flow_max_features", 80, 1, 1000
        );
        const int sparse_flow_quality_permille = config_int(
            configuration_, "sparse_flow_quality_permille", 12, 1, 1000
        );
        const int sparse_flow_min_feature_distance_px = config_int(
            configuration_, "sparse_flow_min_feature_distance_px", 7, 1, 512
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
        const int max_sparse_flow_overlays = config_int(
            configuration_, "max_sparse_flow_overlays", 8, 0, 64
        );

        tracking_state state;
        {
            std::scoped_lock lock(mtx_);
            if (const auto it = state_by_stream_.find(stream_value.get_name());
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

            result.metrics.push_back(
                processing_metric {
                    .name = "track_count",
                    .value = 0.0,
                    .unit = "tracks",
                }
            );
            result.metrics.push_back(
                processing_metric {
                    .name = "stable_track_count",
                    .value = 0.0,
                    .unit = "tracks",
                }
            );
            result.diagnostics.push_back(
                processing_diagnostic {
                    .key = "frame_state",
                    .value = "warmup",
                }
            );
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

        processing_sparse_flow_result sparse_flow;
        if (sparse_flow_enabled) {
            sparse_flow = sparse_optical_flow(
                state.previous_gray, gray, binary_mask,
                processing_sparse_flow_options {
                    .max_features = sparse_flow_max_features,
                    .quality_permille = sparse_flow_quality_permille,
                    .min_feature_distance_px = sparse_flow_min_feature_distance_px,
                    .window_size_px = sparse_flow_window_px,
                    .pyramid_levels = sparse_flow_pyramid_levels,
                    .max_error = sparse_flow_max_error,
                    .min_vector_length_px = sparse_flow_min_vector_px,
                }
            );
        }
        const auto track_predictions = predict_tracks_with_sparse_flow(
            state.tracks, sparse_flow,
            static_cast<double>(sparse_flow_prediction_radius_pct)
        );

        const auto contour_candidates = contour_candidates_by_area(
            binary_mask, gray.size(), static_cast<double>(min_contour_area)
        );
        std::vector<detection> detections;
        detections.reserve(contour_candidates.size());
        for (const auto& candidate : contour_candidates) {
            detections.push_back(
                detection {
                    .center_pct = candidate.center_pct,
                    .area_px2 = candidate.area_px2,
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
            for (size_t detection_index = 0; detection_index < detections.size();
                 ++detection_index) {
                const auto& detected = detections[detection_index];
                const double distance = distance_pct(
                    track_predictions[track_index].center_pct,
                    detected.center_pct
                );
                if (distance > static_cast<double>(match_radius_pct)) {
                    continue;
                }

                match_candidates.push_back(
                    track_match_candidate {
                        .track_index = track_index,
                        .detection_index = detection_index,
                        .distance_pct = distance,
                    }
                );
            }
        }

        std::sort(
            match_candidates.begin(), match_candidates.end(),
            [&state, &detections](const track_match_candidate& lhs,
                                  const track_match_candidate& rhs) {
                if (lhs.distance_pct != rhs.distance_pct) {
                    return lhs.distance_pct < rhs.distance_pct;
                }

                const auto& lhs_track = state.tracks[lhs.track_index];
                const auto& rhs_track = state.tracks[rhs.track_index];
                if (lhs_track.missed_frames != rhs_track.missed_frames) {
                    return lhs_track.missed_frames < rhs_track.missed_frames;
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
            matched_updates.push_back(
                matched_track_update {
                    .track_index = candidate.track_index,
                    .previous_center = track.center_pct,
                    .current_center = detected.center_pct,
                    .age_frames = track.age_frames + 1,
                    .area_px2 = detected.area_px2,
                }
            );

            track.center_pct = detected.center_pct;
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

        for (size_t detection_index = 0; detection_index < detections.size();
             ++detection_index) {
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
        }

        for (const auto& update : matched_updates) {
            if (update.track_index >= state.tracks.size()
                || update.age_frames < min_track_age_frames) {
                continue;
            }

            emit_tripwire_events(
                result.events, stream_value, state.tracks[update.track_index],
                update.previous_center, update.current_center, frame_value.ts,
                update.age_frames, update.area_px2
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
                [min_track_age_frames](const tracked_object& lhs,
                                       const tracked_object& rhs) {
                    const bool lhs_stable
                        = lhs.missed_frames == 0
                        && lhs.age_frames >= min_track_age_frames;
                    const bool rhs_stable
                        = rhs.missed_frames == 0
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

        const size_t stable_track_count = static_cast<size_t>(std::count_if(
            state.tracks.begin(), state.tracks.end(),
            [min_track_age_frames](const tracked_object& track) {
                return track.missed_frames == 0
                    && track.age_frames >= min_track_age_frames;
            }
        ));
        const int largest_track_age_frames
            = state.tracks.empty()
            ? 0
            : std::max_element(
                  state.tracks.begin(), state.tracks.end(),
                  [](const tracked_object& lhs, const tracked_object& rhs) {
                      return lhs.age_frames < rhs.age_frames;
                  }
              )
                  ->age_frames;

        result.metrics.push_back(
            processing_metric {
                .name = "track_count",
                .value = static_cast<double>(state.tracks.size()),
                .unit = "tracks",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "stable_track_count",
                .value = static_cast<double>(stable_track_count),
                .unit = "tracks",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "matched_track_count",
                .value = static_cast<double>(matched_updates.size()),
                .unit = "tracks",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "new_track_count",
                .value = static_cast<double>(new_track_count),
                .unit = "tracks",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "track_association_candidate_count",
                .value = static_cast<double>(match_candidates.size()),
                .unit = "matches",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "largest_track_age_frames",
                .value = static_cast<double>(largest_track_age_frames),
                .unit = "frames",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "motion_focus_shape_count",
                .value = static_cast<double>(focus.shape_count),
                .unit = "shapes",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "sparse_flow_vector_count",
                .value = static_cast<double>(sparse_flow.vectors.size()),
                .unit = "vectors",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "sparse_flow_average_distance_pct",
                .value = sparse_flow.average_distance_pct,
                .unit = "pct",
            }
        );
        result.metrics.push_back(
            processing_metric {
                .name = "sparse_flow_predicted_track_count",
                .value = static_cast<double>(
                    predicted_track_count(track_predictions)
                ),
                .unit = "tracks",
            }
        );

        const size_t flow_overlay_count = std::min(
            sparse_flow.vectors.size(),
            static_cast<size_t>(max_sparse_flow_overlays)
        );
        for (size_t index = 0; index < flow_overlay_count; ++index) {
            const auto& vector = sparse_flow.vectors[index];
            result.overlays.push_back(
                make_polyline_overlay(
                    index == 0 ? "flow_vector_primary" : "flow_vector",
                    std::vector<point> { vector.from_pct, vector.to_pct },
                    vector.to_pct
                )
            );
        }
        if (sparse_flow.average_from_pct.has_value()
            && sparse_flow.average_to_pct.has_value()) {
            result.overlays.push_back(
                make_polyline_overlay(
                    "flow_average",
                    std::vector<point> {
                        *sparse_flow.average_from_pct,
                        *sparse_flow.average_to_pct,
                    },
                    *sparse_flow.average_to_pct
                )
            );
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
                result.overlays.push_back(
                    make_polyline_overlay(
                        index == 0 ? "track_path" : "track",
                        track.history_pct, track.center_pct
                    )
                );
            }

            result.overlays.push_back(
                make_point_overlay(
                    index == 0 ? "track_head" : "track_center",
                    track.center_pct
                )
            );
        }

        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "tracking_state",
                .value = detections.empty()
                    ? "no_detections"
                    : (matched_updates.empty() ? "new_tracks_only"
                                               : "tracking"),
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "frame_state",
                .value = "tracked",
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "background_model",
                .value = processing_background_model_kind_id(background_model),
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "motion_focus",
                .value = processing_motion_focus_mode_id(motion_focus_mode),
            }
        );
        result.diagnostics.push_back(
            processing_diagnostic {
                .key = "sparse_flow",
                .value = sparse_flow_enabled ? "enabled" : "disabled",
            }
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
    };

    struct track_prediction {
        point center_pct;
        size_t flow_count { 0 };
    };

    static std::vector<track_prediction> predict_tracks_with_sparse_flow(
        const std::vector<tracked_object>& tracks,
        const processing_sparse_flow_result& sparse_flow,
        const double prediction_radius_pct
    ) {
        std::vector<track_prediction> predictions;
        predictions.reserve(tracks.size());

        for (const auto& track : tracks) {
            double dx = 0.0;
            double dy = 0.0;
            size_t flow_count = 0;

            for (const auto& vector : sparse_flow.vectors) {
                if (distance_pct(track.center_pct, vector.from_pct)
                    > prediction_radius_pct) {
                    continue;
                }

                dx += static_cast<double>(vector.to_pct.x - vector.from_pct.x);
                dy += static_cast<double>(vector.to_pct.y - vector.from_pct.y);
                flow_count += 1;
            }

            point predicted = track.center_pct;
            if (flow_count > 0) {
                predicted.x = static_cast<float>(std::clamp(
                    static_cast<double>(track.center_pct.x)
                        + dx / static_cast<double>(flow_count),
                    0.0, 100.0
                ));
                predicted.y = static_cast<float>(std::clamp(
                    static_cast<double>(track.center_pct.y)
                        + dy / static_cast<double>(flow_count),
                    0.0, 100.0
                ));
            }

            predictions.push_back(
                track_prediction {
                    .center_pct = predicted,
                    .flow_count = flow_count,
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

    static bool has_tripwire_key(
        const tracked_object& track, const std::string& key
    ) {
        return std::find(
                   track.crossed_tripwire_keys.begin(),
                   track.crossed_tripwire_keys.end(), key
               )
            != track.crossed_tripwire_keys.end();
    }

    static void record_tripwire_key(tracked_object& track, std::string key) {
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
        if (distance_pct(previous_center, current_center) <= point::epsilon) {
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
                distance_pct(previous_center, current_center) / 6.0,
                0.5, 4.0
            );

            event event_value;
            event_value.kind = event_kind::tripwire;
            event_value.stream_name = stream_value.get_name();
            event_value.line_name = crossing.line_name;
            event_value.ts = timestamp;
            event_value.pos_pct = crossing.position_pct;
            event_value.message = crossing.direction + "|"
                + std::to_string(strength) + "|" + std::to_string(speed)
                + "|track="
                + std::to_string(track.track_id);
            events.push_back(std::move(event_value));
        }
    }

    opencv_client daemon_client_;
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
