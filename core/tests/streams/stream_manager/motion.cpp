#include "analysis/default_processing_algorithms.hpp"
#include "analysis/processing_runtime.hpp"
#include "frames.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>

using yodau::core::make_line;
using yodau::core::point;
using yodau::core::processing_result;
using yodau::core::processing_runtime;
using yodau::core::processing_runtime_options;
using yodau::core::render_mode;
using yodau::core::stream;

TEST(portable_motion_baseline_test, portable_motion_and_tripwire_crossings) {
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
TEST(processing_runtime_test, resets_on_frame_resize) {
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
