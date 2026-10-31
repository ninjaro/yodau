#ifndef YODAU_CORE_APPLE_CAPTURE_DEVICE_HPP
#define YODAU_CORE_APPLE_CAPTURE_DEVICE_HPP

#include "concurrency/stoppable_thread.hpp"
#include "streams/frame.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yodau::core {

class stream;

// Explicit source form for AVFoundation cameras. A bare unique ID is accepted
// too: it is the camera ID Qt Multimedia reports on Apple platforms.
inline constexpr std::string_view apple_capture_path_prefix = "avfoundation:";

enum class apple_capture_authorization {
    unavailable,
    not_determined,
    restricted,
    denied,
    authorized,
};

struct apple_capture_format {
    int width { 0 };
    int height { 0 };
    double min_fps { 0.0 };
    double max_fps { 0.0 };
    std::string pixel_format;
};

struct apple_capture_device_info {
    std::string unique_id;
    std::string name;
    std::string model_id;
    std::string device_type;
    bool connected { false };
    bool suspended { false };
    bool in_use_by_other_application { false };
    std::vector<apple_capture_format> formats;
};

struct apple_capture_options {
    // Zero keeps the session's default format. Otherwise the device format
    // closest to this size (by pixel count, then aspect) is selected.
    int preferred_width { 0 };
    int preferred_height { 0 };
    std::chrono::milliseconds first_frame_timeout { std::chrono::seconds(5) };
    std::chrono::milliseconds authorization_timeout {
        std::chrono::seconds(60)
    };
};

[[nodiscard]] std::string_view
apple_capture_authorization_name(apple_capture_authorization status) noexcept;

// True when this build contains the AVFoundation backend. Canonical builds
// currently return false on every platform; native integration is deferred.
[[nodiscard]] bool apple_capture_available() noexcept;

[[nodiscard]] std::optional<std::string>
apple_capture_device_id_from_path(std::string_view path);
[[nodiscard]] std::string
apple_capture_path_for_device_id(std::string_view unique_id);
// Lower-case, dash-separated stream name derived from a device name, unique
// among `taken`.
[[nodiscard]] std::string apple_capture_stream_name(
    std::string_view device_name, const std::vector<std::string>& taken
);

// Resolves a configured local source to a connected device's unique ID:
// `avfoundation:<id>`, a bare unique ID, or `/dev/videoN` as the Nth
// discovered device (the index form OpenCV used on Apple).
[[nodiscard]] std::optional<std::string> resolve_apple_capture_device(
    std::string_view path, const std::vector<apple_capture_device_info>& devices
);
[[nodiscard]] std::optional<std::string>
resolve_apple_capture_device(std::string_view path);

[[nodiscard]] apple_capture_authorization apple_capture_authorization_status();
// Shows the system prompt when the status is undetermined. Returns the status
// after the user answered or `timeout` elapsed.
apple_capture_authorization
request_apple_capture_authorization(std::chrono::milliseconds timeout);

// Discovery needs no camera permission.
[[nodiscard]] std::vector<apple_capture_device_info>
list_apple_capture_devices();

// True for local streams on builds with the AVFoundation backend; such
// streams are captured by run_apple_capture instead of an algorithm client.
[[nodiscard]] bool uses_apple_capture(const stream& stream_value);

// Blocks until `token` requests a stop, delivering bgra32 frames. Only the
// newest undelivered frame is kept, so a slow consumer drops frames instead of
// queueing them. Throws std::runtime_error when the camera cannot be opened,
// access is not granted, no frame arrives in time, or the device disconnects.
void run_apple_capture(
    const std::string& path, const std::function<void(frame&&)>& on_frame,
    const stop_token& token, const apple_capture_options& options = {}
);

} // namespace yodau::core

#endif // YODAU_CORE_APPLE_CAPTURE_DEVICE_HPP
