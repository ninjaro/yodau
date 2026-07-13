#ifndef YODAU_CORE_STREAMS_VIRTUAL_CAMERA_HPP
#define YODAU_CORE_STREAMS_VIRTUAL_CAMERA_HPP

#include "core/namespace_alias.hpp"
#include "streams/frame.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace yodau::core {

struct virtual_camera_frame_info {
    std::string stream_name;
    int width { 0 };
    int height { 0 };
    pixel_format format { pixel_format::bgr24 };
    size_t bytes { 0 };
    size_t update_count { 0 };
    size_t published_frame_count { 0 };
    size_t dropped_frame_count { 0 };
    std::string device_path;
    bool device_ready { false };
    std::string last_error;
};

class virtual_camera {
public:
    explicit virtual_camera(
        std::string camera_name = "yodau", std::string device_path = {}
    );
    ~virtual_camera();

    virtual_camera(const virtual_camera&) = delete;
    virtual_camera& operator=(const virtual_camera&) = delete;

    void publish(const std::string& stream_name, const frame& frame_value);
    void release(const std::string& stream_name);
    std::optional<frame> latest_frame(const std::string& stream_name) const;
    std::vector<virtual_camera_frame_info> frames() const;
    void dump(std::ostream& out) const;

private:
    struct sink_binding {
        mutable std::mutex mtx;
        std::condition_variable_any wake;
        std::shared_ptr<const frame> pending_frame;
        std::jthread worker;
        std::string device_path;
        bool device_ready { false };
        std::string last_error;
        size_t published_frame_count { 0 };
        size_t dropped_frame_count { 0 };
#ifdef __linux__
        int fd { -1 };
        int width { 0 };
        int height { 0 };
        size_t bytes_per_line { 0 };
        size_t bytes_per_frame { 0 };
#endif
    };

    std::shared_ptr<sink_binding>
    ensure_binding_locked(const std::string& stream_name);
    static void run_sink(
        const std::shared_ptr<sink_binding>& binding,
        const std::stop_token& stop_token
    );
    static void stop_sink(const std::shared_ptr<sink_binding>& binding);
    static void close_sink(sink_binding& binding);

    std::string camera_name;
    std::string requested_device_path;
    std::unordered_map<std::string, std::shared_ptr<const frame>>
        latest_by_stream;
    std::unordered_map<std::string, size_t> update_count_by_stream;
    std::unordered_map<std::string, std::shared_ptr<sink_binding>>
        sink_by_stream;
    mutable std::mutex mtx;
};

}

#endif // YODAU_CORE_STREAMS_VIRTUAL_CAMERA_HPP
