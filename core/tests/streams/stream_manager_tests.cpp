#include "analysis/default_processing_algorithms.hpp"
#include "analysis/default_processing_hooks.hpp"
#include "analysis/opencv_client.hpp"
#include "analysis/processing_algorithm.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_focus.hpp"
#include "analysis/processing_runtime.hpp"
#include "analysis/processing_session_store.hpp"
#include "analysis/tripwire_grid_stream_index.hpp"
#include "core/namespace_alias.hpp"
#include "geometry/coords.hpp"
#include "geometry/geometry.hpp"
#include "streams/analysis_scheduler.hpp"
#include "streams/stream_manager.hpp"
#include "streams/virtual_camera.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef YODAU_OPENCV
#include <opencv2/videoio.hpp>
#endif

using yodau::core::analysis_scheduler;
using yodau::core::build_grid_stream_index;
using yodau::core::collect_grid_candidates;
using yodau::core::default_processing_algorithm_id;
using yodau::core::default_processing_algorithm_registry;
using yodau::core::grid_candidate_tracker;
using yodau::core::grid_dims;
using yodau::core::grid_index;
using yodau::core::line_profile;
using yodau::core::line_ptr;
using yodau::core::make_line;
using yodau::core::make_line_profile;
using yodau::core::point;
using yodau::core::processing_algorithm;
using yodau::core::processing_algorithm_configuration;
using yodau::core::processing_algorithm_registry;
using yodau::core::processing_metric;
using yodau::core::processing_result;
using yodau::core::processing_runtime;
using yodau::core::processing_runtime_options;
using yodau::core::processing_session_store;
using yodau::core::recommend_grid_dims;
using yodau::core::render_mode;
using yodau::core::stream;
using yodau::core::stream_manager;
using yodau::core::trace_grid_cells_pct;
using yodau::core::virtual_camera;
using yodau::core::virtual_camera_frame_info;

namespace stream_manager_tests_support {

struct processed_frame_probe {
    size_t call_count { 0 };
    std::string last_stream_name;
    int last_frame_width { 0 };
    size_t last_event_count { 0 };

    [[nodiscard]] static std::vector<yodau::core::event>
    process(const stream& stream_value, const yodau::core::frame& frame_value) {
        yodau::core::event event_value;
        event_value.kind = yodau::core::event_kind::motion;
        event_value.stream_name = stream_value.get_name();
        event_value.ts = frame_value.ts;
        event_value.pos_pct = point { 25.0f, 50.0f };
        return { event_value };
    }

    void on_processed_frame(
        const stream& stream_value, const yodau::core::frame& frame_value,
        const std::vector<yodau::core::event>& events
    ) {
        call_count += 1;
        last_stream_name = stream_value.get_name();
        last_frame_width = frame_value.width;
        last_event_count = events.size();
    }
};

class dummy_processing_algorithm final : public processing_algorithm {
public:
    [[nodiscard]] std::string algorithm_id() const override { return "dummy"; }

    [[nodiscard]] std::string display_name() const override {
        return "dummy algorithm";
    }

    [[nodiscard]] processing_algorithm_configuration
    default_configuration() const override {
        processing_algorithm_configuration configuration;
        configuration.values.emplace("mode", "dummy");
        return configuration;
    }

    [[nodiscard]] processing_algorithm_configuration
    configuration() const override {
        return configuration_;
    }

    void configure(processing_algorithm_configuration configuration) override {
        configuration_ = std::move(configuration);
    }

    void daemon_start(
        const stream& stream_value,
        const std::function<void(yodau::core::frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        (void)stream_value;
        (void)on_frame;
        (void)stop_token;
    }

    processing_result process_frame(
        const stream& stream_value, const yodau::core::frame& frame_value
    ) override {
        (void)frame_value;

        processing_result result;
        result.metrics.push_back(
            processing_metric {
                .name = "stream_name_size",
                .value = static_cast<double>(stream_value.get_name().size()),
                .unit = "chars",
            }
        );
        return result;
    }

private:
    processing_algorithm_configuration configuration_ = default_configuration();
};

class named_processing_algorithm final : public processing_algorithm {
public:
    explicit named_processing_algorithm(std::string algorithm_id_value)
        : algorithm_id_value_(std::move(algorithm_id_value)) { }

    [[nodiscard]] std::string algorithm_id() const override {
        return algorithm_id_value_;
    }

    [[nodiscard]] std::string display_name() const override {
        return algorithm_id_value_ + " algorithm";
    }

    void daemon_start(
        const stream& stream_value,
        const std::function<void(yodau::core::frame&&)>& on_frame,
        const std::stop_token& stop_token
    ) override {
        (void)stream_value;
        (void)on_frame;
        (void)stop_token;
    }

    processing_result process_frame(
        const stream& stream_value, const yodau::core::frame& frame_value
    ) override {
        (void)stream_value;
        (void)frame_value;
        return {};
    }

private:
    std::string algorithm_id_value_;
};

std::string
diagnostic_value(const processing_result& result, const std::string& key) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.key == key) {
            return diagnostic.value;
        }
    }

    return {};
}

double metric_value(
    const processing_result& result, const std::string& name,
    const double fallback = std::numeric_limits<double>::quiet_NaN()
) {
    for (const auto& metric : result.metrics) {
        if (metric.name == name) {
            return metric.value;
        }
    }

    return fallback;
}

yodau::core::frame make_gray_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::core::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width;
    frame_value.format = yodau::core::pixel_format::gray8;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height),
        static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            frame_value.data
                [static_cast<size_t>(y) * static_cast<size_t>(width)
                 + static_cast<size_t>(x)] = 255;
        }
    }

    return frame_value;
}

yodau::core::frame make_rgb_frame(
    const int width, const int height,
    const std::chrono::steady_clock::time_point timestamp,
    const int bright_left = -1, const int bright_top = -1,
    const int bright_right = -1, const int bright_bottom = -1
) {
    yodau::core::frame frame_value;
    frame_value.width = width;
    frame_value.height = height;
    frame_value.stride = width * 3;
    frame_value.format = yodau::core::pixel_format::rgb24;
    frame_value.ts = timestamp;
    frame_value.data.assign(
        static_cast<size_t>(frame_value.stride) * static_cast<size_t>(height),
        static_cast<std::uint8_t>(0)
    );

    if (bright_left < 0 || bright_top < 0 || bright_right <= bright_left
        || bright_bottom <= bright_top) {
        return frame_value;
    }

    for (int y = bright_top; y < bright_bottom; ++y) {
        for (int x = bright_left; x < bright_right; ++x) {
            const auto pixel_offset = static_cast<size_t>(y)
                    * static_cast<size_t>(frame_value.stride)
                + static_cast<size_t>(x) * 3U;
            frame_value.data[pixel_offset + 0] = 255;
            frame_value.data[pixel_offset + 1] = 255;
            frame_value.data[pixel_offset + 2] = 255;
        }
    }

    return frame_value;
}

} // namespace stream_manager_tests_support

TEST(line_profile_test, make_line_profile_normalizes_future_semantic_fields) {
    const line_profile profile
        = make_line_profile("north", 4.5f, 0.0f, 0.0f, 2.0f);

    EXPECT_EQ(profile.line_name, "north");
    EXPECT_FLOAT_EQ(profile.visual_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.interaction_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.effective_length, 1.0f);
    EXPECT_FLOAT_EQ(profile.damping, 1.0f);
}

TEST(analysis_scheduler_test, overrides_and_clear_restore_default_interval) {
    analysis_scheduler scheduler;
    scheduler.set_default_interval_ms(200);

    const analysis_scheduler::time_point t0 {};
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 200);
    EXPECT_TRUE(scheduler.should_process("cam0", t0));
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(100))
    );

    scheduler.set_stream_interval_ms("cam0", 50);
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 50);
    EXPECT_TRUE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(100))
    );
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(140))
    );

    scheduler.clear_stream_interval_ms("cam0");
    EXPECT_EQ(scheduler.interval_for_stream("cam0"), 200);
    EXPECT_FALSE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(250))
    );
    EXPECT_TRUE(
        scheduler.should_process("cam0", t0 + std::chrono::milliseconds(300))
    );
}

TEST(processing_algorithm_registry_test, registry_creates_named_algorithm) {
    processing_algorithm_registry registry;

    EXPECT_TRUE(registry.register_algorithm(
        processing_algorithm_registry::entry {
            .algorithm_id = "dummy",
            .display_name = "dummy algorithm",
            .create =
                [] {
                    return std::make_unique<stream_manager_tests_support::
                                                dummy_processing_algorithm>();
                },
        }
    ));
    EXPECT_FALSE(registry.register_algorithm(
        processing_algorithm_registry::entry {
            .algorithm_id = "dummy",
            .display_name = "dummy duplicate",
            .create =
                [] {
                    return std::make_unique<stream_manager_tests_support::
                                                dummy_processing_algorithm>();
                },
        }
    ));

    const std::vector<std::string> ids = registry.algorithm_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids.front(), "dummy");

    auto algorithm = registry.create("dummy");
    ASSERT_TRUE(algorithm);
    EXPECT_EQ(algorithm->algorithm_id(), "dummy");
    EXPECT_EQ(algorithm->display_name(), "dummy algorithm");

    processing_algorithm_configuration configuration;
    configuration.values.emplace("threshold", "7");
    algorithm->configure(configuration);

    EXPECT_EQ(algorithm->configuration().values.at("threshold"), "7");

    const processing_result result = algorithm->process_frame(
        stream("/tmp/cam.mp4", "cam0", "file", true), yodau::core::frame {}
    );
    ASSERT_EQ(result.metrics.size(), 1u);
    EXPECT_EQ(result.metrics.front().name, "stream_name_size");
    EXPECT_EQ(result.metrics.front().unit, "chars");
    EXPECT_DOUBLE_EQ(result.metrics.front().value, 4.0);
}

TEST(
    processing_session_store_test,
    tracks_default_overrides_cached_algorithms_and_latest_results
) {
    processing_session_store store("spot_grid");

    const processing_session_store::legacy_algorithm_factory factory
        = [](const std::string& algorithm_id) {
              return std::make_unique<
                  stream_manager_tests_support::named_processing_algorithm>(
                  algorithm_id
              );
          };

    EXPECT_TRUE(store.processing_enabled());
    EXPECT_EQ(store.default_algorithm_id(), "spot_grid");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-a"), "spot_grid");

    auto default_algorithm
        = store.active_algorithm_for_stream("cam-a", factory);
    ASSERT_TRUE(default_algorithm);
    EXPECT_EQ(default_algorithm->algorithm_id(), "spot_grid");

    processing_result default_result;
    default_result.diagnostics.push_back({ .key = "stream", .value = "cam-a" });
    store.store_latest_processing_result("cam-a", default_result);
    ASSERT_TRUE(store.latest_processing_result("cam-a").has_value());

    auto override_algorithm = std::shared_ptr<processing_algorithm>(
        std::make_unique<
            stream_manager_tests_support::named_processing_algorithm>(
            "contour_mask"
        )
    );
    store.set_stream_algorithm("cam-b", "contour_mask", override_algorithm);
    EXPECT_EQ(store.algorithm_id_for_stream("cam-b"), "contour_mask");
    ASSERT_EQ(store.stream_algorithm_overrides().size(), 1u);
    EXPECT_EQ(store.stream_algorithm_overrides().at("cam-b"), "contour_mask");

    processing_result override_result;
    override_result.diagnostics.push_back(
        { .key = "stream", .value = "cam-b" }
    );
    store.store_latest_processing_result("cam-b", override_result);
    ASSERT_TRUE(store.latest_processing_result("cam-b").has_value());

    store.set_default_algorithm("motion_baseline");
    EXPECT_EQ(store.default_algorithm_id(), "motion_baseline");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-a"), "motion_baseline");
    EXPECT_FALSE(store.latest_processing_result("cam-a").has_value());
    ASSERT_TRUE(store.latest_processing_result("cam-b").has_value());

    auto refreshed_default_algorithm
        = store.active_algorithm_for_stream("cam-a", factory);
    ASSERT_TRUE(refreshed_default_algorithm);
    EXPECT_EQ(refreshed_default_algorithm->algorithm_id(), "motion_baseline");
    EXPECT_NE(refreshed_default_algorithm, default_algorithm);

    store.clear_stream_algorithm("cam-b");
    EXPECT_EQ(store.algorithm_id_for_stream("cam-b"), "motion_baseline");
    EXPECT_TRUE(store.stream_algorithm_overrides().empty());
    EXPECT_FALSE(store.latest_processing_result("cam-b").has_value());
}

TEST(
    portable_motion_baseline_test,
    detects_motion_and_tripwire_crossings_without_external_libraries
) {
    auto algorithm = yodau::core::make_portable_motion_baseline_algorithm();
    ASSERT_TRUE(algorithm);
    EXPECT_EQ(algorithm->algorithm_id(), "motion_baseline");

    stream source("/tmp/mobile.mp4", "mobile", "file", true);
    source.connect_line(
        make_line({ point { 25.0f, 5.0f }, point { 25.0f, 95.0f } }, "entry")
    );

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto left = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 4, 16, 20, 48
    );
    const auto right = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(400), 40, 16, 56, 48
    );

    EXPECT_TRUE(algorithm->process_frame(source, dark).events.empty());
    const processing_result left_result
        = algorithm->process_frame(source, left);
    EXPECT_TRUE(
        std::ranges::any_of(
            left_result.events, [](const yodau::core::event& event_value) {
                return event_value.kind == yodau::core::event_kind::motion;
            }
        )
    );

    const processing_result crossing_result
        = algorithm->process_frame(source, right);
    EXPECT_TRUE(
        std::ranges::any_of(
            crossing_result.events, [](const yodau::core::event& event_value) {
                return event_value.kind == yodau::core::event_kind::tripwire
                    && event_value.line_name == "entry";
            }
        )
    );
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            crossing_result, "processing_backend"
        ),
        "portable_frame_delta"
    );
    EXPECT_GT(
        stream_manager_tests_support::metric_value(
            crossing_result, "motion_ratio", 0.0
        ),
        0.0
    );
}

#ifdef YODAU_OPENCV
TEST(
    processing_algorithm_registry_test, default_registry_exposes_motion_baseline
) {
    const auto& registry = default_processing_algorithm_registry();

    EXPECT_TRUE(registry.contains(default_processing_algorithm_id()));
    EXPECT_TRUE(registry.contains("spot_grid"));
    EXPECT_TRUE(registry.contains("spot grid"));
    EXPECT_TRUE(registry.contains("contour_mask"));
    EXPECT_TRUE(registry.contains("contour mask"));
    EXPECT_TRUE(registry.contains("centroid_track"));
    EXPECT_TRUE(registry.contains("centroid track"));
    EXPECT_TRUE(registry.contains("hybrid_auto"));

    auto algorithm = registry.create(default_processing_algorithm_id());
    ASSERT_TRUE(algorithm);
    EXPECT_EQ(algorithm->algorithm_id(), "motion_baseline");
    EXPECT_EQ(algorithm->display_name(), "motion baseline");

    auto spot_grid_algorithm = registry.create("spot grid");
    ASSERT_TRUE(spot_grid_algorithm);
    EXPECT_EQ(spot_grid_algorithm->algorithm_id(), "spot_grid");
    EXPECT_EQ(spot_grid_algorithm->display_name(), "spot grid");

    auto contour_mask_algorithm = registry.create("contour mask");
    ASSERT_TRUE(contour_mask_algorithm);
    EXPECT_EQ(contour_mask_algorithm->algorithm_id(), "contour_mask");
    EXPECT_EQ(contour_mask_algorithm->display_name(), "contour mask");

    auto centroid_track_algorithm
        = yodau::core::make_processing_algorithm("track");
    ASSERT_TRUE(centroid_track_algorithm);
    EXPECT_EQ(centroid_track_algorithm->algorithm_id(), "centroid_track");
    EXPECT_EQ(centroid_track_algorithm->display_name(), "centroid track");

    auto hybrid_auto_algorithm
        = yodau::core::make_processing_algorithm("hybrid");
    ASSERT_TRUE(hybrid_auto_algorithm);
    EXPECT_EQ(hybrid_auto_algorithm->algorithm_id(), "hybrid_auto");
    EXPECT_EQ(hybrid_auto_algorithm->display_name(), "hybrid auto");
}

TEST(processing_runtime_test, keeps_default_and_per_stream_algorithm_choices) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = {},
        }
    );

    EXPECT_EQ(runtime.default_algorithm_id(), "motion_baseline");
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-a"), "motion_baseline");
    EXPECT_TRUE(runtime.set_default_algorithm("spot"));
    EXPECT_EQ(runtime.default_algorithm_id(), "spot_grid");
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-a"), "spot_grid");

    EXPECT_TRUE(runtime.set_stream_algorithm("cam-b", "contour"));
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-b"), "contour_mask");

    const auto overrides = runtime.stream_algorithm_overrides();
    ASSERT_EQ(overrides.size(), 1u);
    EXPECT_EQ(overrides.at("cam-b"), "contour_mask");

    EXPECT_TRUE(runtime.clear_stream_algorithm("cam-b"));
    EXPECT_EQ(runtime.algorithm_id_for_stream("cam-b"), "spot_grid");
}

TEST(
    processing_runtime_test, processes_each_stream_with_its_selected_algorithm
) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = "spot_grid",
        }
    );

    ASSERT_TRUE(runtime.set_stream_algorithm("cam-contour", "contour_mask"));

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto bright_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 18, 18, 42, 42
    );

    const stream spot_stream("/tmp/spot.mp4", "cam-spot", "file", true);
    const stream contour_stream(
        "/tmp/contour.mp4", "cam-contour", "file", true
    );

    EXPECT_TRUE(frame_processor(spot_stream, dark_frame).empty());
    EXPECT_FALSE(frame_processor(spot_stream, bright_frame).empty());
    EXPECT_TRUE(frame_processor(contour_stream, dark_frame).empty());
    EXPECT_FALSE(frame_processor(contour_stream, bright_frame).empty());

    const auto spot_result = runtime.latest_processing_result("cam-spot");
    ASSERT_TRUE(spot_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *spot_result, "algorithm"
        ),
        "spot_grid"
    );

    const auto contour_result = runtime.latest_processing_result("cam-contour");
    ASSERT_TRUE(contour_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *contour_result, "algorithm"
        ),
        "contour_mask"
    );
}

TEST(
    processing_runtime_test,
    centroid_track_builds_persistent_tracks_and_emits_motion
) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = "centroid_track",
        }
    );

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    stream tracked_stream("/tmp/tracked.mp4", "cam-tracked", "file", true);
    tracked_stream.connect_line(
        make_line({ point { 50.0f, 10.0f }, point { 50.0f, 90.0f } }, "center")
    );

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto first_motion = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(160), 12, 18, 24, 34
    );
    const auto crossing_motion = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(320), 34, 18, 48, 34
    );

    EXPECT_TRUE(frame_processor(tracked_stream, dark_frame).empty());
    EXPECT_TRUE(frame_processor(tracked_stream, first_motion).empty());
    const auto events = frame_processor(tracked_stream, crossing_motion);

    EXPECT_FALSE(events.empty());
    EXPECT_GE(
        std::count_if(
            events.begin(), events.end(),
            [](const yodau::core::event& event_value) {
                return event_value.kind == yodau::core::event_kind::motion;
            }
        ),
        1
    );

    const auto latest = runtime.latest_processing_result("cam-tracked");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(*latest, "algorithm"),
        "centroid_track"
    );
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *latest, "tracking_state"
        ),
        "tracking"
    );
    EXPECT_GE(
        stream_manager_tests_support::metric_value(*latest, "track_count"), 1.0
    );
    EXPECT_GE(
        stream_manager_tests_support::metric_value(
            *latest, "stable_track_count"
        ),
        1.0
    );
    EXPECT_FALSE(latest->overlays.empty());
}

TEST(
    processing_runtime_test, closed_regions_filter_motion_and_emit_roi_matches
) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = "contour_mask",
        }
    );

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto inside_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 8, 8, 20, 20
    );
    const auto outside_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(400), 44, 44, 60, 60
    );

    const auto region_line = make_line(
        {
            point { 5.0f, 5.0f },
            point { 40.0f, 5.0f },
            point { 40.0f, 40.0f },
            point { 5.0f, 40.0f },
        },
        "zone_a", true
    );

    stream inside_stream("/tmp/inside.mp4", "cam-inside", "file", true);
    inside_stream.connect_line(region_line);
    EXPECT_TRUE(frame_processor(inside_stream, dark_frame).empty());
    const auto inside_events = frame_processor(inside_stream, inside_frame);
    ASSERT_EQ(inside_events.size(), 2u);
    EXPECT_EQ(
        std::count_if(
            inside_events.begin(), inside_events.end(),
            [](const yodau::core::event& event_value) {
                return event_value.kind == yodau::core::event_kind::motion;
            }
        ),
        1
    );
    EXPECT_EQ(
        std::count_if(
            inside_events.begin(), inside_events.end(),
            [](const yodau::core::event& event_value) {
                return event_value.kind == yodau::core::event_kind::roi
                    && event_value.line_name == "zone_a";
            }
        ),
        1
    );

    const auto inside_result = runtime.latest_processing_result("cam-inside");
    ASSERT_TRUE(inside_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *inside_result, "motion_region_filter"
        ),
        "applied"
    );
    EXPECT_DOUBLE_EQ(
        stream_manager_tests_support::metric_value(
            *inside_result, "motion_region_count"
        ),
        1.0
    );
    EXPECT_DOUBLE_EQ(
        stream_manager_tests_support::metric_value(
            *inside_result, "motion_region_roi_event_count"
        ),
        1.0
    );
    EXPECT_FALSE(inside_result->overlays.empty());

    stream outside_stream("/tmp/outside.mp4", "cam-outside", "file", true);
    outside_stream.connect_line(region_line);
    EXPECT_TRUE(frame_processor(outside_stream, dark_frame).empty());
    const auto outside_events = frame_processor(outside_stream, outside_frame);
    EXPECT_TRUE(outside_events.empty());

    const auto outside_result = runtime.latest_processing_result("cam-outside");
    ASSERT_TRUE(outside_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *outside_result, "motion_region_filter"
        ),
        "applied"
    );
    EXPECT_DOUBLE_EQ(
        stream_manager_tests_support::metric_value(
            *outside_result, "motion_region_dropped_event_count"
        ),
        1.0
    );
    EXPECT_DOUBLE_EQ(
        stream_manager_tests_support::metric_value(
            *outside_result, "motion_region_dropped_overlay_count"
        ),
        2.0
    );
    EXPECT_TRUE(outside_result->events.empty());
    EXPECT_TRUE(outside_result->overlays.empty());
}

TEST(processing_runtime_test, hybrid_auto_chooses_delegate_by_scene_shape) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = "hybrid_auto",
        }
    );

    const auto available_algorithm_ids
        = processing_runtime::available_algorithm_ids();
    EXPECT_NE(
        std::find(
            available_algorithm_ids.begin(), available_algorithm_ids.end(),
            std::string("hybrid_auto")
        ),
        available_algorithm_ids.end()
    );

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto calm_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(160)
    );
    const auto tripwire_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(320), 8, 8, 20, 20
    );
    const auto busy_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(480), 16, 16, 48, 48
    );

    stream tripwire_stream("/tmp/tripwire.mp4", "cam-tripwire", "file", true);
    tripwire_stream.connect_line(
        make_line({ point { 10.0f, 50.0f }, point { 90.0f, 50.0f } }, "north")
    );
    const stream calm_stream("/tmp/calm.mp4", "cam-calm", "file", true);
    const stream busy_stream("/tmp/busy.mp4", "cam-busy", "file", true);

    EXPECT_NO_THROW((void)frame_processor(tripwire_stream, dark_frame));
    EXPECT_NO_THROW((void)frame_processor(tripwire_stream, tripwire_frame));
    EXPECT_NO_THROW((void)frame_processor(calm_stream, dark_frame));
    EXPECT_NO_THROW((void)frame_processor(calm_stream, calm_frame));
    EXPECT_NO_THROW((void)frame_processor(busy_stream, dark_frame));
    EXPECT_NO_THROW((void)frame_processor(busy_stream, busy_frame));

    const auto tripwire_result
        = runtime.latest_processing_result("cam-tripwire");
    ASSERT_TRUE(tripwire_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *tripwire_result, "algorithm"
        ),
        "hybrid_auto"
    );
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *tripwire_result, "selected_algorithm"
        ),
        "motion_baseline"
    );

    const auto calm_result = runtime.latest_processing_result("cam-calm");
    ASSERT_TRUE(calm_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *calm_result, "selected_algorithm"
        ),
        "spot_grid"
    );

    const auto busy_result = runtime.latest_processing_result("cam-busy");
    ASSERT_TRUE(busy_result.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *busy_result, "selected_algorithm"
        ),
        "contour_mask"
    );
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            *busy_result, "selection_reason"
        ),
        "busy_scene"
    );
}

TEST(processing_algorithm_registry_test, hybrid_auto_can_force_low_cost_guard) {
    auto algorithm = yodau::core::make_processing_algorithm("auto");
    ASSERT_TRUE(algorithm);

    processing_algorithm_configuration configuration
        = algorithm->default_configuration();
    configuration.values["overload_avg_ms"] = "0";
    algorithm->configure(configuration);

    stream guarded_stream("/tmp/guarded.mp4", "cam-guarded", "file", true);
    guarded_stream.connect_line(
        make_line({ point { 10.0f, 10.0f }, point { 90.0f, 10.0f } }, "north")
    );

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto busy_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(160), 16, 16, 48, 48
    );

    EXPECT_NO_THROW((void)algorithm->process_frame(guarded_stream, dark_frame));
    const processing_result result
        = algorithm->process_frame(guarded_stream, busy_frame);

    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            result, "selected_algorithm"
        ),
        "spot_grid"
    );
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(
            result, "selection_reason"
        ),
        "forced_load_guard"
    );
}

TEST(processing_runtime_test, core_preview_router_publishes_rendered_frame) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::core_only,
            .enable_virtual_camera = true,
            .virtual_camera_device = {},
            .algorithm_id = "spot_grid",
        }
    );

    ASSERT_TRUE(runtime.has_virtual_camera());

    const auto frame_processor = runtime.frame_processor_hook();
    const auto frame_sink = runtime.processed_frame_sink();
    ASSERT_TRUE(static_cast<bool>(frame_processor));
    ASSERT_TRUE(static_cast<bool>(frame_sink));

    stream preview_stream("/tmp/preview.mp4", "cam-preview", "file", true);
    preview_stream.connect_line(
        make_line({ point { 10.0f, 50.0f }, point { 90.0f, 50.0f } }, "guide")
    );

    const auto timestamp = std::chrono::steady_clock::now();
    const auto dark_frame
        = stream_manager_tests_support::make_gray_frame(64, 64, timestamp);
    const auto bright_frame = stream_manager_tests_support::make_gray_frame(
        64, 64, timestamp + std::chrono::milliseconds(200), 18, 18, 42, 42
    );

    EXPECT_TRUE(frame_processor(preview_stream, dark_frame).empty());
    const auto events = frame_processor(preview_stream, bright_frame);
    ASSERT_FALSE(events.empty());

    frame_sink(preview_stream, bright_frame, events);

    const virtual_camera* preview_camera = runtime.preview_camera();
    ASSERT_NE(preview_camera, nullptr);
    const auto published_frame
        = preview_camera->latest_frame(preview_stream.get_name());
    ASSERT_TRUE(published_frame.has_value());
    EXPECT_EQ(published_frame->width, bright_frame.width);
    EXPECT_EQ(published_frame->height, bright_frame.height);
    EXPECT_EQ(published_frame->format, yodau::core::pixel_format::bgr24);
    EXPECT_FALSE(published_frame->data.empty());
    EXPECT_NE(published_frame->data, bright_frame.data);
}

TEST(
    processing_runtime_test,
    motion_baseline_resets_when_stream_frame_size_changes
) {
    processing_runtime runtime(
        processing_runtime_options {
            .mode = render_mode::app_only,
            .enable_virtual_camera = false,
            .virtual_camera_device = {},
            .algorithm_id = "motion_baseline",
        }
    );

    const auto frame_processor = runtime.frame_processor_hook();
    ASSERT_TRUE(static_cast<bool>(frame_processor));

    const auto timestamp = std::chrono::steady_clock::now();
    const auto first_frame
        = stream_manager_tests_support::make_rgb_frame(64, 64, timestamp);
    const auto resized_frame = stream_manager_tests_support::make_rgb_frame(
        48, 32, timestamp + std::chrono::milliseconds(200)
    );
    const auto resumed_frame = stream_manager_tests_support::make_rgb_frame(
        48, 32, timestamp + std::chrono::milliseconds(400), 8, 6, 40, 26
    );

    const stream motion_stream("/tmp/motion.mp4", "cam-motion", "file", true);

    EXPECT_NO_THROW((void)frame_processor(motion_stream, first_frame));
    EXPECT_NO_THROW((void)frame_processor(motion_stream, resized_frame));
    EXPECT_NO_THROW((void)frame_processor(motion_stream, resumed_frame));

    const auto latest = runtime.latest_processing_result("cam-motion");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(
        stream_manager_tests_support::diagnostic_value(*latest, "algorithm"),
        "motion_baseline"
    );
}
#endif

TEST(
    tripwire_grid_stream_index_test,
    recommend_grid_dims_focuses_on_new_small_line
) {
    std::vector<line_ptr> lines {
        make_line({ point { 5.0f, 5.0f }, point { 95.0f, 5.0f } }, "top"),
        make_line({ point { 5.0f, 95.0f }, point { 95.0f, 95.0f } }, "bottom"),
        make_line({ point { 5.0f, 5.0f }, point { 5.0f, 95.0f } }, "left"),
        make_line({ point { 95.0f, 5.0f }, point { 95.0f, 95.0f } }, "right"),
        make_line({ point { 10.0f, 20.0f }, point { 90.0f, 20.0f } }, "mid_a"),
        make_line({ point { 10.0f, 80.0f }, point { 90.0f, 80.0f } }, "mid_b"),
    };

    const line_ptr focus_line = make_line(
        { point { 48.0f, 49.0f }, point { 51.0f, 51.0f } }, "focus"
    );
    lines.push_back(focus_line);

    const grid_dims neutral = recommend_grid_dims(lines);
    const grid_dims focused = recommend_grid_dims(lines, focus_line.get());

    EXPECT_GE(focused.nx, neutral.nx);
    EXPECT_GE(focused.ny, neutral.ny);
    EXPECT_GT(focused.nx * focused.ny, neutral.nx * neutral.ny);
}

TEST(
    tripwire_grid_stream_index_test,
    collect_grid_candidates_stays_local_to_active_cells
) {
    const std::vector<line_ptr> lines {
        make_line({ point { 10.0f, 10.0f }, point { 10.0f, 90.0f } }, "left"),
        make_line({ point { 50.0f, 10.0f }, point { 50.0f, 90.0f } }, "center"),
        make_line({ point { 90.0f, 10.0f }, point { 90.0f, 90.0f } }, "right"),
    };

    const grid_dims dims { 12, 12 };
    const auto index = build_grid_stream_index(lines, dims);

    std::vector<int> active_cell_indices;
    for (const auto& cell : trace_grid_cells_pct(
             point { 7.0f, 15.0f }, point { 14.0f, 85.0f }, dims
         )) {
        active_cell_indices.push_back(grid_index(cell, dims));
    }

    grid_candidate_tracker tracker;
    std::vector<size_t> candidate_segment_ids;
    collect_grid_candidates(
        index, active_cell_indices, tracker, candidate_segment_ids
    );

    ASSERT_FALSE(candidate_segment_ids.empty());
    EXPECT_LT(candidate_segment_ids.size(), index.segments.size());

    std::set<std::string> candidate_line_names;
    for (const size_t segment_id : candidate_segment_ids) {
        ASSERT_LT(segment_id, index.segments.size());
        const auto& ref = index.segments[segment_id];
        ASSERT_LT(ref.line_index, index.lines.size());
        candidate_line_names.insert(index.lines[ref.line_index].name);
    }

    EXPECT_EQ(candidate_line_names, std::set<std::string> { "left" });
}

TEST(
    tripwire_grid_stream_index_test,
    collect_grid_candidates_deduplicates_segment_ids
) {
    const std::vector<line_ptr> lines {
        make_line({ point { 0.0f, 0.0f }, point { 100.0f, 100.0f } }, "diag"),
    };

    const grid_dims dims { 10, 10 };
    const auto index = build_grid_stream_index(lines, dims);

    std::vector<int> active_cell_indices;
    for (const auto& cell : trace_grid_cells_pct(
             point { 0.0f, 0.0f }, point { 100.0f, 100.0f }, dims
         )) {
        active_cell_indices.push_back(grid_index(cell, dims));
    }

    grid_candidate_tracker tracker;
    std::vector<size_t> candidate_segment_ids;
    collect_grid_candidates(
        index, active_cell_indices, tracker, candidate_segment_ids
    );

    ASSERT_EQ(index.segments.size(), 1u);
    ASSERT_EQ(candidate_segment_ids.size(), 1u);
    EXPECT_EQ(candidate_segment_ids.front(), 0u);
}

TEST(stream_manager_test, processed_frame_sink_receives_frame_and_events) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);

    stream_manager_tests_support::processed_frame_probe probe;
    mgr.set_frame_processor(
        &stream_manager_tests_support::processed_frame_probe::process
    );
    mgr.set_processed_frame_sink(
        std::bind_front(
            &stream_manager_tests_support::processed_frame_probe::
                on_processed_frame,
            &probe
        )
    );

    yodau::core::frame frame_value;
    frame_value.width = 320;
    frame_value.height = 180;
    frame_value.stride = 320 * 3;
    frame_value.format = yodau::core::pixel_format::rgb24;
    frame_value.data.resize(static_cast<size_t>(frame_value.stride) * 180u);
    frame_value.ts = std::chrono::steady_clock::now();

    mgr.push_frame("cam0", std::move(frame_value));

    EXPECT_EQ(probe.call_count, 1u);
    EXPECT_EQ(probe.last_stream_name, "cam0");
    EXPECT_EQ(probe.last_frame_width, 320);
    EXPECT_EQ(probe.last_event_count, 1u);
}

TEST(
    stream_manager_test,
    analysis_interval_override_throttles_and_clear_restores_default
) {
    stream_manager mgr;
    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.set_analysis_interval_ms(1000);

    mgr.set_frame_processor(
        &stream_manager_tests_support::processed_frame_probe::process
    );

    yodau::core::frame frame_value;
    frame_value.ts = std::chrono::steady_clock::now();

    EXPECT_EQ(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).size(), 1u
    );
    EXPECT_TRUE(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).empty()
    );

    mgr.set_stream_analysis_interval_ms("cam0", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_EQ(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).size(), 1u
    );

    mgr.clear_stream_analysis_interval_ms("cam0");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(
        mgr.process_frame("cam0", yodau::core::frame(frame_value)).empty()
    );
}

TEST(stream_manager_test, line_profile_is_stored_beside_geometry_line) {
    stream_manager mgr;

    const line_ptr line_value = mgr.add_line("0,0;100,0", false, "north");
    ASSERT_TRUE(line_value);
    EXPECT_EQ(line_value->name, "north");
    EXPECT_EQ(line_value->points.size(), 2u);
    EXPECT_FALSE(line_value->closed);

    const auto default_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(default_profile.has_value());
    EXPECT_EQ(default_profile->line_name, "north");
    EXPECT_FLOAT_EQ(default_profile->visual_width, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->interaction_width, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->effective_length, 1.0f);
    EXPECT_FLOAT_EQ(default_profile->damping, 0.5f);

    mgr.set_line_profile(
        line_profile {
            .line_name = "north",
            .visual_width = 6.0f,
            .interaction_width = 0.0f,
            .effective_length = 2.5f,
            .damping = -1.0f,
        }
    );

    const auto stored_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(stored_profile.has_value());
    EXPECT_EQ(
        *stored_profile, make_line_profile("north", 6.0f, 0.0f, 2.5f, -1.0f)
    );

    EXPECT_EQ(line_value->name, "north");
    EXPECT_EQ(line_value->points.size(), 2u);
    EXPECT_FALSE(line_value->closed);
}

TEST(
    stream_manager_test, stream_profile_override_does_not_mutate_global_profile
) {
    stream_manager mgr;

    mgr.add_stream("/tmp/cam.mp4", "cam0", "file", true);
    mgr.add_line("0,0;100,0", false, "north");
    mgr.set_line_profile(make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f));
    mgr.set_line("cam0", "north");

    const auto inherited_profile
        = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(inherited_profile.has_value());
    EXPECT_EQ(
        *inherited_profile, make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    mgr.set_stream_line_profile(
        "cam0", make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );

    const auto global_profile = mgr.find_line_profile("north");
    ASSERT_TRUE(global_profile.has_value());
    EXPECT_EQ(
        *global_profile, make_line_profile("north", 6.0f, 7.0f, 1.4f, 0.75f)
    );

    const auto stream_profile = mgr.find_stream_line_profile("cam0", "north");
    ASSERT_TRUE(stream_profile.has_value());
    EXPECT_EQ(
        *stream_profile, make_line_profile("north", 2.0f, 2.0f, 0.75f, 0.25f)
    );
}

TEST(stream_manager_test, clear_stream_line_disconnects_only_selected_stream) {
    stream_manager mgr;

    mgr.add_stream("/tmp/cam0.mp4", "cam0", "file", true);
    mgr.add_stream("/tmp/cam1.mp4", "cam1", "file", true);
    mgr.add_line("0,0;100,0", false, "north");
    mgr.set_line("cam0", "north");
    mgr.set_line("cam1", "north");

    EXPECT_EQ(mgr.stream_lines("cam0").size(), 1u);
    EXPECT_EQ(mgr.stream_lines("cam1").size(), 1u);
    ASSERT_TRUE(mgr.find_stream_line_profile("cam0", "north").has_value());
    ASSERT_TRUE(mgr.find_stream_line_profile("cam1", "north").has_value());

    mgr.clear_stream_line("cam0", "north");

    EXPECT_TRUE(mgr.stream_lines("cam0").empty());
    EXPECT_EQ(mgr.stream_lines("cam1").size(), 1u);
    EXPECT_TRUE(mgr.find_stream_line_profile("cam0", "north").has_value());
    EXPECT_TRUE(mgr.find_stream_line_profile("cam1", "north").has_value());
    EXPECT_TRUE(mgr.find_line_profile("north").has_value());
}

TEST(virtual_camera_test, latest_frame_replaces_previous_frame_for_stream) {
    virtual_camera camera("core_cli");

    yodau::core::frame first_frame;
    first_frame.width = 160;
    first_frame.height = 90;
    first_frame.stride = 160 * 3;
    first_frame.format = yodau::core::pixel_format::bgr24;
    first_frame.data.resize(static_cast<size_t>(first_frame.stride) * 90u);

    yodau::core::frame second_frame;
    second_frame.width = 640;
    second_frame.height = 360;
    second_frame.stride = 640 * 3;
    second_frame.format = yodau::core::pixel_format::bgr24;
    second_frame.data.resize(static_cast<size_t>(second_frame.stride) * 360u);

    camera.publish("cam0", first_frame);
    camera.publish("cam0", second_frame);

    const auto latest = camera.latest_frame("cam0");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->width, 640);
    EXPECT_EQ(latest->height, 360);

    const std::vector<virtual_camera_frame_info> frames = camera.frames();
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames.front().stream_name, "cam0");
    EXPECT_EQ(frames.front().update_count, 2u);
    EXPECT_EQ(frames.front().bytes, second_frame.data.size());
}

TEST(virtual_camera_test, release_clears_cached_frame_state_for_stream) {
    virtual_camera camera("core_cli");

    yodau::core::frame frame_value;
    frame_value.width = 320;
    frame_value.height = 180;
    frame_value.stride = 320 * 3;
    frame_value.format = yodau::core::pixel_format::bgr24;
    frame_value.data.resize(static_cast<size_t>(frame_value.stride) * 180u);

    camera.publish("cam0", frame_value);
    ASSERT_TRUE(camera.latest_frame("cam0").has_value());
    ASSERT_EQ(camera.frames().size(), 1u);

    camera.release("cam0");

    EXPECT_FALSE(camera.latest_frame("cam0").has_value());
    EXPECT_TRUE(camera.frames().empty());
}

TEST(frame_layout_test, accepts_each_format_with_padded_rows) {
    using yodau::core::frame;
    using yodau::core::pixel_format;
    using yodau::core::validate_frame_layout;

    const std::vector<std::pair<pixel_format, size_t>> formats {
        { pixel_format::gray8, 1U },  { pixel_format::rgb24, 3U },
        { pixel_format::bgr24, 3U },  { pixel_format::rgba32, 4U },
        { pixel_format::bgra32, 4U },
    };

    for (const auto& [format, pixel_size] : formats) {
        frame value;
        value.width = 3;
        value.height = 2;
        value.stride = static_cast<int>(3U * pixel_size + 5U);
        value.format = format;
        const size_t required
            = static_cast<size_t>(value.stride) + 3U * pixel_size;
        value.data.resize(required);

        const auto validation = validate_frame_layout(value);
        EXPECT_TRUE(validation.valid());
        EXPECT_EQ(validation.bytes_per_pixel, pixel_size);
        EXPECT_EQ(validation.row_bytes, 3U * pixel_size);
        EXPECT_EQ(validation.required_bytes, required);
    }
}

TEST(frame_layout_test, rejects_small_stride_truncation_and_unknown_format) {
    using yodau::core::frame;
    using yodau::core::frame_layout_error;
    using yodau::core::pixel_format;
    using yodau::core::validate_frame_layout;

    frame value;
    value.width = 4;
    value.height = 2;
    value.stride = 11;
    value.format = pixel_format::rgb24;
    value.data.resize(24U);
    EXPECT_EQ(
        validate_frame_layout(value).error, frame_layout_error::stride_too_small
    );

    value.stride = 16;
    value.data.resize(27U);
    EXPECT_EQ(
        validate_frame_layout(value).error, frame_layout_error::data_too_small
    );

    value.format = static_cast<pixel_format>(255);
    EXPECT_EQ(
        validate_frame_layout(value).error,
        frame_layout_error::unsupported_pixel_format
    );
}

TEST(frame_layout_test, rejects_huge_layout_without_allocating_its_extent) {
    using yodau::core::frame;
    using yodau::core::frame_layout_error;
    using yodau::core::pixel_format;
    using yodau::core::validate_frame_layout;

    frame value;
    value.width = std::numeric_limits<int>::max();
    value.height = std::numeric_limits<int>::max();
    value.stride = std::numeric_limits<int>::max();
    value.format = pixel_format::gray8;

    const auto error = validate_frame_layout(value).error;
    EXPECT_TRUE(
        error == frame_layout_error::extent_overflow
        || error == frame_layout_error::data_too_small
    );
}

TEST(geometry_validation_test, accepts_boundary_open_and_closed_lines) {
    EXPECT_NO_THROW(
        yodau::core::validate_line_geometry(
            std::vector<point> { point { 0.0f, 0.0f },
                                 point { 100.0f, 100.0f } },
            "boundary", false
        )
    );
    EXPECT_NO_THROW(
        yodau::core::validate_line_geometry(
            std::vector<point> {
                point { 0.0f, 0.0f },
                point { 100.0f, 0.0f },
                point { 0.0f, 100.0f },
            },
            "region", true
        )
    );
}

TEST(geometry_validation_test, reports_invalid_coordinate_and_shape_fields) {
    try {
        (void)yodau::core::parse_points("10,20;101,40");
        FAIL() << "out-of-range coordinate was accepted";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(
            std::string(error.what()).find("point 2 x"), std::string::npos
        );
        EXPECT_NE(
            std::string(error.what()).find("inclusive range [0, 100]"),
            std::string::npos
        );
    }

    EXPECT_THROW(
        yodau::core::validate_line_geometry(
            std::vector<point> { point { 1.0f, 1.0f } }, "open", false
        ),
        std::invalid_argument
    );
    EXPECT_THROW(
        yodau::core::validate_line_geometry(
            std::vector<point> {
                point { 0.0f, 0.0f },
                point { 50.0f, 50.0f },
                point { 100.0f, 100.0f },
            },
            "flat-region", true
        ),
        std::invalid_argument
    );
    EXPECT_THROW(
        yodau::core::validate_line_geometry(
            std::vector<point> { point { 0.0f, 0.0f }, point { 1.0f, 1.0f } },
            " \t", false
        ),
        std::invalid_argument
    );
}

TEST(geometry_validation_test, permits_drafts_but_rejects_invalid_attachment) {
    const line_ptr draft = make_line({ point { 25.0f, 25.0f } }, "draft");
    ASSERT_NE(draft, nullptr);
    ASSERT_EQ(draft->points.size(), 1U);

    stream target("/tmp/cam.mp4", "cam", "file", true);
    EXPECT_THROW(target.connect_line(draft), std::invalid_argument);
}

TEST(geometry_validation_test, stream_store_enforces_processing_invariants) {
    stream_manager manager;

    EXPECT_THROW(
        (void)manager.add_line("10,10", false, "one-point"),
        std::invalid_argument
    );
    EXPECT_THROW(
        (void)manager.add_line("10,10;90,90", true, "two-point-region"),
        std::invalid_argument
    );
    EXPECT_THROW(
        (void)manager.add_line("0,0;120,20", false, "outside"),
        std::invalid_argument
    );

    EXPECT_NE(manager.add_line("0,0;100,100", false, "valid"), nullptr);
}

TEST(stream_test, pipeline_state_supports_concurrent_status_reads) {
    stream value("/tmp/cam.mp4", "cam", "file", true);
    std::atomic<bool> invalid_value_seen { false };

    std::jthread writer([&value] {
        for (int index = 0; index < 20000; ++index) {
            value.activate(
                index % 2 == 0 ? yodau::core::stream_pipeline::manual
                               : yodau::core::stream_pipeline::automatic
            );
        }
        value.deactivate();
    });
    std::jthread reader([&value, &invalid_value_seen] {
        for (int index = 0; index < 20000; ++index) {
            const auto pipeline = value.pipeline();
            if (pipeline != yodau::core::stream_pipeline::manual
                && pipeline != yodau::core::stream_pipeline::automatic
                && pipeline != yodau::core::stream_pipeline::none) {
                invalid_value_seen.store(true);
            }
        }
    });

    writer.join();
    reader.join();
    EXPECT_FALSE(invalid_value_seen.load());
    EXPECT_EQ(value.pipeline(), yodau::core::stream_pipeline::none);
}

#ifdef YODAU_OPENCV

TEST(opencv_frame_adapter_test, rejects_invalid_layout_before_mat_view) {
    yodau::core::frame truncated;
    truncated.width = 4;
    truncated.height = 2;
    truncated.stride = 12;
    truncated.format = yodau::core::pixel_format::bgr24;
    truncated.data.resize(23U);

    cv::Mat bgr;
    cv::Mat gray;
    EXPECT_TRUE(yodau::core::frame_to_gray_mat(truncated).empty());
    EXPECT_FALSE(yodau::core::frame_to_bgr_gray_mats(truncated, bgr, gray));
    EXPECT_TRUE(bgr.empty());
    EXPECT_TRUE(gray.empty());

    truncated.stride = 11;
    truncated.data.resize(24U);
    EXPECT_TRUE(yodau::core::frame_to_gray_mat(truncated).empty());
}

TEST(opencv_frame_adapter_test, accepts_padded_rows) {
    yodau::core::frame padded;
    padded.width = 2;
    padded.height = 2;
    padded.stride = 8;
    padded.format = yodau::core::pixel_format::rgb24;
    padded.data.resize(14U, 127U);

    cv::Mat bgr;
    cv::Mat gray;
    ASSERT_TRUE(yodau::core::frame_to_bgr_gray_mats(padded, bgr, gray));
    EXPECT_EQ(bgr.rows, 2);
    EXPECT_EQ(bgr.cols, 2);
    EXPECT_EQ(gray.rows, 2);
    EXPECT_EQ(gray.cols, 2);
}

TEST(opencv_frame_adapter_test, converts_only_documented_8_bit_channels) {
    const auto timestamp = std::chrono::steady_clock::now();
    for (const int type : { CV_8UC1, CV_8UC3, CV_8UC4 }) {
        const cv::Mat input(3, 4, type, cv::Scalar::all(19));
        const auto converted = yodau::core::bgr_mat_to_frame(input, timestamp);
        EXPECT_EQ(converted.width, 4);
        EXPECT_EQ(converted.height, 3);
        EXPECT_EQ(converted.stride, 12);
        EXPECT_EQ(converted.format, yodau::core::pixel_format::bgr24);
        EXPECT_TRUE(yodau::core::validate_frame_layout(converted));
    }

    EXPECT_THROW(
        (void)yodau::core::bgr_mat_to_frame(cv::Mat(2, 2, CV_8UC2), timestamp),
        std::invalid_argument
    );
    EXPECT_THROW(
        (void)yodau::core::bgr_mat_to_frame(cv::Mat(2, 2, CV_16UC1), timestamp),
        std::invalid_argument
    );
}

TEST(processing_motion_focus_test, automatic_preserves_detection_context) {
    stream source("/tmp/focus.mp4", "focus", "file", true);
    source.connect_line(
        make_line({ point { 50.0f, 5.0f }, point { 50.0f, 95.0f } }, "tripwire")
    );
    source.connect_line(make_line(
        {
            point { 5.0f, 5.0f },
            point { 35.0f, 5.0f },
            point { 35.0f, 35.0f },
            point { 5.0f, 35.0f },
        },
        "region", true
    ));

    const auto automatic = yodau::core::build_motion_focus_mask(
        source, cv::Size(64, 64),
        { .mode = yodau::core::processing_motion_focus_mode::auto_focus }
    );
    EXPECT_TRUE(automatic.mask.empty());
    EXPECT_EQ(automatic.shape_count, 0U);

    const auto regions = yodau::core::build_motion_focus_mask(
        source, cv::Size(64, 64),
        { .mode = yodau::core::processing_motion_focus_mode::regions }
    );
    EXPECT_FALSE(regions.mask.empty());
    EXPECT_EQ(regions.shape_count, 1U);

    const auto corridors = yodau::core::build_motion_focus_mask(
        source, cv::Size(64, 64),
        { .mode = yodau::core::processing_motion_focus_mode::corridors }
    );
    EXPECT_FALSE(corridors.mask.empty());
    EXPECT_EQ(corridors.shape_count, 1U);
}

TEST(video_capture_test, looping_unopened_source_finishes_after_one_rewind) {
    const stream source("/tmp/does-not-exist.mp4", "missing", "file", true);
    cv::VideoCapture capture;
    cv::Mat image;

    EXPECT_EQ(
        yodau::core::read_video_capture_frame(source, capture, image),
        yodau::core::video_capture_read_status::finished
    );
}

TEST(video_capture_test, daemon_reports_capture_open_failure_without_path) {
    const stream source(
        "/tmp/private-capture-marker.mp4", "missing", "file", false
    );
    std::stop_source stop_source;

    try {
        yodau::core::opencv_client::daemon_start(
            source, [](yodau::core::frame&&) { }, stop_source.get_token()
        );
        FAIL() << "expected the unavailable capture source to fail";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("could not be opened"), std::string::npos);
        EXPECT_NE(message.find("file"), std::string::npos);
        EXPECT_EQ(message.find("private-capture-marker"), std::string::npos);
    }
}

TEST(opencv_grid_cache_test, snapshots_survive_parallel_stream_insertions) {
    constexpr int worker_count = 12;
    yodau::core::opencv_client client;
    std::barrier start_final_frame(worker_count);
    std::atomic<bool> failed { false };
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);

    for (int index = 0; index < worker_count; ++index) {
        workers.emplace_back([&, index] {
            bool reached_barrier = false;
            try {
                stream source(
                    "/tmp/cache.mp4", "cache-" + std::to_string(index), "file",
                    true
                );
                source.connect_line(make_line(
                    { point { 50.0f, 5.0f }, point { 50.0f, 95.0f } }, "center"
                ));

                const auto timestamp = std::chrono::steady_clock::now();
                const auto dark = stream_manager_tests_support::make_gray_frame(
                    96, 96, timestamp
                );
                const auto bright
                    = stream_manager_tests_support::make_gray_frame(
                        96, 96, timestamp + std::chrono::milliseconds(200), 16,
                        24, 48, 72
                    );
                (void)client.motion_processor(source, dark);
                (void)client.motion_processor(source, bright);

                std::this_thread::sleep_for(std::chrono::milliseconds(170));
                start_final_frame.arrive_and_wait();
                reached_barrier = true;
                (void)client.motion_processor(source, dark);
            } catch (...) {
                if (!reached_barrier) {
                    start_final_frame.arrive_and_drop();
                }
                failed.store(true);
            }
        });
    }

    workers.clear();
    EXPECT_FALSE(failed.load());
}

#endif
