#include "geometry/geometry.hpp"
#include "streams/frame.hpp"
#include "streams/stream.hpp"
#include "streams/stream_manager.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using yodau::core::line_profile;
using yodau::core::line_ptr;
using yodau::core::make_line;
using yodau::core::make_line_profile;
using yodau::core::point;
using yodau::core::stream;
using yodau::core::stream_manager;

TEST(line_profile_test, normalizes_semantic_fields) {
    const line_profile profile
        = make_line_profile("north", 4.5f, 0.0f, 0.0f, 2.0f);

    EXPECT_EQ(profile.line_name, "north");
    EXPECT_FLOAT_EQ(profile.visual_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.interaction_width, 4.5f);
    EXPECT_FLOAT_EQ(profile.effective_length, 1.0f);
    EXPECT_FLOAT_EQ(profile.damping, 1.0f);
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
