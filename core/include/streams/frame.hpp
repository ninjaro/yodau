#ifndef YODAU_CORE_FRAME_HPP
#define YODAU_CORE_FRAME_HPP
#include "core/namespace_alias.hpp"
#include <chrono>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace yodau::core {
enum class pixel_format { gray8, rgb24, bgr24, rgba32, bgra32 };

struct frame {
    int width { 0 };
    int height { 0 };
    int stride { 0 };
    pixel_format format { pixel_format::bgr24 };
    std::vector<std::uint8_t> data;
    std::chrono::steady_clock::time_point ts;
};

enum class frame_layout_error {
    none,
    non_positive_width,
    non_positive_height,
    non_positive_stride,
    unsupported_pixel_format,
    row_size_overflow,
    stride_too_small,
    extent_overflow,
    data_too_small,
};

struct frame_layout_validation {
    frame_layout_error error { frame_layout_error::none };
    std::size_t bytes_per_pixel { 0 };
    std::size_t row_bytes { 0 };
    std::size_t required_bytes { 0 };

    [[nodiscard]] constexpr bool valid() const noexcept {
        return error == frame_layout_error::none;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return valid();
    }
};

[[nodiscard]] constexpr std::string_view
frame_layout_error_message(const frame_layout_error error) noexcept {
    switch (error) {
    case frame_layout_error::none:
        return "valid frame layout";
    case frame_layout_error::non_positive_width:
        return "frame width must be positive";
    case frame_layout_error::non_positive_height:
        return "frame height must be positive";
    case frame_layout_error::non_positive_stride:
        return "frame stride must be positive";
    case frame_layout_error::unsupported_pixel_format:
        return "frame pixel format is unsupported";
    case frame_layout_error::row_size_overflow:
        return "frame row size overflows the addressable range";
    case frame_layout_error::stride_too_small:
        return "frame stride is smaller than one pixel row";
    case frame_layout_error::extent_overflow:
        return "frame extent overflows the addressable range";
    case frame_layout_error::data_too_small:
        return "frame data is smaller than the declared layout";
    }

    return "unknown frame layout error";
}

[[nodiscard]] constexpr std::size_t
bytes_per_pixel(const pixel_format format) noexcept {
    switch (format) {
    case pixel_format::gray8:
        return 1;
    case pixel_format::rgb24:
    case pixel_format::bgr24:
        return 3;
    case pixel_format::rgba32:
    case pixel_format::bgra32:
        return 4;
    }

    return 0;
}

// Extra row padding and trailing bytes are valid. The required extent reaches
// only through the last declared pixel, not through hypothetical padding after
// the final row.
[[nodiscard]] inline frame_layout_validation
validate_frame_layout(const frame& frame_value) noexcept {
    frame_layout_validation result;

    if (frame_value.width <= 0) {
        result.error = frame_layout_error::non_positive_width;
        return result;
    }
    if (frame_value.height <= 0) {
        result.error = frame_layout_error::non_positive_height;
        return result;
    }
    if (frame_value.stride <= 0) {
        result.error = frame_layout_error::non_positive_stride;
        return result;
    }

    result.bytes_per_pixel = bytes_per_pixel(frame_value.format);
    if (result.bytes_per_pixel == 0) {
        result.error = frame_layout_error::unsupported_pixel_format;
        return result;
    }

    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    const auto width = static_cast<std::size_t>(frame_value.width);
    if (width > max_size / result.bytes_per_pixel) {
        result.error = frame_layout_error::row_size_overflow;
        return result;
    }
    result.row_bytes = width * result.bytes_per_pixel;

    const auto stride = static_cast<std::size_t>(frame_value.stride);
    if (stride < result.row_bytes) {
        result.error = frame_layout_error::stride_too_small;
        return result;
    }

    const auto preceding_rows
        = static_cast<std::size_t>(frame_value.height - 1);
    if (preceding_rows > (max_size - result.row_bytes) / stride) {
        result.error = frame_layout_error::extent_overflow;
        return result;
    }
    result.required_bytes = preceding_rows * stride + result.row_bytes;

    if (frame_value.data.size() < result.required_bytes) {
        result.error = frame_layout_error::data_too_small;
        return result;
    }

    return result;
}
}
#endif // YODAU_CORE_FRAME_HPP
