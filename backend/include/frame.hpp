#ifndef YODAU_BACKEND_FRAME_HPP
#define YODAU_BACKEND_FRAME_HPP
#include <chrono>
#include <cstdint>
#include <vector>

namespace yodau::backend {
enum class pixel_format { gray8, rgb24, bgr24, rgba32, bgra32 };

struct frame {
    int width { 0 };
    int height { 0 };
    int stride { 0 };
    pixel_format format { pixel_format::bgr24 };
    std::vector<std::uint8_t> data;
    std::chrono::steady_clock::time_point ts;
};
}
#endif // YODAU_BACKEND_FRAME_HPP