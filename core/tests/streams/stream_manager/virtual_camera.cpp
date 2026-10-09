#include "streams/virtual_camera.hpp"
#include "streams/frame.hpp"

#include <gtest/gtest.h>
#include <vector>

using yodau::core::virtual_camera;
using yodau::core::virtual_camera_frame_info;

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
