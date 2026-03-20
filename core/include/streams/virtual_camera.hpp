#ifndef YODAU_BACKEND_STREAMS_VIRTUAL_CAMERA_HPP
#define YODAU_BACKEND_STREAMS_VIRTUAL_CAMERA_HPP

#include "streams/frame.hpp"

#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace yodau::backend {

struct virtual_camera_frame_info {
    std::string stream_name;
    int width { 0 };
    int height { 0 };
    pixel_format format { pixel_format::bgr24 };
    size_t bytes { 0 };
    size_t update_count { 0 };
};

class virtual_camera {
public:
    explicit virtual_camera(std::string camera_name = "backend_preview");

    void publish(const std::string& stream_name, const frame& frame_value);
    std::optional<frame> latest_frame(const std::string& stream_name) const;
    std::vector<virtual_camera_frame_info> frames() const;
    void dump(std::ostream& out) const;

private:
    std::string camera_name;
    std::unordered_map<std::string, frame> latest_by_stream;
    std::unordered_map<std::string, size_t> update_count_by_stream;
    mutable std::mutex mtx;
};

}

#endif // YODAU_BACKEND_STREAMS_VIRTUAL_CAMERA_HPP
