#ifndef YODAU_BACKEND_FRAME_HPP
#define YODAU_BACKEND_FRAME_HPP

#include <chrono>
#include <cstdint>
#include <vector>

namespace yodau::backend {

/**
 * @brief Pixel format of a @ref frame buffer.
 *
 * The enumerators describe the byte layout of pixels in @ref frame::data.
 * All formats are tightly packed without per-pixel padding.
 */
enum class pixel_format {
    /** 8-bit grayscale, 1 byte per pixel. */
    gray8,
    /** RGB, 8-bit per channel, 3 bytes per pixel. */
    rgb24,
    /** BGR, 8-bit per channel, 3 bytes per pixel. */
    bgr24,
    /** RGBA, 8-bit per channel, 4 bytes per pixel. */
    rgba32,
    /** BGRA, 8-bit per channel, 4 bytes per pixel. */
    bgra32
};

/**
 * @brief Video frame container.
 *
 * A frame holds raw pixel data and basic metadata. The buffer is stored in
 * row-major order in @ref data.
 *
 * Typical size relation:
 * @code
 * data.size() >= stride * height
 * @endcode
 *
 * where @ref stride is the number of bytes between two consecutive rows.
 *
 * @note The timestamp uses std::chrono::steady_clock and is monotonic.
 */
struct frame {
    /**
     * @brief Frame width in pixels.
     */
    int width { 0 };

    /**
     * @brief Frame height in pixels.
     */
    int height { 0 };

    /**
     * @brief Number of bytes per row.
     *
     * This may be wider than width * bytes_per_pixel due to alignment/padding.
     */
    int stride { 0 };

    /**
     * @brief Pixel format of the buffer.
     *
     * Defaults to @ref pixel_format::bgr24.
     */
    pixel_format format { pixel_format::bgr24 };

    /**
     * @brief Raw pixel bytes.
     *
     * Layout is determined by @ref format and @ref stride.
     */
    std::vector<std::uint8_t> data;

    /**
     * @brief Monotonic timestamp when the frame was captured/produced.
     */
    std::chrono::steady_clock::time_point ts;
};

} // namespace yodau::backend

#endif // YODAU_BACKEND_FRAME_HPP