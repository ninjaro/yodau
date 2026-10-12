#include "analysis/default_processing_hooks.hpp"
#include "analysis/processing_runtime.hpp"
#include "frames.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <string>

using yodau::core::make_line;
using yodau::core::point;
using yodau::core::processing_algorithm_configuration;
using yodau::core::processing_result;
using yodau::core::processing_runtime;
using yodau::core::processing_runtime_options;
using yodau::core::render_mode;
using yodau::core::stream;

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
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
#endif
