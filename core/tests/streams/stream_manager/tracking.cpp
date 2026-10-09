#include "analysis/processing_runtime.hpp"
#include "frames.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>

using yodau::core::make_line;
using yodau::core::point;
using yodau::core::processing_runtime;
using yodau::core::processing_runtime_options;
using yodau::core::render_mode;
using yodau::core::stream;

#ifdef YODAU_OPENCV
TEST(processing_runtime_test, persistent_tracks_emit_motion) {
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
#endif

#ifdef YODAU_OPENCV
TEST(processing_runtime_test, regions_filter_motion_and_match_roi) {
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
#endif
