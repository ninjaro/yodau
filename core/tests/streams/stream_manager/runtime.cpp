#include "analysis/processing_runtime.hpp"
#include "frames.hpp"
#include "streams/stream.hpp"
#include "streams/virtual_camera.hpp"

#include <chrono>
#include <gtest/gtest.h>

using yodau::core::make_line;
using yodau::core::point;
using yodau::core::processing_runtime;
using yodau::core::processing_runtime_options;
using yodau::core::render_mode;
using yodau::core::stream;
using yodau::core::virtual_camera;

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
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
#endif
