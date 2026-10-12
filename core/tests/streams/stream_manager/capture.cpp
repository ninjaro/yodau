#include "analysis/opencv_client.hpp"
#include "analysis/processing_frame_tools.hpp"
#include "analysis/processing_motion_focus.hpp"
#include "geometry/geometry.hpp"
#include "streams/stream.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>
#include <stop_token>
#include <string>

#ifdef YODAU_OPENCV
#include <opencv2/videoio.hpp>
#endif

using yodau::core::make_line;
using yodau::core::point;
using yodau::core::stream;

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
#endif

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
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
#endif

#ifdef YODAU_OPENCV
TEST(video_capture_test, looping_unopened_source_finishes_after_one_rewind) {
    const stream source("/tmp/does-not-exist.mp4", "missing", "file", true);
    cv::VideoCapture capture;
    cv::Mat image;

    EXPECT_EQ(
        yodau::core::read_video_capture_frame(source, capture, image),
        yodau::core::video_capture_read_status::finished
    );
}
#endif

#ifdef YODAU_OPENCV
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
#endif
